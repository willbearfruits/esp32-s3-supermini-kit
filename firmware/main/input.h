// Physical inputs: encoder (turn, push), joystick (tilt, click, flicks),
// and the BOOT button standing in for the encoder push until one is wired.
#pragma once
#include <stdbool.h>

typedef struct {
    int   enc;                 // detents turned since last poll, signed
    bool  tap, hold, longp;    // push released < 0.5 s; held 0.6 s; held 3 s (once)
    bool  pressed; int held_ms;
    bool  from_boot;           // the current/last press started on BOOT rather than the encoder switch
    bool  joy_click;
    bool  flick_l, flick_r, flick_u, flick_d;
    float jx, jy;              // -1..1 with dead zone, 0 when no joystick
    bool  has_joy;
} input_ev_t;

void input_init(void);
void input_poll(input_ev_t *ev);   // call every ~10 ms
bool input_encoder_held_now(void); // raw, for the boot-time USB mode check
