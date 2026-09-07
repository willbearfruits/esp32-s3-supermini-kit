#include "song.h"
#include "looper.h"
#include "kit.h"
#include "scale.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "song";
static wl_handle_t wl = WL_INVALID_HANDLE;
static bool mounted, saving;
static int slot = 1;
static volatile int export_pct = -1;
static char export_msg[40] = "";

static void slot_dir(char *buf, int len, int n) { snprintf(buf, len, STORAGE_BASE "/songs/%d", n); }
static void vocal_path(char *buf, int len, int n, int t, int p) { snprintf(buf, len, STORAGE_BASE "/songs/%d/v%d_%d.ul", n, t, p); }

bool song_init(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); e = nvs_flash_init(); }
    ESP_ERROR_CHECK(e);
    nvs_handle_t h;
    if (nvs_open("kit", NVS_READONLY, &h) == ESP_OK) { uint8_t v = 1; nvs_get_u8(h, "slot", &v); slot = v >= 1 && v <= SONG_SLOTS ? v : 1; nvs_close(h); }
    esp_vfs_fat_mount_config_t mc = { .max_files = 4, .format_if_mount_failed = true, .allocation_unit_size = 4096 };
    e = esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_BASE, "storage", &mc, &wl);
    if (e != ESP_OK) { ESP_LOGE(TAG, "storage mount failed: %s", esp_err_to_name(e)); return false; }
    mounted = true;
    mkdir(STORAGE_BASE "/songs", 0777);
    mkdir(STORAGE_BASE "/kits", 0777);
    mkdir(STORAGE_BASE "/kits/user", 0777);
    mkdir(STORAGE_BASE "/export", 0777);
    int got = kit_load_user(STORAGE_BASE "/kits/user");
    uint64_t total = 0, freeb = 0;
    esp_vfs_fat_info(STORAGE_BASE, &total, &freeb);
    ESP_LOGI(TAG, "storage %llu KB, %llu KB free, slot %d, user kit %d/3 files", total / 1024, freeb / 1024, slot, got);
    return true;
}

int song_slot(void) { return slot; }
wl_handle_t song_wl(void) { return wl; }
void song_unmount(void) { if (mounted) { esp_vfs_fat_spiflash_unmount_rw_wl(STORAGE_BASE, wl); mounted = false; } }

static void set_slot(int n)
{
    slot = n;
    nvs_handle_t h;
    if (nvs_open("kit", NVS_READWRITE, &h) == ESP_OK) { nvs_set_u8(h, "slot", n); nvs_commit(h); nvs_close(h); }
}

bool song_load(int n)
{
    if (!mounted) return false;
    char path[64];
    slot_dir(path, sizeof path, n);
    strlcat(path, "/state.bin", sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) { ESP_LOGI(TAG, "slot %d empty", n); set_slot(n); return false; }
    song_state_t *st = heap_caps_malloc(sizeof *st, MALLOC_CAP_SPIRAM);
    size_t got = st ? fread(st, 1, sizeof *st, f) : 0;
    fclose(f);
    if (got != sizeof *st || st->magic != SONG_MAGIC) { ESP_LOGW(TAG, "slot %d unreadable", n); free(st); set_slot(n); return false; }
    looper_set_state(st);
    int len = looper_loop_len();
    for (int t = 0; t < TRACK_N; t++) {
        if (st->tr[t].kind != K_VOCAL) continue;
        for (int p = 0; p < PAT_N; p++) {
            if (!st->tr[t].has[p]) continue;
            vocal_path(path, sizeof path, n, t, p);
            FILE *v = fopen(path, "rb");
            if (!v) continue;
            int s = looper_vocal_alloc(t, p);
            if (s >= 0) {
                int r = fread(looper_vocal_data(s), 1, len, v);
                if (r == len) looper_vocal_loaded(t, p);
            }
            fclose(v);
        }
    }
    free(st);
    set_slot(n);
    ESP_LOGI(TAG, "slot %d loaded", n);
    return true;
}

static void write_state(int n)
{
    char path[64];
    slot_dir(path, sizeof path, n);
    mkdir(path, 0777);
    strlcat(path, "/state.bin", sizeof path);
    song_state_t *st = heap_caps_malloc(sizeof *st, MALLOC_CAP_SPIRAM);
    if (!st) return;
    looper_get_state(st);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(st, 1, sizeof *st, f); fclose(f); }
    else ESP_LOGE(TAG, "cannot write %s", path);
    free(st);
}

void song_save_state(void) { if (mounted) write_state(slot); }

