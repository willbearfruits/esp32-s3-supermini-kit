#include "looper.h"
#include "dsp.h"
#include "scale.h"
#include "synth.h"
#include "kit.h"
#include "tune.h"
#include "mix.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "sdkconfig.h"

static const char *TAG = "looper";

#define MS(x)        ((int)((x) * 0.001f * FS))
#define CLASSIFY_S   MS(8)
#define REFRACT_S    MS(100)
#define HIT_LAG      MS(5)
#define NOTE_LAG     MS(30)
#define ONSET_MAX_S  MS(80)
#define BLK          64

static const char *KIND_NAMES[K_N] = { "DRUMS", "BASS", "KEYS", "LEAD", "VOCAL" };
static const int KIND_SYNTH[K_N] = { -1, SK_BASS, SK_KEYS, SK_LEAD, -1 };

typedef struct {
    uint8_t kind, sound;
    bool mute, solo;
    int vol, rev, dly, lowcut, tone, pan;      // 0..100, 0..100, 0..100, 0..8 (x50 Hz), -100..100, -100..100
    int octave, fx, fx_amt;                    // -2..2, FX_*, 0..100
    bool has[PAT_N];
    int8_t vslot[PAT_N];                        // vocal slot per pattern
    synth_t pb, live;
    drums_t drums;
    int live_note;
} track_t;

static struct {
    bool  ready;
    int   n_tracks, cur, scene, scene_next;
    bool  armed, rec, replace_next, song_mode;
    float bpm, step_len;
    int   bars, steps, loop_len, pos, last_step;
    int   swing, human, kit, count_in, quant8, key, mic_gain, gate_db, stability;
    uint8_t fresh[LOOP_MAX_STEPS];             // steps written during the running take (bit per drum, bit 7 melodic)
    int   post_roll;                           // samples after the loop start that still belong to the finished take
    int   early_type, early_vel;               // a hit played just before the take starts
    bool  metro;
    uint8_t arr[ARR_N]; int arr_rep, arr_pos, arr_loops;

    track_t tr[TRACK_N];
    uint8_t drum[TRACK_N][PAT_N][LOOP_MAX_STEPS][DRUM_N];
    uint8_t seq[TRACK_N][PAT_N][LOOP_MAX_STEPS];

    uint8_t *vpool[VOCAL_SLOTS];
    int8_t   vowner[VOCAL_SLOTS][2];            // track, pat or -1
    int      vcap;

    int     bk_track, bk_pat; bool bk_has;
    uint8_t bk_drum[LOOP_MAX_STEPS][DRUM_N], bk_seq[LOOP_MAX_STEPS];
    uint8_t *bk_vocal;

    scale_t scale;
    float   pc_w[12];
    float   expr_cut, expr_bend, expr_vib, expr_space;

    float bg_db, cap_db;
    int   refr, cap_len, cap_pos;
    bool  capturing;
    drum_bands_t cap;
    struct { int left; uint8_t track, type, vel; } pend[8];
    uint32_t rnd;

    float click_amp, click_ph, click_hz, click_c;
    void (*clock_cb)(void);
    int   clock_next;

    int      bounce_first;
    bool     bounce_on, bounce_running, bounce_done;
    int16_t *bounce[3];
    int      bounce_pos;

    volatile int act_q[8], act_n;
    volatile int dirty, vdirty;
    char  msg[24];
    int   msg_samples;
    int   last_drum;
    float drum_lo, drum_hi, peak;
    float tbuf[TRACK_N][BLK], live_out[BLK];
    uint32_t prof[4];
} S;
uint32_t *looper_prof(void) { return S.prof; }

// ---- mu-law
static inline uint8_t mulaw_enc(float x)
{
    int s = (int)(clampf(x, -1.0f, 1.0f) * 32767.0f);
    int sign = s < 0 ? 0x80 : 0;
    if (s < 0) s = -s;
    s += 132;
    if (s > 32767) s = 32767;
    int exp = 7;
    for (int m = 0x4000; (s & m) == 0 && exp > 0; m >>= 1) exp--;
    int mant = (s >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | mant);
}
static inline float mulaw_dec(uint8_t u)
{
    u = ~u;
    int exp = (u >> 4) & 7, mant = u & 0x0F;
    int s = ((mant << 3) + 132) << exp;
    s -= 132;
    return (float)(u & 0x80 ? -s : s) / 32767.0f;
}

static void say(const char *m) { snprintf(S.msg, sizeof S.msg, "%s", m); S.msg_samples = MS(2500); }
static void mark(int bits) { S.dirty |= bits; }
static void mark_vocal(int t, int p) { int s = -1; for (int i = 0; i < VOCAL_SLOTS; i++) if (S.vowner[i][0] == t && S.vowner[i][1] == p) s = i; if (s >= 0) S.vdirty |= 1 << s; S.dirty |= DIRTY_VOCAL; }
static inline track_t *cur(void) { return &S.tr[S.cur]; }
static inline int pat_of(int t, int scene) { return S.tr[t].has[scene] ? scene : 0; }   // empty pattern falls back to A
static inline int rec_pat(void) { return S.scene; }

// ---- vocal pool
static int vslot_find(int t, int p)
{
    for (int i = 0; i < VOCAL_SLOTS; i++) if (S.vowner[i][0] == t && S.vowner[i][1] == p) return i;
    return -1;
}
static int vslot_alloc(int t, int p)
{
    int s = vslot_find(t, p);
    if (s >= 0) return s;
    for (int i = 0; i < VOCAL_SLOTS; i++) if (S.vowner[i][0] < 0 && S.vpool[i]) { S.vowner[i][0] = t; S.vowner[i][1] = p; memset(S.vpool[i], 0xFF, S.vcap); return i; }
    return -1;
}
static void vslot_free(int t, int p)
{
    int s = vslot_find(t, p);
    if (s >= 0) { S.vowner[s][0] = -1; S.vowner[s][1] = -1; }
}

// ---- tracks
static void track_setup(track_t *tr, int kind, int sound)
{
    tr->kind = kind; tr->sound = sound;
    if (KIND_SYNTH[kind] >= 0) {
        int voices = kind == K_KEYS ? 3 : 1;
        synth_init(&tr->pb, synth_preset(KIND_SYNTH[kind], sound), voices);
        synth_init(&tr->live, synth_preset(KIND_SYNTH[kind], sound), voices);
    }
    if (kind == K_DRUMS) drums_init(&tr->drums, S.kit);
    tr->live_note = -1;
}

static void track_new(int i, int kind)
{
    track_t *tr = &S.tr[i];
    memset(tr, 0, sizeof *tr);
    tr->vol = 80; tr->rev = kind == K_DRUMS ? 10 : 25; tr->dly = (kind == K_LEAD || kind == K_VOCAL) ? 25 : 0;
    tr->pan = 0; tr->tone = 0; tr->lowcut = kind == K_DRUMS || kind == K_BASS ? 0 : 2;
    tr->octave = 0; tr->fx = 0; tr->fx_amt = 50;
    for (int p = 0; p < PAT_N; p++) tr->vslot[p] = -1;
    memset(S.drum[i], 0, sizeof S.drum[i]);
    memset(S.seq[i], 0, sizeof S.seq[i]);
    track_setup(tr, kind, 0);
}

