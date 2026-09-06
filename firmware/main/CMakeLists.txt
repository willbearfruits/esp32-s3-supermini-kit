idf_component_register(SRCS "main.c" "oled.c" "dsp.c" "voice.c" "audio.c"
                            "selftest.c" "usb_midi.c" "fx_list.c"
                            "fx_test.c" "fx_vocoder.c" "fx_autotune.c" "fx_guitar.c"
                            "fx_delay.c" "fx_reverb.c" "fx_stutter.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_timer esp_lcd esp_system)
