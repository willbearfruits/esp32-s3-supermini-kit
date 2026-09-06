// Musical scale: detection from a pitch-class histogram, snapping, triads.
#pragma once
#include <stdbool.h>

typedef struct {
    int  root;      // pitch class 0..11 (C = 0)
    bool minor;     // natural minor, else major
    bool locked;    // when false, snapping is chromatic
} scale_t;

int  scale_snap(const scale_t *s, float note_f);          // nearest scale note
bool scale_contains(const scale_t *s, int note);
void scale_triad(const scale_t *s, int root_note, int out[3]); // diatonic triad
// Pick the key that best explains the weights (12 pitch classes). Returns
// false if there is too little evidence.
bool scale_detect(const float w[12], scale_t *s);
const char *scale_name(const scale_t *s, char *buf, int len); // "A min", "C maj", "free"