static void set_grid(float bpm, int bars)
{
    S.bpm = bpm; S.bars = bars; S.steps = bars * 16;
    S.step_len = 15.0f * FS / S.bpm;
    S.loop_len = (int)(S.step_len * S.steps + 0.5f);
    if (S.loop_len > S.vcap) S.loop_len = S.vcap;
    if (S.pos >= S.loop_len) S.pos = 0;
    S.last_step = -1;
    S.clock_next = 0;
    mix_set_tempo(bpm);
}

static void apply_key(void)
{
    if (S.key == 0) return;
    S.scale.root = (S.key - 1) / 2; S.scale.minor = (S.key - 1) % 2; S.scale.locked = true;
}

static void new_song(void)
{
    S.armed = false; S.rec = false; S.scene = 0; S.scene_next = -1; S.song_mode = false; S.arr_pos = 0; S.arr_loops = 0;
    for (int i = 0; i < VOCAL_SLOTS; i++) { S.vowner[i][0] = -1; S.vowner[i][1] = -1; }
    static const int def[5] = { K_DRUMS, K_BASS, K_KEYS, K_LEAD, K_VOCAL };
    S.n_tracks = 5;
    for (int i = 0; i < TRACK_N; i++) track_new(i, i < 5 ? def[i] : K_LEAD);
    S.cur = 0;
    memset(S.pc_w, 0, sizeof S.pc_w);
    S.scale.locked = false; apply_key();
    memset(S.arr, 0, sizeof S.arr); S.arr[0] = 1; S.arr_rep = 2;
    S.bk_track = -1;
    mark(DIRTY_STATE);
}