static void write_vocals(int n, int mask, bool all)
{
    char path[64];
    for (int s = 0; s < VOCAL_SLOTS; s++) {
        int t, p;
        looper_vocal_owner(s, &t, &p);
        if (t < 0 || !(all || (mask & (1 << s)))) continue;
        vocal_path(path, sizeof path, n, t, p);
        FILE *f = fopen(path, "wb");
        if (!f) continue;
        int len = looper_loop_len();
        const uint8_t *b = looper_vocal_data(s);
        for (int i = 0; i < len; i += 8192) { fwrite(b + i, 1, len - i < 8192 ? len - i : 8192, f); vTaskDelay(1); }
        fclose(f);
    }
    // patterns that no longer exist lose their file
    for (int t = 0; t < TRACK_N; t++) for (int p = 0; p < PAT_N; p++) {
        if (looper_track_kind(t) == K_VOCAL && looper_track_has(t, p)) continue;
        vocal_path(path, sizeof path, n, t, p);
        unlink(path);
    }
}

static void save_vocal_task(void *arg)
{
    int mask = (int)(intptr_t)arg;
    write_vocals(slot, mask, false);
    ESP_LOGI(TAG, "vocals saved (mask %x)", mask);
    saving = false;
    vTaskDelete(NULL);
}

void song_save_vocal(void)
{
    if (!mounted || saving) return;
    int mask = looper_vocal_take_dirty();
    saving = true;
    xTaskCreate(save_vocal_task, "vsave", 4096, (void *)(intptr_t)mask, 2, NULL);
}

void song_save_to(int n)
{
    if (!mounted) return;
    write_state(n);
    write_vocals(n, 0, true);
    looper_vocal_take_dirty();
    set_slot(n);
}

bool song_saving(void) { return saving; }

// ---- export
static void wav_write(const char *name, const int16_t *a, const int16_t *b, int n)
{
    char path[64];
    snprintf(path, sizeof path, STORAGE_BASE "/export/%s.wav", name);
    FILE *f = fopen(path, "wb");
    if (!f) { ESP_LOGE(TAG, "cannot write %s", path); return; }
    int chn = b ? 2 : 1;
    uint32_t rate = CONFIG_KIT_SAMPLE_RATE, bytes = n * 2 * chn, brate = rate * 2 * chn;
    uint8_t h[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 1,0, chn,0,
                      0,0,0,0, 0,0,0,0, 2 * chn,0, 16,0, 'd','a','t','a', 0,0,0,0 };
    uint32_t riff = 36 + bytes;
    memcpy(h + 4, &riff, 4); memcpy(h + 24, &rate, 4); memcpy(h + 28, &brate, 4); memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f);
    if (!b) for (int i = 0; i < n; i += 4096) { fwrite(a + i, 2, n - i < 4096 ? n - i : 4096, f); vTaskDelay(1); }
    else {
        static int16_t il[2048];
        for (int i = 0; i < n; i += 1024) {
            int m = n - i < 1024 ? n - i : 1024;
            for (int k = 0; k < m; k++) { il[2 * k] = a[i + k]; il[2 * k + 1] = b[i + k]; }
            fwrite(il, 4, m, f); vTaskDelay(1);
        }
    }
    fclose(f);
}

typedef struct { uint32_t tick; uint8_t st, d1, d2; } mev_t;
static int mev_cmp(const void *a, const void *b)
{
    const mev_t *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return (x->st & 0xF0) == 0x80 ? -1 : 1;
}
static void vlq(FILE *f, uint32_t v)
{
    uint8_t buf[4]; int n = 0;
    buf[n++] = v & 0x7F;
    while (v >>= 7) buf[n++] = 0x80 | (v & 0x7F);
    while (n--) fputc(buf[n], f);
}

