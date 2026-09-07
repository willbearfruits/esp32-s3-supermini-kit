#include "usbmode.h"
#include "looper.h"
#include "dsp.h"
#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "class/midi/midi_device.h"

static const char *TAG = "usb";
static bool active;

enum { ITF_MSC, ITF_MIDI, ITF_MIDI_STREAM, ITF_TOTAL };
#define EP_MSC_OUT  0x01
#define EP_MSC_IN   0x81
#define EP_MIDI_OUT 0x02
#define EP_MIDI_IN  0x82

static const tusb_desc_device_t dev_desc = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC, .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = 64, .idVendor = 0x303A, .idProduct = 0x4010, .bcdDevice = 0x0100,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
static const char *strings[] = { (const char[]){ 0x09, 0x04 }, "ESP32-S3 Kit", "Kit Voice Looper", "0001", "Kit drive", "Kit MIDI" };
#define CFG_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN + TUD_MIDI_DESC_LEN)
static const uint8_t cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, CFG_LEN, 0, 100),
    TUD_MSC_DESCRIPTOR(ITF_MSC, 4, EP_MSC_OUT, EP_MSC_IN, 64),
    TUD_MIDI_DESCRIPTOR(ITF_MIDI, 5, EP_MIDI_OUT, EP_MIDI_IN, 64),
};

bool usbmode_requested(void)
{
    nvs_handle_t h; uint8_t v = 0;
    if (nvs_open("kit", NVS_READONLY, &h) != ESP_OK) return false;
    nvs_get_u8(h, "usb", &v);
    nvs_close(h);
    return v == 1;
}

void usbmode_request(bool on)
{
    nvs_handle_t h;
    if (nvs_open("kit", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "usb", on ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

bool usbmode_active(void) { return active; }
bool usbmode_host_connected(void) { return active && tud_mounted(); }

static void midi_clock(void)
{
    if (tud_midi_mounted()) { uint8_t m = 0xF8; tud_midi_stream_write(0, &m, 1); }
}

static void midi_rx_task(void *arg)
{
    uint8_t pk[4];
    while (1) {
        while (tud_midi_available()) {
            if (tud_midi_packet_read(pk)) {
                uint8_t st = pk[1] & 0xF0;
                if (st == 0x90 && pk[3]) looper_midi_note(pk[2], pk[3], true);
                else if (st == 0x80 || st == 0x90) looper_midi_note(pk[2], 0, false);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void usbmode_start(wl_handle_t wl)
{
    tinyusb_msc_spiflash_config_t msc = { .wl_handle = wl, .mount_config = { .max_files = 4, .format_if_mount_failed = false, .allocation_unit_size = 4096 } };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&msc));
    tinyusb_config_t cfg = {
        .device_descriptor = &dev_desc, .string_descriptor = strings,
        .string_descriptor_count = sizeof strings / sizeof strings[0],
        .external_phy = false, .configuration_descriptor = cfg_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
    active = true;
    looper_set_clock_cb(midi_clock);
    xTaskCreate(midi_rx_task, "midirx", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "USB drive + MIDI up");
}

void usbmode_track_voice(const voice_t *v)
{
    static int note = -1; static int hold;
    if (!active || !tud_midi_mounted()) return;
    int want = v->voiced ? v->note : -1;
    if (want != note) {
        if (want < 0 && ++hold < 6) return;           // 12 ms of silence before a note-off
        hold = 0;
        if (note >= 0) { uint8_t m[3] = { 0x80, note, 0 }; tud_midi_stream_write(0, m, 3); }
        if (want >= 0) {
            int vel = (int)((v->db + 50) * 3); vel = vel < 20 ? 20 : (vel > 127 ? 127 : vel);
            uint8_t m[3] = { 0x90, want, vel }; tud_midi_stream_write(0, m, 3);
        }
        note = want;
    } else hold = 0;
    if (note >= 0) {
        int bend = 8192 + (int)((v->note_f - note) * 4096);
        bend = bend < 0 ? 0 : (bend > 16383 ? 16383 : bend);
        uint8_t m[3] = { 0xE0, bend & 0x7F, bend >> 7 }; tud_midi_stream_write(0, m, 3);
    }
}