static void init(void)
{
    memset(&S, 0, sizeof S);
    S.last_drum = -1; S.bg_db = -60; S.last_step = -1; S.bk_track = -1; S.scene_next = -1;
    S.rnd = 777; S.expr_cut = 1;
    S.swing = 0; S.human = 20; S.kit = 0; S.count_in = 1; S.metro = false; S.quant8 = 0; S.key = 0;
    S.mic_gain = CONFIG_KIT_MIC_GAIN; S.gate_db = CONFIG_KIT_GATE_DB; S.stability = 1; S.early_type = -1;
    drums_analyser_init();
    tune_init();
    mix_init();
    S.click_c = expf(-1.0f / (0.012f * FS));
    S.vcap = CONFIG_KIT_LOOP_MAX_SEC * CONFIG_KIT_SAMPLE_RATE;
    for (int i = 0; i < VOCAL_SLOTS; i++) S.vpool[i] = heap_caps_malloc(S.vcap, MALLOC_CAP_SPIRAM);
    S.bk_vocal = heap_caps_malloc(S.vcap, MALLOC_CAP_SPIRAM);
    set_grid(CONFIG_KIT_BPM, CONFIG_KIT_LOOP_BARS);
    new_song();
    ESP_LOGI(TAG, "%d bpm, %d bars, loop %d ms; %d vocal slots of %d KB, free heap %u", CONFIG_KIT_BPM,
             S.bars, (int)(S.loop_len * 1000 / FS), VOCAL_SLOTS, S.vcap / 1024, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    S.dirty = 0;
    S.ready = true;
}

bool looper_ready(void) { return S.ready; }

static inline int step_at(int pos)
{
    int s = (int)lroundf(pos / S.step_len);
    if (S.quant8) s = (int)lroundf(pos / (2 * S.step_len)) * 2;
    return ((s % S.steps) + S.steps) % S.steps;
}
static inline float rnd01(void) { return (frand(&S.rnd) + 1.0f) * 0.5f; }

static void snap_all(void)
{
    for (int t = 0; t < S.n_tracks; t++) {
        if (KIND_SYNTH[S.tr[t].kind] < 0) continue;
        for (int p = 0; p < PAT_N; p++) for (int s = 0; s < S.steps; s++) {
            uint8_t b = S.seq[t][p][s];
            if (b & 0x7F) S.seq[t][p][s] = (b & SEQ_ATTACK) | scale_snap(&S.scale, (float)(b & 0x7F));
        }
    }
}

// ---- takes
static void backup_pat(void)
{
    int t = S.cur, p = rec_pat();
    track_t *tr = cur();
    S.bk_track = t; S.bk_pat = p; S.bk_has = tr->has[p];
    if (tr->kind == K_DRUMS) memcpy(S.bk_drum, S.drum[t][p], sizeof S.bk_drum);
    else if (tr->kind == K_VOCAL) { int s = vslot_find(t, p); if (s >= 0 && S.bk_vocal) memcpy(S.bk_vocal, S.vpool[s], S.loop_len); }
    else memcpy(S.bk_seq, S.seq[t][p], LOOP_MAX_STEPS);
}

static void undo(void)
{
    int t = S.bk_track, p = S.bk_pat;
    if (t < 0) { say("nothing to undo"); return; }
    track_t *tr = &S.tr[t];
    if (tr->kind == K_DRUMS) memcpy(S.drum[t][p], S.bk_drum, sizeof S.bk_drum);
    else if (tr->kind == K_VOCAL) { int s = vslot_find(t, p); if (s >= 0 && S.bk_vocal) memcpy(S.vpool[s], S.bk_vocal, S.loop_len); }
    else memcpy(S.seq[t][p], S.bk_seq, LOOP_MAX_STEPS);
    tr->has[p] = S.bk_has;
    if (!S.bk_has && tr->kind == K_VOCAL) vslot_free(t, p);
    S.bk_track = -1;
    say("undone");
    mark(DIRTY_STATE);
    if (tr->kind == K_VOCAL) mark_vocal(t, p);
}

static bool song_empty(void)
{
    for (int t = 0; t < S.n_tracks; t++) for (int p = 0; p < PAT_N; p++) if (S.tr[t].has[p]) return false;
    return true;
}

static void arm(void)
{
    track_t *tr = cur();
    if (tr->kind == K_VOCAL && vslot_find(S.cur, rec_pat()) < 0) {
        bool free_slot = false;
        for (int i = 0; i < VOCAL_SLOTS; i++) if (S.vowner[i][0] < 0 && S.vpool[i]) free_slot = true;
        if (!free_slot) { say("vocal memory full"); return; }
    }
    S.armed = true;
    if (song_empty()) {
        int bars = S.count_in < S.bars ? S.count_in : S.bars;
        S.pos = S.loop_len - (int)(S.step_len * 16 * bars); S.last_step = -1;
    }
}

static void clear_pat(int t, int p)
{
    track_t *tr = &S.tr[t];
    if (tr->kind == K_DRUMS) memset(S.drum[t][p], 0, sizeof S.drum[t][p]);
    else if (tr->kind == K_VOCAL) vslot_free(t, p);
    else { memset(S.seq[t][p], 0, LOOP_MAX_STEPS); synth_note_off(&tr->pb, -1); }
    tr->has[p] = false;
    mark(DIRTY_STATE | (tr->kind == K_VOCAL ? DIRTY_VOCAL : 0));
}

static void rec_begin(void)
{
    S.armed = false; S.rec = true;
    int t = S.cur, p = rec_pat();
    track_t *tr = cur();
    backup_pat();
    bool overdub = tr->has[p] && !S.replace_next;
    S.replace_next = false;
    if (tr->kind == K_VOCAL) { int s = vslot_alloc(t, p); if (s < 0) { S.rec = false; say("vocal memory full"); return; } if (!overdub) memset(S.vpool[s], 0xFF, S.loop_len); }
    else if (!overdub) { if (tr->kind == K_DRUMS) memset(S.drum[t][p], 0, sizeof S.drum[t][p]); else memset(S.seq[t][p], 0, LOOP_MAX_STEPS); }
    if (!overdub) tr->has[p] = false;
    memset(S.pc_w, 0, sizeof S.pc_w);
    memset(S.fresh, 0, sizeof S.fresh);
    // a hit or note that came in just before the downbeat belongs to step 0
    if (tr->kind == K_DRUMS && S.early_type >= 0) { S.drum[t][p][0][S.early_type] = S.early_vel; S.fresh[0] |= 1 << S.early_type; }
    if (KIND_SYNTH[tr->kind] >= 0 && tr->live_note >= 0) { S.seq[t][p][0] = tr->live_note | SEQ_ATTACK; S.fresh[0] |= 0x80; }
    S.early_type = -1;
}

static void rec_end(void)
{
    S.rec = false;
    S.post_roll = (int)(S.step_len * 0.4f);
    track_t *tr = cur();
    if (KIND_SYNTH[tr->kind] >= 0 && !S.scale.locked && S.key == 0) {
        float total = 0;
        for (int i = 0; i < 12; i++) total += S.pc_w[i];
        if (total > 10 && scale_detect(S.pc_w, &S.scale)) {
            snap_all();
            char nm[10];
            ESP_LOGI(TAG, "key locked: %s", scale_name(&S.scale, nm, sizeof nm));
        }
    }
    tr->has[rec_pat()] = true;
    mark(DIRTY_STATE);
    if (tr->kind == K_VOCAL) mark_vocal(S.cur, rec_pat());
}

static void cancel(void)
{
    S.armed = false;
    if (S.rec) { S.rec = false; undo(); say("take cancelled"); }
}

static void live_off(void)
{
    for (int t = 0; t < TRACK_N; t++) { if (KIND_SYNTH[S.tr[t].kind] >= 0) synth_note_off(&S.tr[t].live, -1); S.tr[t].live_note = -1; }
}

static void select_track(int t)
{
    cancel(); live_off();
    S.cur = ((t % S.n_tracks) + S.n_tracks) % S.n_tracks;
    if (cur()->kind == K_VOCAL) tune_set_mode(cur()->sound);
    mark(DIRTY_SOFT);
}

static void add_track(int kind);
static int add_kind = K_LEAD;
static void do_action(int a)
{
    switch (a) {
    case ACT_ADD_TRACK:  add_track(add_kind); break;
    case ACT_TAP:        if (S.armed || S.rec) cancel(); else arm(); break;
    case ACT_CANCEL:     cancel(); break;
    case ACT_NEXT_TRACK: select_track(S.cur + 1); break;
    case ACT_PREV_TRACK: select_track(S.cur - 1); break;
    case ACT_UNDO:       undo(); break;
    case ACT_CLEAR_PAT:  backup_pat(); clear_pat(S.cur, rec_pat()); say("pattern cleared"); break;
    case ACT_MUTE:       cur()->mute = !cur()->mute; mark(DIRTY_STATE); break;
    case ACT_SOLO:       cur()->solo = !cur()->solo; mark(DIRTY_STATE); break;
    case ACT_CLEAR_SONG: for (int t = 0; t < S.n_tracks; t++) for (int p = 0; p < PAT_N; p++) clear_pat(t, p); S.scale.locked = false; apply_key(); say("song cleared"); break;
    case ACT_NEW_SONG:   new_song(); say("new song"); break;
    case ACT_REPLACE:    S.replace_next = !S.replace_next; say(S.replace_next ? "next take replaces" : "next take overdubs"); break;
    case ACT_SCENE_UP:   S.scene_next = (S.scene + 1) % PAT_N; break;
    case ACT_SCENE_DOWN: S.scene_next = (S.scene + PAT_N - 1) % PAT_N; break;
    case ACT_DEL_TRACK:
        if (S.n_tracks <= 1) { say("last track"); break; }
        for (int p = 0; p < PAT_N; p++) vslot_free(S.cur, p);
        for (int t = S.cur; t < S.n_tracks - 1; t++) {
            S.tr[t] = S.tr[t + 1];
            memcpy(S.drum[t], S.drum[t + 1], sizeof S.drum[t]);
            memcpy(S.seq[t], S.seq[t + 1], sizeof S.seq[t]);
            for (int i = 0; i < VOCAL_SLOTS; i++) if (S.vowner[i][0] == t + 1) S.vowner[i][0] = t;
        }
        S.n_tracks--;
        if (S.cur >= S.n_tracks) S.cur = S.n_tracks - 1;
        S.bk_track = -1;
        say("track deleted"); mark(DIRTY_STATE | DIRTY_VOCAL);
        break;
    case ACT_COPY_A: {
        int t = S.cur, p = rec_pat();
        if (p == 0) { say("this is A"); break; }
        backup_pat();
        track_t *tr = cur();
        if (tr->kind == K_DRUMS) memcpy(S.drum[t][p], S.drum[t][0], sizeof S.drum[t][p]);
        else if (tr->kind == K_VOCAL) { int a = vslot_find(t, 0), b = vslot_alloc(t, p); if (a >= 0 && b >= 0) memcpy(S.vpool[b], S.vpool[a], S.loop_len); }
        else memcpy(S.seq[t][p], S.seq[t][0], LOOP_MAX_STEPS);
        tr->has[p] = tr->has[0];
        say("copied from A"); mark(DIRTY_STATE);
        if (tr->kind == K_VOCAL) mark_vocal(t, p);
        break; }
    case ACT_SONG_MODE:  S.song_mode = !S.song_mode; S.arr_pos = 0; S.arr_loops = 0; if (S.song_mode && S.arr[0]) S.scene_next = S.arr[0] - 1; mark(DIRTY_STATE); break;
    }
}

static void add_track(int kind)
{
    if (S.n_tracks >= TRACK_N) { say("8 tracks max"); return; }
    track_new(S.n_tracks, kind);
    S.n_tracks++;
    select_track(S.n_tracks - 1);
    say("track added");
}

static void handle_actions(void)
{
    while (S.act_n > 0) {
        int a = S.act_q[0];
        for (int i = 1; i < S.act_n; i++) S.act_q[i - 1] = S.act_q[i];
        S.act_n--;
        do_action(a);
    }
}

// ---- sequencer
static void drum_hit(track_t *tr, int t, float vel)
{
    drums_trigger(&tr->drums, t, vel);
    if (t == DRUM_KICK) mix_kick();
}

static void play_seq(int t, int s)
{
    track_t *tr = &S.tr[t];
    int p = (S.rec && t == S.cur) ? rec_pat() : pat_of(t, S.scene);
    uint8_t b = S.seq[t][p][s];
    int note = b & 0x7F;
    if (!note) { synth_note_off(&tr->pb, -1); return; }
    if (!(b & SEQ_ATTACK)) return;
    float vel = 0.9f - S.human * 0.002f * rnd01();
    if (tr->kind == K_KEYS) {
        int tri[3];
        scale_triad(&S.scale, note, tri);
        synth_note_off(&tr->pb, -1);
        for (int i = 0; i < 3; i++) synth_note_on(&tr->pb, tri[i], vel);
    } else synth_note_on(&tr->pb, note, vel);
}

static void click(bool accent)
{
    S.click_amp = accent ? 0.5f : 0.3f;
    S.click_hz = accent ? 2000.0f : 1400.0f;
    S.click_ph = 0;
}

static void on_step(int s)
{
    bool metro = S.armed || S.rec || S.metro;
    if (metro && (s % 4) == 0) click((s % 16) == 0);
    for (int t = 0; t < S.n_tracks; t++) {
        track_t *tr = &S.tr[t];
        bool rec_this = S.rec && t == S.cur;
        int p = pat_of(t, S.scene);
        if (tr->kind == K_DRUMS) {
            if (rec_this) p = rec_pat();
            for (int d = 0; d < DRUM_N; d++) {
                uint8_t v = S.drum[t][p][s][d];
                if (!v || (rec_this && (S.fresh[s] & (1 << d)))) continue;
                float vel = v / 127.0f * (1.0f - S.human * 0.003f * rnd01());
                int delay = S.human > 0 ? (int)(rnd01() * S.human * 0.04f) : 0;
                if (delay == 0) drum_hit(tr, d, vel);
                else for (int i = 0; i < 8; i++) if (S.pend[i].left == 0) { S.pend[i].left = delay; S.pend[i].track = t; S.pend[i].type = d; S.pend[i].vel = (uint8_t)(vel * 127); break; }
            }
        } else if (KIND_SYNTH[tr->kind] >= 0) {
            if (rec_this) {
                if (tr->live_note >= 0) {
                    uint8_t *b = &S.seq[t][rec_pat()][s];
                    if (!((*b & SEQ_ATTACK) && (*b & 0x7F) == tr->live_note)) *b = tr->live_note;
                    S.fresh[s] |= 0x80;
                } else if (!(S.fresh[s] & 0x80)) play_seq(t, s);   // overdub: the old notes still play
            } else play_seq(t, s);
        }
    }
}

// ---- live voice
static int kind_note(int kind, int note)
{
    int n = scale_snap(&S.scale, (float)note);
    switch (kind) {
    case K_BASS: while (n > 52) n -= 12; while (n < 28) n += 12; break;
    case K_KEYS: while (n > 64) n -= 12; while (n < 48) n += 12; break;
    default:     while (n > 88) n -= 12; while (n < 45) n += 12; break;
    }
    n += cur()->octave * 12;
    return n < 24 ? 24 : (n > 100 ? 100 : n);
}

static void live_note_on(track_t *tr, int n, int lag)
{
    if (tr->kind == K_KEYS) {
        int tri[3];
        scale_triad(&S.scale, n, tri);
        synth_note_off(&tr->live, -1);
        for (int i = 0; i < 3; i++) synth_note_on(&tr->live, tri[i], 0.9f);
    } else synth_note_on(&tr->live, n, 0.9f);
    if (S.rec) { int st = step_at(S.pos - lag); S.seq[S.cur][rec_pat()][st] = n | SEQ_ATTACK; S.fresh[st] |= 0x80; }
    else if (S.post_roll > 0 && S.pos - lag < S.post_roll) { S.seq[S.cur][rec_pat()][0] = n | SEQ_ATTACK; }
}

static void melodic_live(const voice_t *v)
{
    track_t *tr = cur();
    int nn = (v->voiced && v->note >= 0) ? kind_note(tr->kind, v->note) : -1;
    if (nn != tr->live_note) {
        if (nn < 0) synth_note_off(&tr->live, -1);
        else {
            int lag = tr->live_note < 0 ? v->since_onset : NOTE_LAG;
            if (lag > ONSET_MAX_S) lag = ONSET_MAX_S;
            live_note_on(tr, nn, lag);
        }
        tr->live_note = nn;
    }
    if (S.rec && v->voiced && v->note >= 0) S.pc_w[v->note % 12] += 1;
}

void looper_midi_note(int note, int vel, bool on)
{
    track_t *tr = cur();
    if (tr->kind == K_DRUMS) {
        if (!on) return;
        int d = note == 36 || note == 35 ? DRUM_KICK : (note == 42 || note == 44 || note == 46 ? DRUM_HAT : DRUM_SNARE);
        drum_hit(tr, d, vel / 127.0f);
        if (S.rec) { int s = step_at(S.pos); uint8_t *b = &S.drum[S.cur][rec_pat()][s][d]; if (vel > *b) *b = vel; }
        return;
    }
    if (KIND_SYNTH[tr->kind] < 0) return;
    if (on) { int n = kind_note(tr->kind, note); live_note_on(tr, n, 0); tr->live_note = n; }
    else { synth_note_off(&tr->live, -1); tr->live_note = -1; }
}

static void drums_live(const float *in, int n, const voice_t *v)
{
    track_t *tr = cur();
    drum_bands_t b;
    drums_analyse(in, n, &b);
    if (S.refr > 0) S.refr -= n;
    const float thr = voice_noise_db() + CONFIG_KIT_ONSET_DB;
    if (!S.capturing && S.refr <= 0 && v->db > thr && v->db > S.bg_db + 8.0f) {
        S.capturing = true; S.cap_len = 0;
        S.cap_pos = S.pos - HIT_LAG;
        S.cap_db = v->db; S.cap.lo = S.cap.hi = S.cap.all = 0; S.refr = REFRACT_S;
    }
    const float blk = (float)n / FS;
    S.bg_db += (1 - expf(-blk / (v->db > S.bg_db ? 0.004f : 0.05f))) * (v->db - S.bg_db);
    if (S.capturing) {
        S.cap.lo += b.lo; S.cap.hi += b.hi; S.cap.all += b.all;
        if (v->db > S.cap_db) S.cap_db = v->db;
        S.cap_len += n;
        if (S.cap_len >= CLASSIFY_S) {
            S.capturing = false;
            int type = drums_classify(&S.cap, &S.drum_lo, &S.drum_hi);
            float vel = clampf(0.35f + (S.cap_db - thr) / 25.0f, 0.35f, 1.0f);
            drum_hit(tr, type, vel);
            S.last_drum = type;
            uint8_t v7 = (uint8_t)(vel * 127);
            if (S.rec) {
                int st = step_at(S.cap_pos);
                uint8_t *slot = &S.drum[S.cur][rec_pat()][st][type];
                if (v7 > *slot) *slot = v7;
                S.fresh[st] |= 1 << type;
            } else if (S.armed && S.loop_len - S.cap_pos < S.step_len * 0.5f) {
                S.early_type = type; S.early_vel = v7;            // just before the downbeat: keep for step 0
            } else if (S.post_roll > 0 && S.cap_pos < S.post_roll) {
                uint8_t *slot = &S.drum[S.cur][rec_pat()][0][type];   // just after the take ended: still step 0
                if (v7 > *slot) *slot = v7;
                mark(DIRTY_STATE);
            }
        }
    }
}

static void loop_start(void)
{
    if (S.rec) rec_end();
    if (S.scene_next >= 0 && !S.armed) { S.scene = S.scene_next; S.scene_next = -1; }
    if (S.armed) rec_begin();
    if (S.song_mode && !S.rec) {
        if (++S.arr_loops >= S.arr_rep) {
            S.arr_loops = 0;
            int next = S.arr_pos + 1;
            if (next >= ARR_N || !S.arr[next]) next = 0;
            S.arr_pos = next;
            if (S.arr[next]) S.scene_next = S.arr[next] - 1;
        }
    }
    if (S.bounce_on && !S.bounce_running && !S.bounce_done) { S.bounce_running = true; S.bounce_pos = 0; }
}

static void IRAM_ATTR process(const float *in, float *out, int n, const voice_t *v)
{
    handle_actions();
    if (S.msg_samples > 0 && (S.msg_samples -= n) <= 0) S.msg[0] = 0;
    if (S.post_roll > 0) S.post_roll -= n;
    if (S.pos == 0 || S.last_step < 0) loop_start();

    int s = (int)(S.pos / S.step_len);
    if (s >= S.steps) s = S.steps - 1;
    if ((s & 1) && S.pos - s * S.step_len < S.swing * 0.01f * S.step_len / 3.0f) s--;
    if (s != S.last_step) { on_step(s); S.last_step = s; }
    for (int i = 0; i < 8; i++) if (S.pend[i].left > 0 && --S.pend[i].left == 0) drum_hit(&S.tr[S.pend[i].track], S.pend[i].type, S.pend[i].vel / 127.0f);
    if (S.clock_cb) {
        int tick = (int)(S.step_len * 4 / 24);
        while (S.pos >= S.clock_next) { S.clock_cb(); S.clock_next += tick; }
        if (S.pos + n >= S.loop_len) S.clock_next = 0;
    }

    uint32_t c0 = esp_cpu_get_cycle_count();
    track_t *tr = cur();
    switch (tr->kind) {
    case K_DRUMS: drums_live(in, n, v); break;
    case K_VOCAL: tune_process(in, S.live_out, n, v, &S.scale); tr->live_note = tune_target(); break;
    default:      melodic_live(v); break;
    }
    if (KIND_SYNTH[tr->kind] >= 0) synth_set_expr(&tr->live, S.expr_bend, S.expr_cut, S.expr_vib);

    uint32_t c1 = esp_cpu_get_cycle_count();
    float *lay[TRACK_N];
    bool any_solo = false;
    for (int t = 0; t < S.n_tracks; t++) if (S.tr[t].solo) any_solo = true;
    for (int t = 0; t < S.n_tracks; t++) {
        track_t *k = &S.tr[t];
        lay[t] = S.tbuf[t];
        memset(lay[t], 0, n * sizeof(float));
        if (k->kind == K_DRUMS) drums_render(&k->drums, lay[t], n, 0.8f);
        else if (KIND_SYNTH[k->kind] >= 0) { synth_render(&k->pb, lay[t], n); if (t == S.cur) synth_render(&k->live, lay[t], n); }
        else {
            bool vrec = S.rec && t == S.cur;
            int p = vrec ? rec_pat() : pat_of(t, S.scene);
            int slot = vslot_find(t, p);
            if (slot >= 0 && (k->has[p] || vrec)) {
                uint8_t *vb = S.vpool[slot];
                for (int i = 0; i < n; i++) {
                    int q = S.pos + i;
                    if (q >= S.loop_len) q -= S.loop_len;
                    if (vrec) { float old = k->has[p] ? mulaw_dec(vb[q]) : 0; vb[q] = mulaw_enc(old + S.live_out[i]); }
                    else lay[t][i] += mulaw_dec(vb[q]) * 0.9f;
                }
            }
            if (t == S.cur) for (int i = 0; i < n; i++) lay[t][i] += S.live_out[i] * 0.9f;
        }
    }
    uint32_t c2 = esp_cpu_get_cycle_count();
    mix_ch_t ch[TRACK_N];
    for (int t = 0; t < S.n_tracks; t++) {
        track_t *k = &S.tr[t];
        ch[t].vol = k->vol * 0.01f; ch[t].pan = k->pan * 0.01f;
        ch[t].rev = k->rev * 0.01f; ch[t].dly = k->dly * 0.01f;
        ch[t].lowcut_hz = k->lowcut * 50.0f; ch[t].tone = k->tone * 0.01f;
        ch[t].mute = k->mute || (any_solo && !k->solo);
        ch[t].duck = k->kind != K_DRUMS;
        ch[t].fx = k->fx; ch[t].fx_amt = k->fx_amt * 0.01f;
        if (t == S.cur && S.expr_space > ch[t].rev) ch[t].rev = S.expr_space;
    }
    mix_process(lay, ch, S.n_tracks, out, n);
    uint32_t c3 = esp_cpu_get_cycle_count();
    S.prof[0] += (c1 - c0) >> 4; S.prof[1] += (c2 - c1) >> 4; S.prof[2] += (c3 - c2) >> 4;

    if (S.click_amp > 0.001f) {
        const float inc = S.click_hz / FS;
        for (int i = 0; i < n; i++) {
            float c = fast_sin01(S.click_ph) * S.click_amp;
            out[2 * i] += c; out[2 * i + 1] += c;
            S.click_ph += inc; if (S.click_ph >= 1) S.click_ph -= 1;
            S.click_amp *= S.click_c;
        }
    }
    for (int i = 0; i < 2 * n; i++) { float a = fabsf(out[i]); if (a > S.peak) S.peak = a; }

    if (S.bounce_running) {
        for (int i = 0; i < n && S.bounce_pos + i < S.loop_len; i++) {
            for (int k = 0; k < 3; k++) {
                int t = S.bounce_first + k;
                float x;
                if (S.bounce_first >= S.n_tracks) x = k == 0 ? out[2 * i] : (k == 1 ? out[2 * i + 1] : 0);
                else x = t < S.n_tracks ? (ch[t].mute ? 0 : lay[t][i] * ch[t].vol) : 0;
                S.bounce[k][S.bounce_pos + i] = (int16_t)(clampf(x, -1, 1) * 32767);
            }
        }
        S.bounce_pos += n;
        if (S.bounce_pos >= S.loop_len) { S.bounce_running = false; S.bounce_done = true; }
    }

    S.pos += n;
    if (S.pos >= S.loop_len) S.pos -= S.loop_len;
    if (S.pos < n && S.pos != 0) S.pos = 0;
}

static void status(char *buf, size_t len) { snprintf(buf, len, "%s%s", KIND_NAMES[cur()->kind], S.rec ? " REC" : ""); }

const fx_t fx_looper = { "LOOPER", init, process, status, NULL, NULL, true };

// ---- UI side ---------------------------------------------------------------
void looper_action(int act) { if (S.act_n < 8) S.act_q[S.act_n++] = act; }
const char *looper_kind_name(int kind) { return kind >= 0 && kind < K_N ? KIND_NAMES[kind] : "?"; }

static const char *PNAME[PR_N] = {
    "Volume", "Pan", "Reverb", "Delay", "Low cut", "Tone", "Sound", "Octave", "Effect", "Fx amount", "Stability",
    "Kit", "Swing", "Humanise", "Tempo", "Bars", "Key", "Count-in", "Metronome", "Quantise", "Mic gain", "Gate", "Add track",
    "Pump", "Drive", "Master tone",
    "Scene", "Arr 1", "Arr 2", "Arr 3", "Arr 4", "Arr 5", "Arr 6", "Arr 7", "Arr 8", "Loops/scene",
};
const char *looper_param_name(int p) { return p >= 0 && p < PR_N ? PNAME[p] : "?"; }

int looper_param_get(int p)
{
    track_t *tr = cur();
    switch (p) {
    case PR_VOL:    return tr->vol;
    case PR_PAN:    return tr->pan;
    case PR_REV:    return tr->rev;
    case PR_DLY:    return tr->dly;
    case PR_LOWCUT: return tr->lowcut;
    case PR_TONE:   return tr->tone;
    case PR_SOUND:  return tr->sound;
    case PR_OCTAVE: return tr->octave;
    case PR_FX:     return tr->fx;
    case PR_FXAMT:  return tr->fx_amt;
    case PR_STABLE: return S.stability;
    case PR_KIT:    return S.kit;
    case PR_SWING:  return S.swing;
    case PR_HUMAN:  return S.human;
    case PR_BPM:    return (int)(S.bpm + 0.5f);
    case PR_BARS:   return S.bars;
    case PR_KEY:    return S.key;
    case PR_COUNTIN: return S.count_in;
    case PR_METRO:  return S.metro;
    case PR_QUANT:  return S.quant8;
    case PR_MICGAIN: return S.mic_gain;
    case PR_GATE:   return S.gate_db;
    case PR_ADDKIND: return add_kind;
    case PR_PUMP:   return (int)(mix_params()->pump * 100 + 0.5f);
    case PR_DRIVE:  return (int)(mix_params()->drive * 10 + 0.5f);
    case PR_MTONE:  return (int)(mix_params()->tone * 100);
    case PR_SCENE:  return S.scene_next >= 0 ? S.scene_next : S.scene;
    case PR_ARR_REP: return S.arr_rep;
    default: if (p >= PR_ARR0 && p <= PR_ARR7) return S.arr[p - PR_ARR0]; return 0;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void looper_param_set(int p, int v)
{
    track_t *tr = cur();
    switch (p) {
    case PR_VOL:    tr->vol = clampi(v, 0, 100); break;
    case PR_PAN:    tr->pan = clampi(v, -100, 100); break;
    case PR_REV:    tr->rev = clampi(v, 0, 100); break;
    case PR_DLY:    tr->dly = clampi(v, 0, 100); break;
    case PR_LOWCUT: tr->lowcut = clampi(v, 0, 8); break;
    case PR_TONE:   tr->tone = clampi(v, -100, 100); break;
    case PR_SOUND:
        if (tr->kind == K_VOCAL) { tr->sound = ((v % TUNE_N) + TUNE_N) % TUNE_N; tune_set_mode(tr->sound); }
        else if (KIND_SYNTH[tr->kind] >= 0) { tr->sound = ((v % SYNTH_PRESETS) + SYNTH_PRESETS) % SYNTH_PRESETS; track_setup(tr, tr->kind, tr->sound); }
        break;
    case PR_OCTAVE: tr->octave = clampi(v, -2, 2); break;
    case PR_FX:     tr->fx = ((v % FX_N) + FX_N) % FX_N; break;
    case PR_FXAMT:  tr->fx_amt = clampi(v, 0, 100); break;
    case PR_STABLE: S.stability = clampi(v, 0, 2); voice_set_stability(S.stability); break;
    case PR_KIT:    S.kit = ((v % KIT_N) + KIT_N) % KIT_N; for (int t = 0; t < TRACK_N; t++) if (S.tr[t].kind == K_DRUMS) drums_set_kit(&S.tr[t].drums, S.kit); S.kit = cur()->kind == K_DRUMS ? cur()->drums.kit : S.kit; break;
    case PR_SWING:  S.swing = clampi(v, 0, 100); break;
    case PR_HUMAN:  S.human = clampi(v, 0, 100); break;
    case PR_BPM:    v = clampi(v, 40, 240); if (v != (int)(S.bpm + 0.5f)) { set_grid(v, S.bars); for (int t = 0; t < S.n_tracks; t++) if (S.tr[t].kind == K_VOCAL) for (int q = 0; q < PAT_N; q++) if (S.tr[t].has[q]) clear_pat(t, q); } break;
    case PR_BARS:   v = clampi(v, 1, LOOP_MAX_BARS); if (v != S.bars) { set_grid(S.bpm, v); for (int t = 0; t < S.n_tracks; t++) if (S.tr[t].kind == K_VOCAL) for (int q = 0; q < PAT_N; q++) if (S.tr[t].has[q]) clear_pat(t, q); } break;
    case PR_KEY:    S.key = clampi(v, 0, 24); if (S.key) { apply_key(); snap_all(); } else S.scale.locked = false; break;
    case PR_COUNTIN: S.count_in = clampi(v, 1, 2); break;
    case PR_METRO:  S.metro = v & 1; break;
    case PR_QUANT:  S.quant8 = v & 1; break;
    case PR_MICGAIN: S.mic_gain = clampi(v, 1, 64); audio_set_mic_gain(S.mic_gain); break;
    case PR_GATE:   S.gate_db = clampi(v, -70, -10); voice_set_gate_db(S.gate_db); break;
    case PR_ADDKIND: add_kind = ((v % K_N) + K_N) % K_N; break;
    case PR_PUMP:   mix_params()->pump = clampf(v * 0.01f, 0, 1); break;
    case PR_DRIVE:  mix_params()->drive = clampf(v * 0.1f, 1, 4); break;
    case PR_MTONE:  mix_params()->tone = clampf(v * 0.01f, -1, 1); break;
    case PR_SCENE:  S.scene_next = ((v % PAT_N) + PAT_N) % PAT_N; break;
    case PR_ARR_REP: S.arr_rep = v <= 1 ? 1 : (v <= 2 ? 2 : 4); break;
    default: if (p >= PR_ARR0 && p <= PR_ARR7) S.arr[p - PR_ARR0] = clampi(v, 0, PAT_N); break;
    }
    mark(DIRTY_STATE);
}

void looper_param_step(int p, int dir)
{
    int step = 5;
    switch (p) {
    case PR_LOWCUT: case PR_SOUND: case PR_KIT: case PR_BPM: case PR_KEY: case PR_COUNTIN: case PR_METRO: case PR_QUANT:
    case PR_OCTAVE: case PR_FX: case PR_STABLE:
    case PR_MICGAIN: case PR_GATE: case PR_ADDKIND: case PR_SCENE: case PR_ARR_REP: case PR_DRIVE: step = 1; break;
    case PR_PAN: case PR_TONE: case PR_MTONE: step = 10; break;
    default: if (p >= PR_ARR0 && p <= PR_ARR7) step = 1; break;
    }
    if (p == PR_BARS) { int b = S.bars; b = dir > 0 ? (b < 8 ? b * 2 : 8) : (b > 1 ? b / 2 : 1); looper_param_set(p, b); return; }
    if (p == PR_ARR_REP) { int r = S.arr_rep; r = dir > 0 ? (r < 4 ? r * 2 : 4) : (r > 1 ? r / 2 : 1); looper_param_set(p, r); return; }
    if (p == PR_METRO || p == PR_QUANT) { looper_param_set(p, !looper_param_get(p)); return; }
    looper_param_set(p, looper_param_get(p) + dir * step);
}

void looper_param_text(int p, char *buf, int len)
{
    static const char *KEYN[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    track_t *tr = cur();
    int v = looper_param_get(p);
    switch (p) {
    case PR_SOUND:
        if (tr->kind == K_VOCAL) snprintf(buf, len, "%s", tune_mode_name(v));
        else if (KIND_SYNTH[tr->kind] >= 0) snprintf(buf, len, "%s", synth_preset_name(KIND_SYNTH[tr->kind], v));
        else snprintf(buf, len, "%s", kit_name(S.kit));
        break;
    case PR_KIT:    snprintf(buf, len, "%s", kit_name(v)); break;
    case PR_OCTAVE: snprintf(buf, len, "%+d", v); break;
    case PR_FX:     snprintf(buf, len, "%s", mix_fx_name(v)); break;
    case PR_STABLE: snprintf(buf, len, "%s", v == 0 ? "loose" : (v == 1 ? "normal" : "steady")); break;
    case PR_DRIVE:  snprintf(buf, len, "%d.%d", v / 10, v % 10); break;
    case PR_BPM:    snprintf(buf, len, "%d", v); break;
    case PR_BARS:   snprintf(buf, len, "%d", v); break;
    case PR_KEY:    if (!v) snprintf(buf, len, "auto"); else snprintf(buf, len, "%s %s", KEYN[(v - 1) / 2], (v - 1) % 2 ? "min" : "maj"); break;
    case PR_LOWCUT: if (!v) snprintf(buf, len, "off"); else snprintf(buf, len, "%dHz", v * 50); break;
    case PR_PAN:    if (!v) snprintf(buf, len, "C"); else snprintf(buf, len, "%c%d", v < 0 ? 'L' : 'R', abs(v)); break;
    case PR_TONE: case PR_MTONE: snprintf(buf, len, "%+d", v / 10); break;
    case PR_COUNTIN: snprintf(buf, len, "%d bar", v); break;
    case PR_METRO:  snprintf(buf, len, "%s", v ? "on" : "off"); break;
    case PR_QUANT:  snprintf(buf, len, "%s", v ? "1/8" : "1/16"); break;
    case PR_MICGAIN: snprintf(buf, len, "x%d", v); break;
    case PR_GATE:   snprintf(buf, len, "%ddB", v); break;
    case PR_ADDKIND: snprintf(buf, len, "%s", KIND_NAMES[v]); break;
    case PR_SCENE:  snprintf(buf, len, "%c", 'A' + v); break;
    case PR_ARR_REP: snprintf(buf, len, "%d", v); break;
    default:
        if (p >= PR_ARR0 && p <= PR_ARR7) { if (!v) snprintf(buf, len, "-"); else snprintf(buf, len, "%c", 'A' + v - 1); }
        else snprintf(buf, len, "%d%%", v);
        break;
    }
}

void looper_expr(float cut, float bend, float vib, float space)
{
    S.expr_cut = cut; S.expr_bend = bend; S.expr_vib = vib; S.expr_space = space;
}

void looper_get_ui(looper_ui_t *u)
{
    track_t *tr = cur();
    u->track = S.cur; u->n_tracks = S.n_tracks; u->kind = tr->kind;
    snprintf(u->track_name, sizeof u->track_name, "%d %s", S.cur + 1, KIND_NAMES[tr->kind]);
    char snd[12];
    looper_param_text(PR_SOUND, snd, sizeof snd);
    snprintf(u->sound, sizeof u->sound, "%s", snd);
    u->scene = S.scene; u->scene_next = S.scene_next; u->song_mode = S.song_mode; u->arr_pos = S.arr_pos;
    u->armed = S.armed; u->rec = S.rec; u->replace = S.replace_next; u->bpm = S.bpm;
    u->bars = S.bars; u->steps = S.steps; u->step = S.last_step < 0 ? 0 : S.last_step;
    u->beat = u->step / 4;
    u->beats_to_go = S.armed ? (S.loop_len - S.pos + (int)(S.step_len * 4) - 1) / (int)(S.step_len * 4) : 0;
    for (int t = 0; t < TRACK_N; t++) { u->tr[t].kind = S.tr[t].kind; u->tr[t].has = S.tr[t].has[pat_of(t, S.scene)] || S.tr[t].has[S.scene]; u->tr[t].mute = S.tr[t].mute; u->tr[t].solo = S.tr[t].solo; }
    scale_name(&S.scale, u->key, sizeof u->key);
    u->live_note = tr->live_note; u->last_drum = S.last_drum; u->drum_lo = S.drum_lo; u->drum_hi = S.drum_hi;
    memcpy(u->msg, S.msg, sizeof u->msg);
    u->level = S.peak; S.peak = 0;
    u->gr = mix_gain_reduction();
    u->bounce = S.bounce_on;
    u->can_undo = S.bk_track >= 0;
}

const uint8_t *looper_drum_pattern(void) { return &S.drum[S.cur][pat_of(S.cur, S.scene)][0][0]; }
const uint8_t *looper_seq(void) { return S.seq[S.cur][pat_of(S.cur, S.scene)]; }

// ---- persistence
int looper_take_dirty(void) { int d = S.dirty; S.dirty = 0; return d; }

void looper_get_state(song_state_t *st)
{
    memset(st, 0, sizeof *st);
    st->magic = SONG_MAGIC;
    st->bpm10 = (uint16_t)(S.bpm * 10 + 0.5f); st->bars = S.bars; st->cur_track = S.cur; st->scene = S.scene;
    st->count_in = S.count_in; st->metro = S.metro; st->quant8 = S.quant8; st->swing = S.swing; st->human = S.human;
    st->pump = (uint8_t)(mix_params()->pump * 100 + 0.5f); st->drive10 = (uint8_t)(mix_params()->drive * 10 + 0.5f);
    st->mtone = (int8_t)(mix_params()->tone * 100); st->kit = S.kit; st->key = S.key; st->mic_gain = S.mic_gain; st->gate_db = S.gate_db;
    st->n_tracks = S.n_tracks; st->root = S.scale.root; st->minor = S.scale.minor; st->scale_locked = S.scale.locked;
    st->song_mode = S.song_mode; st->arr_rep = S.arr_rep; memcpy(st->arr, S.arr, sizeof st->arr);
    for (int t = 0; t < TRACK_N; t++) {
        track_t *tr = &S.tr[t];
        st->tr[t].kind = tr->kind; st->tr[t].sound = tr->sound; st->tr[t].mute = tr->mute; st->tr[t].solo = tr->solo;
        st->tr[t].vol = tr->vol; st->tr[t].rev = tr->rev; st->tr[t].dly = tr->dly; st->tr[t].lowcut = tr->lowcut;
        st->tr[t].pan = tr->pan; st->tr[t].tone = tr->tone;
        st->tr[t].octave = tr->octave; st->tr[t].fx = tr->fx; st->tr[t].fx_amt = tr->fx_amt;
        for (int p = 0; p < PAT_N; p++) st->tr[t].has[p] = tr->has[p];
    }
    st->stability = S.stability;
    memcpy(st->drum, S.drum, sizeof st->drum);
    memcpy(st->seq, S.seq, sizeof st->seq);
}

void looper_set_state(const song_state_t *st)
{
    if (st->magic != SONG_MAGIC) return;
    S.armed = false; S.rec = false;
    for (int i = 0; i < VOCAL_SLOTS; i++) { S.vowner[i][0] = -1; S.vowner[i][1] = -1; }
    set_grid(st->bpm10 / 10.0f, clampi(st->bars, 1, LOOP_MAX_BARS));
    S.count_in = clampi(st->count_in, 1, 2); S.metro = st->metro; S.quant8 = st->quant8; S.swing = st->swing; S.human = st->human;
    mix_params()->pump = st->pump * 0.01f; mix_params()->drive = clampf(st->drive10 * 0.1f, 1, 4); mix_params()->tone = st->mtone * 0.01f;
    S.kit = st->kit; S.key = clampi(st->key, 0, 24);
    S.mic_gain = clampi(st->mic_gain ? st->mic_gain : CONFIG_KIT_MIC_GAIN, 1, 64); audio_set_mic_gain(S.mic_gain);
    S.gate_db = st->gate_db ? st->gate_db : CONFIG_KIT_GATE_DB; voice_set_gate_db(S.gate_db);
    S.n_tracks = clampi(st->n_tracks, 1, TRACK_N);
    S.stability = clampi(st->stability, 0, 2); voice_set_stability(S.stability);
    S.scale.root = st->root; S.scale.minor = st->minor; S.scale.locked = st->scale_locked; apply_key();
    S.song_mode = st->song_mode; S.arr_rep = st->arr_rep ? st->arr_rep : 2; memcpy(S.arr, st->arr, sizeof S.arr);
    S.scene = clampi(st->scene, 0, PAT_N - 1); S.scene_next = -1; S.arr_pos = 0; S.arr_loops = 0;
    for (int t = 0; t < TRACK_N; t++) {
        track_t *tr = &S.tr[t];
        track_new(t, st->tr[t].kind < K_N ? st->tr[t].kind : K_LEAD);
        tr->sound = st->tr[t].sound; track_setup(tr, tr->kind, tr->sound);
        tr->mute = st->tr[t].mute; tr->solo = st->tr[t].solo;
        tr->vol = st->tr[t].vol; tr->rev = st->tr[t].rev; tr->dly = st->tr[t].dly; tr->lowcut = st->tr[t].lowcut;
        tr->pan = st->tr[t].pan; tr->tone = st->tr[t].tone;
        tr->octave = clampi(st->tr[t].octave, -2, 2); tr->fx = st->tr[t].fx < FX_N ? st->tr[t].fx : 0; tr->fx_amt = clampi(st->tr[t].fx_amt, 0, 100);
        for (int p = 0; p < PAT_N; p++) tr->has[p] = tr->kind == K_VOCAL ? false : st->tr[t].has[p];
    }
    memcpy(S.drum, st->drum, sizeof S.drum);
    memcpy(S.seq, st->seq, sizeof S.seq);
    S.cur = clampi(st->cur_track, 0, S.n_tracks - 1);
    if (cur()->kind == K_VOCAL) tune_set_mode(cur()->sound);
    S.bk_track = -1;
    S.dirty = 0;
}

int looper_loop_len(void) { return S.loop_len; }
int looper_vocal_slot(int t, int p) { return vslot_find(t, p); }
uint8_t *looper_vocal_data(int slot) { return slot >= 0 && slot < VOCAL_SLOTS ? S.vpool[slot] : NULL; }
int looper_vocal_alloc(int t, int p) { return vslot_alloc(t, p); }
void looper_vocal_loaded(int t, int p) { if (t < TRACK_N && p < PAT_N) S.tr[t].has[p] = true; }
void looper_set_clock_cb(void (*cb)(void)) { S.clock_cb = cb; }
int  looper_vocal_take_dirty(void) { int m = S.vdirty; S.vdirty = 0; return m; }
void looper_vocal_owner(int slot, int *t, int *p) { *t = S.vowner[slot][0]; *p = S.vowner[slot][1]; }
int  looper_track_kind(int t) { return S.tr[t].kind; }
bool looper_track_has(int t, int p) { return S.tr[t].has[p]; }

bool looper_bounce_start(int first)
{
    if (S.bounce_on) return false;
    for (int k = 0; k < 3; k++) {
        S.bounce[k] = heap_caps_malloc(S.loop_len * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (!S.bounce[k]) { looper_bounce_release(); return false; }
    }
    S.bounce_done = false; S.bounce_running = false; S.bounce_first = first; S.bounce_on = true;
    return true;
}
bool looper_bounce_done(void) { return S.bounce_done; }
int16_t *looper_bounce_buf(int i) { return S.bounce[i]; }
void looper_bounce_release(void)
{
    for (int k = 0; k < 3; k++) { if (S.bounce[k]) free(S.bounce[k]); S.bounce[k] = NULL; }
    S.bounce_on = false; S.bounce_done = false; S.bounce_running = false;
}
