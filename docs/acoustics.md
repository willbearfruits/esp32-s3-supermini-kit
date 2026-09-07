# Acoustics for the enclosure

What the box can and cannot do for a MEMS microphone, with the formulas the
OpenSCAD files use. Speed of sound c = 343 m/s.

## The one formula that matters: Helmholtz

A cavity of volume V with an opening of area A and neck length L resonates at

    f = c / (2 pi) * sqrt( A / (V * L_eff) ),   L_eff = L + 1.7 r

where r is the opening radius and the 1.7 r term is the end correction. This
is the ocarina, the cup, a ported speaker box and a bottle, all at once.

Turned around: the volume you need for a target frequency is

    V = A / ( L_eff * (2 pi f / c)^2 )

### Consequence 1: no handheld cavity reaches kick range

For 100 Hz with a 20 mm opening and a 20 mm neck you need 2.5 litres. For
200 Hz still 630 cm3. Anything that fits in a hand (50-150 cm3) resonates
between 400 and 900 Hz, the vowel range. So the enclosure cannot boost the
"b" of a beatbox kick. Kick versus hat separation stays electronic, which the
firmware already does with band energies.

### Consequence 2: it reaches whistle range easily

The same 81 cm3 chamber with a 4 x 12 mm window resonates at A4 = 440 Hz. That
is an ocarina, and it is the strongest acoustic idea for this kit: a fipple
turns breath into a loud, pure tone with a stable fundamental. The YIN
tracker locks onto it instantly, with none of the jitter a voice gives, and it
is 20 dB above room noise so the gate has nothing to think about. Voice and
beatbox still work on the other pages, the ocarina is just a very good pitch
controller for bass, chords and lead.

Finger holes add their own A / L_eff to the sum inside the square root, so
opening a hole raises the pitch. `ocarina.scad` solves the hole diameters for
a list of target notes; expect to be within a semitone and then tune by
enlarging (sharper) or taping (flatter), as every ocarina maker does.

## Tubes

A tube closed at one end resonates at odd multiples of c / 4L. A 15 cm tube
gives 572, 1715, 2858 Hz: a talkbox-like colour on the voice, nothing at the
fundamental. Fun, but it does not help tracking.

## What a cup does

The INMP441 is omnidirectional, so it has no proximity effect: there is no
bass boost from getting close. What a cup around the mic gives is isolation.
Lips on the rim, the voice arrives at the mic 15-20 dB above the room and the
speaker, which is exactly the "mic is too sensitive" complaint solved in
plastic. Sealed by the lips it becomes a Helmholtz resonator around 700 Hz
(printed by `cup.scad`), mild vowel colour, harmless for pitch.

## The microphone port

The INMP441 hears through a hole in its PCB. Mount the board face-down over a
1.5 mm port in the shell with a thin foam or rubber gasket so sound only
enters through the port. Keep the port short: a long narrow port plus the
tiny cavity in front of the membrane makes its own resonance, and below about
2 mm length and 1.5 mm diameter that sits above 10 kHz where it does not
matter. A slotted grille 5 mm in front of the port, with a thin foam disc,
is the pop filter.

## The speaker

A 40 mm 3 W driver in a sealed box needs volume to keep any bass: 26 cm3 (the
slab as drawn) is on the small side, 60-100 cm3 is comfortable. A port would
have to be large to tune below 200 Hz (see consequence 1), so stay sealed and
let the firmware make the bass. What matters more is feedback: the speaker
box must be its own sealed volume, separate from the mic side, with foam in
between, or the mic-to-speaker loop howls with the amp on.

## Gyroscope

Not acoustic, but worth deciding now: the MPU6050 goes near the centre of the
body, screwed rigidly to the shell, axes aligned with the body. Tilt then
maps cleanly to expression (vibrato, filter, pitch bend) without calibration
tricks.
