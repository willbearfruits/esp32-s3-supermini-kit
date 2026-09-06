set(srcs main.c oled.c dsp.c voice.c audio.c selftest.c usb_midi.c fx_list.c)
if(CONFIG_KIT_APP_LOOPER)
    list(APPEND srcs app_looper.c looper.c scale.c synth.c drums.c tune.c)
else()
    list(APPEND srcs app_instrument.c fx_test.c fx_vocoder.c fx_autotune.c fx_guitar.c
                     fx_delay.c fx_reverb.c fx_stutter.c)
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_timer esp_lcd esp_system)
