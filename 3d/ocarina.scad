// Option B: ocarina body. The chamber is a Helmholtz resonator sized by the
// formula for a chosen lowest note; the mic listens inside the chamber, so
// the pitch tracker gets a loud, pure whistle tone instead of a voice.
// Four finger holes on top, thumb rests underneath, electronics bay below
// with the OLED, encoder and joystick on the bay's front lip.
//
// The formulas get within a semitone or so. Tune by enlarging holes
// (sharper) or partially taping them (flatter), like every ocarina maker.
include <parts.scad>
include <acoustics.scad>

part = "body";        // "body" | "bay"
show_parts = true;

wall = 2.5;
f_low = note_freq(69);                 // A4 = 440 Hz with all holes closed
win_w = 4; win_l = 12;                 // voicing window (the sound hole by the edge)
ww_h = 1.2; ww_l = 22;                 // windway slot height and length
holes_midi = [71, 73, 74, 76];         // B4 C#5 D5 E5, each measured alone

// chamber: ellipsoid with the volume the formula asks for, aspect ~ 2 : 1.3 : 1
cond_win = conductance(win_w * win_l, wall, circle_d(win_w * win_l));
V = helmholtz_V(f_low, win_w * win_l, wall, circle_d(win_w * win_l));
k = pow(V / (4/3 * PI * 2 * 1.3 * 1), 1/3);
a = 2 * k; b = 1.3 * k; c = k;         // semi-axes, mm
hole_d = [for (m = holes_midi) ocarina_hole_d(V, cond_win, note_freq(m), wall)];

echo(str("chamber volume ", round(V/1000), " cm3, inner ", round(2*a), " x ", round(2*b), " x ", round(2*c), " mm"));
echo(str("hole diameters (mm) ", [for (d = hole_d) round(d*10)/10], " for notes ", holes_midi));

module chamber_solid(g=0) { scale([a + g, b + g, c + g]) sphere(1, $fn=96); }

module body() {
    difference() {
        union() {
            chamber_solid(wall);
            // mouthpiece: flat tube out of the -x end, windway inside it
            translate([-a - wall - ww_l + 6, -win_l/2 - wall, -4]) cube([ww_l + 6, win_l + 2*wall, 4 + ww_h + wall]);
        }
        chamber_solid(0);
        // windway: slot from the mouthpiece tip to the window
        translate([-a - wall - ww_l, -win_l/2, 0]) cube([ww_l + wall + 2, win_l, ww_h]);
        // voicing window through the top wall, right where the windway ends
        translate([-a + 2, -win_l/2, -1]) cube([win_w, win_l, c + wall + 2]);
        // labium: 25 degree ramp on the far edge of the window
        translate([-a + 2 + win_w, -win_l/2 - eps, ww_h]) rotate([0, -25, 0]) translate([0, 0, -10]) cube([12, win_l + 2*eps, 10]);
        // finger holes across the top, right hand then left
        for (i = [0 : len(hole_d) - 1])
            translate([-a/2 + i * (a / (len(hole_d) - 0.5)), (i % 2 == 0 ? 6 : -6), 0]) cylinder(d=hole_d[i], h=c + wall + 1);
        // mic port into the chamber from below, the INMP441 pad is glued around it
        translate([a/3, 0, -c - wall - 1]) cylinder(d=mic_hole_d, h=wall + 2);
        // hole for the wires to the bay
        translate([a/3 + 8, 0, -c - wall - 1]) cylinder(d=4, h=wall + 2);
    }
}

// electronics bay: a shallow tray under the chamber; its +x end sticks out
// with the OLED on top, encoder beside it, joystick on the right rim.
bay_l = 2*a + 40; bay_w = 2*b + 6; bay_h = 22;
module bay() {
    difference() {
        translate([-a - 3, -bay_w/2, -c - wall - bay_h]) rounded_box([bay_l, bay_w, bay_h], 6);
        translate([-a - 3 + wall, -bay_w/2 + wall, -c - wall - bay_h + wall]) rounded_box([bay_l - 2*wall, bay_w - 2*wall, bay_h], 4);
        translate([a + 6, -oled_l/2, -c - wall - oled_t - oled_glass_t]) oled_096(cut=true);
        translate([a + 22, bay_w/2 - 10, -c - wall - enc_body_h]) encoder(cut=true);
        translate([a + 22, -bay_w/2 + 12, -c - wall - joy_t - joy_base_h + 1.5]) joystick(cut=true);
        translate([a + 37 - esp_l, -esp_w/2, -c - wall - bay_h + 3]) rotate([0, 0, 0]) esp32_zero(cut=true);
    }
    if (show_parts && $preview) {
        %translate([a + 6, -oled_l/2, -c - wall - oled_t - oled_glass_t]) oled_096();
        %translate([a + 22, bay_w/2 - 10, -c - wall - enc_body_h]) encoder();
        %translate([a + 22, -bay_w/2 + 12, -c - wall - joy_t - joy_base_h + 1.5]) joystick();
        %translate([a/3 - mic_port_xy[0], -mic_port_xy[1], -c - wall - mic_t]) inmp441();
    }
}

if (part == "body") body();
if (part == "bay") bay();
if (part == "both") { body(); bay(); }
