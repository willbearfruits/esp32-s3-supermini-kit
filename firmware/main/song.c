#include "song.h"
#include "looper.h"
#include "kit.h"
#include "scale.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "song";
static wl_handle_t wl = WL_INVALID_HANDLE;
static bool mounted, saving;
static volatile int export_pct = -1;
static char export_msg[24] = "";

bool song_init(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); e = nvs_flash_init(); }
    ESP_ERROR_CHECK(e);
    esp_vfs_fat_mount_config_t mc = { .max_files = 4, .format_if_mount_failed = true, .allocation_unit_size = 4096 };
    e = esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_BASE, "storage", &mc, &wl);
    if (e != ESP_OK) { ESP_LOGE(TAG, "storage mount failed: %s", esp_err_to_name(e)); return false; }
    mounted = true;
    mkdir(STORAGE_BASE "/song", 0777);
    mkdir(STORAGE_BASE "/kits", 0777);
    mkdir(STORAGE_BASE "/kits/user", 0777);
    mkdir(STORAGE_BASE "/export", 0777);
    int got = kit_load_user(STORAGE_BASE "/kits/user");
    uint64_t total = 0, freeb = 0;
    esp_vfs_fat_info(STORAGE_BASE, &total, &freeb);
    ESP_LOGI(TAG, "storage %llu KB, %llu KB free, user kit %d/3 files", total / 1024, freeb / 1024, got);
    return true;
}

wl_handle_t song_wl(void) { return wl; }
void song_unmount(void) { if (mounted) { esp_vfs_fat_spiflash_unmount_rw_wl(STORAGE_BASE, wl); mounted = false; } }

bool song_load(void)
{
    nvs_handle_t h;
    if (nvs_open("kit", NVS_READONLY, &h) != ESP_OK) return false;
    song_state_t st;
    size_t len = sizeof st;
    bool ok = nvs_get_blob(h, "song", &st, &len) == ESP_OK && len == sizeof st && st.magic == SONG_MAGIC;
    nvs_close(h);
    if (!ok) { ESP_LOGI(TAG, "no saved song"); return false; }
    looper_set_state(&st);
    if (st.layer[PG_VOCAL].has && mounted) {
        FILE *f = fopen(STORAGE_BASE "/song/vocal.ul", "rb");
        if (f) {
            int n = looper_loop_len();
            int got = fread(looper_vocal_buf(), 1, n, f);
            fclose(f);
            if (got == n) looper_vocal_loaded();
            ESP_LOGI(TAG, "vocal loop %d/%d bytes", got, n);
        }
    }
    ESP_LOGI(TAG, "song restored: %d bpm, %d bars", st.bpm10 / 10, st.bars);
    return true;
}

void song_save_state(void)
{
    song_state_t st;
    looper_get_state(&st);
    nvs_handle_t h;
    if (nvs_open("kit", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "song", &st, sizeof st);
    nvs_commit(h);
    nvs_close(h);
}

static void save_vocal_task(void *arg)
{
    FILE *f = fopen(STORAGE_BASE "/song/vocal.ul", "wb");
    if (f) {
        int n = looper_loop_len();
        const uint8_t *b = looper_vocal_buf();
        for (int i = 0; i < n; i += 8192) { fwrite(b + i, 1, n - i < 8192 ? n - i : 8192, f); vTaskDelay(1); }
        fclose(f);
        ESP_LOGI(TAG, "vocal loop saved");
    }
    saving = false;
    vTaskDelete(NULL);
}

void song_save_vocal(void)
{
    if (!mounted || saving) return;
    saving = true;
    xTaskCreate(save_vocal_task, "vsave", 4096, NULL, 2, NULL);
}

bool song_saving(void) { return saving; }

// ---- export
static void wav_write(const char *name, const int16_t *data, int n)
{
    char path[64];
    snprintf(path, sizeof path, STORAGE_BASE "/export/%s.wav", name);
    FILE *f = fopen(path, "wb");
    if (!f) { ESP_LOGE(TAG, "cannot write %s", path); return; }
    uint32_t rate = CONFIG_KIT_SAMPLE_RATE, bytes = n * 2, brate = rate * 2;
    uint8_t h[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
                      0,0,0,0, 0,0,0,0, 2,0, 16,0, 'd','a','t','a', 0,0,0,0 };
    uint32_t riff = 36 + bytes;
    memcpy(h + 4, &riff, 4); memcpy(h + 24, &rate, 4); memcpy(h + 28, &brate, 4); memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f);
    for (int i = 0; i < n; i += 4096) { fwrite(data + i, 2, n - i < 4096 ? n - i : 4096, f); vTaskDelay(1); }
    fclose(f);
}

