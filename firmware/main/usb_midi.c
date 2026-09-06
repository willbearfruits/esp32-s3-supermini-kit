#include "usb_midi.h"
#include "audio.h"
#include "sdkconfig.h"

#ifdef CONFIG_KIT_USB_MIDI

#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s3/rom/usb/chip_usb_dw_wrapper.h"
#include "esp32s3/rom/usb/usb_persist.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

static const char *TAG = "usb";

// --- descriptors: CDC (2 interfaces) + MIDI (2 interfaces) -----------------

enum { ITF_CDC = 0, ITF_CDC_DATA, ITF_MIDI, ITF_MIDI_STREAMING, ITF_TOTAL };
enum { EP_CDC_NOTIF = 0x81, EP_CDC_OUT = 0x02, EP_CDC_IN = 0x82, EP_MIDI_OUT = 0x03, EP_MIDI_IN = 0x83 };

#define DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN)

static const uint8_t cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, DESC_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 4, EP_CDC_NOTIF, 8, EP_CDC_OUT, EP_CDC_IN, 64),
    TUD_MIDI_DESCRIPTOR(ITF_MIDI, 5, EP_MIDI_OUT, EP_MIDI_IN, 64),
};

static const char *str_desc[] = {
    (const char[]){ 0x09, 0x04 }, // English
    "ESP32-S3 Kit",               // 1 manufacturer
    "Kit Voice Instrument",       // 2 product
    "0001",                       // 3 serial
    "Kit console",                // 4 CDC
    "Kit MIDI",                   // 5 MIDI
};

// --- reset to bootloader on the esptool DTR/RTS dance -----------------------
// esptool toggles the lines one at a time, so the states it walks through are
//   (dtr,rts): (0,1) -> (1,1) -> (1,0) -> (0,0)   enter bootloader
//   (dtr,rts): (0,1) -> (0,0)                      plain reset
// Any other transition resets the detector.

static int seq;

static void line_state_cb(int itf, cdcacm_event_t *ev)
{
    bool dtr = ev->line_state_changed_data.dtr, rts = ev->line_state_changed_data.rts;
    if (!dtr && rts) { seq = 1; return; }
    if (seq == 1 && dtr && rts) { seq = 2; return; }
    if (seq == 1 && !dtr && !rts) { esp_restart(); }
    if ((seq == 2 || seq == 1) && dtr && !rts) {
        // Same recipe as IDF's own USB CDC console: keep the USB link up
        // across the reset and ask the ROM for download mode.
        chip_usb_set_persist_flags(USBDC_PERSIST_ENA);
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
    }
    seq = 0;
}

// --- MIDI out from the voice tracker -----------------------------------------

#define CH 0
static int  playing = -1;
static int  last_bend = 8192;
static int  bend_div;

static void send3(uint8_t a, uint8_t b, uint8_t c)
{
    if (!tud_midi_mounted()) return;
    uint8_t m[3] = { a, b, c };
    tud_midi_stream_write(0, m, 3);
}

static void note_off(void)
{
    if (playing >= 0) { send3(0x80 | CH, playing, 0); playing = -1; }
    if (last_bend != 8192) { send3(0xE0 | CH, 0, 64); last_bend = 8192; }
}

void usb_midi_track_voice(const voice_t *v)
{
    if (!tud_midi_mounted()) { playing = -1; return; }
    if (v->voiced && v->note >= 0) {
        if (v->note != playing) {
            int vel = (int)(127 + (v->db + 10) * 2.5f);
            if (vel < 30) vel = 30;
            if (vel > 127) vel = 127;
            if (playing >= 0) send3(0x80 | CH, playing, 0);
            send3(0x90 | CH, v->note, vel);
            playing = v->note;
        }
        // pitch bend, +-2 semitone range
        if (++bend_div >= 2) {
            bend_div = 0;
            int bend = 8192 + (int)((v->note_f - v->note) * 4096.0f);
            if (bend < 0) bend = 0;
            if (bend > 16383) bend = 16383;
            if (abs(bend - last_bend) > 24) {
                send3(0xE0 | CH, bend & 0x7F, bend >> 7);
                last_bend = bend;
            }
        }
    } else if (!v->gate) {
        note_off();
    }
}

void usb_midi_program_change(int program)
{
    if (!tud_midi_mounted()) return;
    uint8_t m[2] = { 0xC0 | CH, (uint8_t)(program & 0x7F) };
    tud_midi_stream_write(0, m, 2);
}

bool usb_midi_connected(void) { return tud_midi_mounted(); }

// --- MIDI in ------------------------------------------------------------------

static void midi_in_task(void *arg)
{
    uint8_t pkt[4];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2));
        while (tud_midi_available()) {
            if (!tud_midi_packet_read(pkt)) break;
            uint8_t st = pkt[1] & 0xF0;
            if (st == 0x90 && pkt[3] > 0) audio_midi_note_on(pkt[2], pkt[3]);
            else if (st == 0x80 || (st == 0x90 && pkt[3] == 0)) audio_midi_note_off(pkt[2]);
        }
    }
}

void usb_midi_init(void)
{
    tinyusb_config_t cfg = {
        .device_descriptor = NULL,
        .string_descriptor = str_desc,
        .string_descriptor_count = sizeof str_desc / sizeof str_desc[0],
        .external_phy = false,
        .configuration_descriptor = cfg_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));

    tinyusb_config_cdcacm_t acm = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_line_state_changed = line_state_cb,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm));
    esp_tusb_init_console(TINYUSB_CDC_ACM_0);

    xTaskCreate(midi_in_task, "midi_in", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "USB MIDI + console up");
}

#else // CONFIG_KIT_USB_MIDI: stubs, the USB port stays a Serial/JTAG console

void usb_midi_init(void) {}
bool usb_midi_connected(void) { return false; }
void usb_midi_track_voice(const voice_t *v) { (void)v; }
void usb_midi_program_change(int program) { (void)program; }

#endif
