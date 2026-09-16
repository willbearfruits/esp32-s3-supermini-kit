set(srcs main.c oled.c dsp.c voice.c audio.c selftest.c fx_list.c)
if(CONFIG_KIT_APP_LOOPER)
    list(APPEND srcs app_looper.c looper.c scale.c synth.c kit.c mix.c tune.c
                     input.c imu.c tof.c song.c usbmode.c)
elseif(CONFIG_KIT_APP_TEST)
    list(APPEND srcs app_test.c fx_sine.c fx_delay.c fx_reverb.c fx_stutter.c input.c)
elseif(CONFIG_KIT_APP_JAM)
    list(APPEND srcs app_jam.c jam.c kit.c synth.c mix.c input.c faust_voice.cpp)
elseif(CONFIG_KIT_APP_FAUST)
    list(APPEND srcs app_test.c fx_sine.c fx_faust.cpp input.c)
else()
    list(APPEND srcs app_instrument.c fx_test.c fx_vocoder.c fx_autotune.c fx_guitar.c
                     fx_delay.c fx_reverb.c fx_stutter.c)
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_timer esp_lcd esp_system nvs_flash fatfs wear_levelling esp_adc)
# The DSP is plain C loops; let the compiler go all in on them.
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -ffast-math -fno-math-errno)