typedef struct { uint32_t tick; uint8_t st, d1, d2; } mev_t;
static int mev_cmp(const void *a, const void *b)
{
    const mev_t *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return (x->st & 0xF0) == 0x80 ? -1 : 1;      // note-offs first at equal time
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
    static mev_t ev[2048];
    int n = 0, steps = st->bars * 16;
    const int T = 24;                                    // ticks per 16th at 96 ppq
    for (int s = 0; s < steps && n < 2000; s++)
        for (int t = 0; t < DRUM_N; t++) if (st->drum_pat[s][t]) {
            uint8_t note = t == DRUM_KICK ? 36 : (t == DRUM_SNARE ? 38 : 42);
            ev[n++] = (mev_t){ s * T, 0x99, note, st->drum_pat[s][t] };
            ev[n++] = (mev_t){ s * T + T / 2, 0x89, note, 0 };
        }
    scale_t sc = { .root = st->root, .minor = st->minor, .locked = st->scale_locked };
    for (int l = 0; l < 3; l++) {
        int held[3] = { -1, -1, -1 }, nheld = 0;
        for (int s = 0; s <= steps && n < 2000; s++) {
            uint8_t b = s < steps ? st->seq[l][s] : 0;
            int note = b & 0x7F;
            bool end = !note || (b & SEQ_ATTACK);
            if (end && nheld) { for (int i = 0; i < nheld; i++) ev[n++] = (mev_t){ s * T, 0x80 | l, held[i], 0 }; nheld = 0; }
            if (note && (b & SEQ_ATTACK)) {
                if (l == 1) { int tri[3]; scale_triad(&sc, note, tri); for (int i = 0; i < 3; i++) held[i] = tri[i]; nheld = 3; }
                else { held[0] = note; nheld = 1; }
                for (int i = 0; i < nheld; i++) ev[n++] = (mev_t){ s * T, 0x90 | l, held[i], 100 };
            }
        }
    }
    qsort(ev, n, sizeof ev[0], mev_cmp);
    FILE *f = fopen(STORAGE_BASE "/export/song.mid", "wb");
    if (!f) return;
    uint8_t hdr[14] = { 'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96 };
    fwrite(hdr, 1, 14, f);
    long len_pos;
    fwrite("MTrk\0\0\0\0", 1, 8, f); len_pos = ftell(f) - 4;
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
    static const char *names[2][3] = { { "drums", "bass", "chords" }, { "lead", "vocal", "mix" } };
    song_state_t st;
    looper_get_state(&st);
    for (int pass = 1; pass <= 2; pass++) {
        snprintf(export_msg, sizeof export_msg, "bounce %d/2", pass);
        if (!looper_bounce_start(pass)) { snprintf(export_msg, sizeof export_msg, "no memory"); export_pct = 100; vTaskDelete(NULL); }
        int waited = 0;
        while (!looper_bounce_done() && waited < 40000) { vTaskDelay(pdMS_TO_TICKS(20)); waited += 20; export_pct = (pass - 1) * 50 + (waited * 15) / 40000; }
        for (int k = 0; k < 3; k++) {
            snprintf(export_msg, sizeof export_msg, "write %s", names[pass - 1][k]);
            wav_write(names[pass - 1][k], looper_bounce_buf(k), looper_loop_len());
            export_pct = (pass - 1) * 50 + 15 + (k + 1) * 10;
        }
        looper_bounce_release();
    }
    snprintf(export_msg, sizeof export_msg, "write midi");
    midi_write(&st);
    FILE *f = fopen(STORAGE_BASE "/export/song.txt", "w");
    if (f) {
        char key[10];
        scale_t sc = { .root = st.root, .minor = st.minor, .locked = st.scale_locked };
        fprintf(f, "tempo %d bpm\nbars %d\nkey %s\nrate %d Hz\nstems: drums bass chords lead vocal mix (one loop each)\nmidi: song.mid, drums on channel 10\n",
                st.bpm10 / 10, st.bars, scale_name(&sc, key, sizeof key), CONFIG_KIT_SAMPLE_RATE);
        fclose(f);
    }
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
