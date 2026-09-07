// Option C: beatbox cup on a wand. The mic sits at the bottom of a cup you
// press to your mouth, so it hears you 15-20 dB above the room and nothing
// from the speaker. The cup is a Helmholtz resonator only when the lips seal
// it; the echo below prints where it sits (mid-vowel range, not kick range,
// see docs/acoustics.md). The handle carries, from the cup outwards, the
// joystick under the thumb, the OLED, the encoder, then the ESP with USB-C
// out of the end. Open underneath; a flat lid closes it.
include <parts.scad>
include <acoustics.scad>

show_parts = true;
wall = 2.0;
cup_d = 44; cup_depth = 28;            // inner
hnd_w = 34; hnd_h = 20; hnd_l = 130; hnd_r = 6;

echo(str("cup sealed by the lips: ~", round(helmholtz_f(PI*pow(cup_d/2,2)*cup_depth, PI*pow(8,2), 15, 16)), " Hz through a 16 mm lip gap; open cup: quarter-wave ", round(tube_closed_f(cup_depth)), " Hz"));

module cup() {
    difference() {
        union() {
            cylinder(d=cup_d + 2*wall, h=cup_depth + wall);
            // mic pocket on the back of the cup
            translate([-mic_w/2 - 2, 12 - mic_l/2 - 2, -mic_t - wall]) cube([mic_w + 4, mic_l + 4, mic_t + wall]);
        }
        translate([0, 0, wall]) cylinder(d=cup_d, h=cup_depth + 1);
        // mic port in the cup floor, off-centre so the OLED can sit behind
        translate([0, 12, -1]) cylinder(d=mic_hole_d, h=wall + 2);
        translate([-mic_w/2, 12 - mic_l/2, -mic_t - wall - 1]) cube([mic_w, mic_l, mic_t + 1]);
        // drain / pressure slots so the seal does not pop the mic
        for (r = [0 : 60 : 300]) rotate([0, 0, r]) translate([cup_d/2 - 1, -0.5, cup_depth - 6]) cube([wall + 2, 1, 4]);
    }
    if (show_parts && $preview) %translate([-mic_port_xy[0], 12 - mic_port_xy[1], -mic_t]) inmp441();
}

joy_y = hnd_l - 18; oled_y = hnd_l - 64; enc_y = hnd_l - 84;
module handle() {
    translate([-hnd_w/2, -cup_d/2 - hnd_l, -hnd_h]) difference() {
        rounded_box([hnd_w, hnd_l + 10, hnd_h], hnd_r);
        translate([wall, wall, -eps]) rounded_box([hnd_w - 2*wall, hnd_l + 10 - 2*wall, hnd_h - wall], hnd_r - wall);
        translate([hnd_w/2, joy_y, hnd_h - wall - joy_t - joy_base_h + 1.5]) joystick(cut=true);
        translate([hnd_w/2 - oled_w/2, oled_y, hnd_h - wall - oled_t - oled_glass_t]) oled_096(cut=true);
        translate([hnd_w/2, enc_y, hnd_h - wall - enc_body_h]) encoder(cut=true);
        translate([hnd_w/2 - esp_w/2, 1, 3]) rotate([0, 0, 180]) translate([-esp_w, -esp_l, 0]) esp32_zero(cut=true);
    }
    if (show_parts && $preview) translate([-hnd_w/2, -cup_d/2 - hnd_l, -hnd_h]) {
        %translate([hnd_w/2, joy_y, hnd_h - wall - joy_t - joy_base_h + 1.5]) joystick();
        %translate([hnd_w/2 - oled_w/2, oled_y, hnd_h - wall - oled_t - oled_glass_t]) oled_096();
        %translate([hnd_w/2, enc_y, hnd_h - wall - enc_body_h]) encoder();
    }
}

cup();
handle();