static void midi_write(const song_state_t *st)
{
    static mev_t ev[3000];
    int n = 0, steps = st->bars * 16;
    const int T = 24;
    scale_t sc = { .root = st->root, .minor = st->minor, .locked = st->scale_locked };
    for (int t = 0; t < st->n_tracks; t++) {
        int p = st->tr[t].has[st->scene] ? st->scene : 0;
        if (st->tr[t].kind == K_DRUMS) {
            for (int s = 0; s < steps && n < 2990; s++)
                for (int d = 0; d < DRUM_N; d++) if (st->drum[t][p][s][d]) {
                    uint8_t note = d == DRUM_KICK ? 36 : (d == DRUM_SNARE ? 38 : 42);
                    ev[n++] = (mev_t){ s * T, 0x99, note, st->drum[t][p][s][d] };
                    ev[n++] = (mev_t){ s * T + T / 2, 0x89, note, 0 };
                }
        } else if (st->tr[t].kind != K_VOCAL) {
            int chn = t == 9 ? 10 : t;
            int held[3] = { -1, -1, -1 }, nheld = 0;
            for (int s = 0; s <= steps && n < 2990; s++) {
                uint8_t b = s < steps ? st->seq[t][p][s] : 0;
                int note = b & 0x7F;
                bool end = !note || (b & SEQ_ATTACK);
                if (end && nheld) { for (int i = 0; i < nheld; i++) ev[n++] = (mev_t){ s * T, 0x80 | chn, held[i], 0 }; nheld = 0; }
                if (note && (b & SEQ_ATTACK)) {
                    if (st->tr[t].kind == K_KEYS) { int tri[3]; scale_triad(&sc, note, tri); for (int i = 0; i < 3; i++) held[i] = tri[i]; nheld = 3; }
                    else { held[0] = note; nheld = 1; }
                    for (int i = 0; i < nheld; i++) ev[n++] = (mev_t){ s * T, 0x90 | chn, held[i], 100 };
                }
            }
        }
    }
    qsort(ev, n, sizeof ev[0], mev_cmp);
    FILE *f = fopen(STORAGE_BASE "/export/song.mid", "wb");
    if (!f) return;
    uint8_t hdr[14] = { 'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96 };
    fwrite(hdr, 1, 14, f);
    fwrite("MTrk\0\0\0\0", 1, 8, f);
    long len_pos = ftell(f) - 4;
    uint32_t us = (uint32_t)(60000000.0f / (st->bpm10 / 10.0f));
    uint8_t tempo[7] = { 0x00, 0xFF, 0x51, 0x03, us >> 16, us >> 8, us };
    fwrite(tempo, 1, 7, f);
    uint32_t last = 0;
    for (int i = 0; i < n; i++) {
        vlq(f, ev[i].tick - last); last = ev[i].tick;
        fputc(ev[i].st, f); fputc(ev[i].d1, f); fputc(ev[i].d2, f);
    }
    vlq(f, steps * T - last);
    fwrite("\xFF\x2F\x00", 1, 3, f);
    long end = ftell(f);
    uint32_t tl = end - len_pos - 4;
    uint8_t lb[4] = { tl >> 24, tl >> 16, tl >> 8, tl };
    fseek(f, len_pos, SEEK_SET); fwrite(lb, 1, 4, f);
    fclose(f);
}

static void export_task(void *arg)
{
    song_state_t *st = heap_caps_malloc(sizeof *st, MALLOC_CAP_SPIRAM);
    if (!st) { export_pct = 100; vTaskDelete(NULL); }
    looper_get_state(st);
    int nt = st->n_tracks, passes = (nt + 2) / 3 + 1;
    for (int pass = 0; pass < passes; pass++) {
        int first = pass * 3;
        bool master = first >= nt;
        snprintf(export_msg, sizeof export_msg, "bounce %d/%d", pass + 1, passes);
        if (!looper_bounce_start(master ? nt : first)) { snprintf(export_msg, sizeof export_msg, "no memory"); break; }
        int waited = 0;
        while (!looper_bounce_done() && waited < 40000) { vTaskDelay(pdMS_TO_TICKS(20)); waited += 20; export_pct = (pass * 100 + waited * 60 / 40000) / passes; }
        if (master) { snprintf(export_msg, sizeof export_msg, "write mix"); wav_write("mix", looper_bounce_buf(0), looper_bounce_buf(1), looper_loop_len()); }
        else for (int k = 0; k < 3 && first + k < nt; k++) {
            char name[24];
            snprintf(name, sizeof name, "t%d_%s", first + k + 1, looper_kind_name(st->tr[first + k].kind));
            for (char *c = name; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
            snprintf(export_msg, sizeof export_msg, "write %s", name);
            wav_write(name, looper_bounce_buf(k), NULL, looper_loop_len());
            export_pct = (pass * 100 + 60 + (k + 1) * 13) / passes;
        }
        looper_bounce_release();
    }
    snprintf(export_msg, sizeof export_msg, "write midi");
    midi_write(st);
    FILE *f = fopen(STORAGE_BASE "/export/song.txt", "w");
    if (f) {
        char key[10];
        scale_t sc = { .root = st->root, .minor = st->minor, .locked = st->scale_locked };
        fprintf(f, "tempo %d bpm\nbars %d\nkey %s\nscene %c\nrate %d Hz\nstems: one loop per track, mix.wav is the stereo master\nmidi: song.mid, drum tracks on channel 10, others on their track channel\n",
                st->bpm10 / 10, st->bars, scale_name(&sc, key, sizeof key), 'A' + st->scene, CONFIG_KIT_SAMPLE_RATE);
        fclose(f);
    }
    free(st);
    snprintf(export_msg, sizeof export_msg, "exported");
    export_pct = 100;
    ESP_LOGI(TAG, "export done");
    vTaskDelete(NULL);
}

void song_export_start(void)
{
    if (!mounted || (export_pct >= 0 && export_pct < 100)) return;
    export_pct = 0;
    xTaskCreate(export_task, "export", 6144, NULL, 2, NULL);
}
int song_export_progress(void) { return export_pct; }
const char *song_export_status(void) { return export_msg; }
