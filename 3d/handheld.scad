// Option A: handheld slab. OLED and controls on top, speaker in a sealed
// sub-box, mic on the top edge, USB-C at the bottom edge, battery under
// the lid. The safe, Game-Boy-shaped choice.
//
// Print: shell top-face down, lid flat. Renders shell + lid side by side;
// set part = "shell" or "lid" to export one STL.
include <parts.scad>
include <acoustics.scad>

part = "both";       // "shell" | "lid" | "both"
show_parts = true;   // draw component placeholders in preview

L = 118; W = 74; H = 26; R = 8; wall = 2.0;
lid_t = 2.0;

// placement (x, y from the shell corner, parts sit under the top plate)
oled_pos = [9, 40];
enc_pos  = [100, 56];
joy_pos  = [96, 22];
spk_pos  = [31, 24];
esp_pos  = [L/2 - esp_w/2, 1.5];              // USB-C out of the bottom wall
mic_pos  = [L/2, W];                          // port through the top edge wall
z_top_in = H - wall;                          // underside of the top plate

module placed_parts(cut=false) {
    translate([oled_pos[0], oled_pos[1], z_top_in - oled_t - oled_glass_t]) oled_096(cut);
    translate([enc_pos[0], enc_pos[1], z_top_in - enc_body_h]) encoder(cut);
    translate([joy_pos[0], joy_pos[1], z_top_in - joy_t - joy_base_h + 1.5]) joystick(cut);
    translate([spk_pos[0], spk_pos[1], z_top_in - spk_h]) speaker(cut);
    translate([esp_pos[0], esp_pos[1], lid_t + 1]) rotate([0, 0, 180]) translate([-esp_w, -esp_l, 0]) esp32_zero(cut);
    // mic PCB stands vertically against the inside of the top wall, port through it
    translate([mic_pos[0] - mic_w/2, W - wall, H/2 - mic_l/2]) rotate([90, 0, 0]) translate([0, 0, -mic_t]) inmp441(cut);
}

module shell() {
    difference() {
        union() {
            difference() {
                rounded_box([L, W, H], R);
                translate([wall, wall, -eps]) rounded_box([L - 2*wall, W - 2*wall, H - wall], R - wall);
            }
            // screw posts in the corners
            for (x = [R, L - R], y = [R, W - R]) translate([x, y, 0]) cylinder(d=6, h=H - wall);
            // sealed speaker box: a ring from the top plate down to the lid line
            translate([spk_pos[0], spk_pos[1], 0]) difference() {
                cylinder(d=spk_d + 6, h=H - wall);
                translate([0, 0, -eps]) cylinder(d=spk_d + 2, h=H);
            }
            // OLED and joystick posts hang from the top plate
            translate([oled_pos[0], oled_pos[1], z_top_in - oled_t - oled_glass_t]) oled_posts(h=oled_t + oled_glass_t + eps);
            translate([joy_pos[0], joy_pos[1], z_top_in - joy_t - joy_base_h + 1.5]) joystick_posts(h=joy_t + joy_base_h);
        }
        placed_parts(cut=true);
        translate([spk_pos[0], spk_pos[1], H - wall]) speaker_grille(t=wall);
        // pop slots around the mic port in the top edge wall
        translate([mic_pos[0], W - wall/2, H/2]) rotate([90, 0, 0]) pop_grille(14, 10, t=wall + 1);
        // headphone jack out of the left wall (DAC lies along it)
        translate([-1, 30, 9]) rotate([0, 90, 0]) cylinder(d=dac_jack_d + 0.6, h=wall + 2);
        for (x = [R, L - R], y = [R, W - R]) translate([x, y, -eps]) cylinder(d=1.7, h=8);
    }
}

module lid() {
    difference() {
        rounded_box([L, W, lid_t], R);
        for (x = [R, L - R], y = [R, W - R]) translate([x, y, -eps]) cylinder(d=2.3, h=lid_t + 1);
    }
    // battery cradle
    translate([L/2 - bat_w/2 - 1.5, W/2 - bat_l/2 - 1.5, lid_t]) difference() {
        cube([bat_w + 3, bat_l + 3, 4]);
        translate([1.5, 1.5, -eps]) cube([bat_w, bat_l, 5]);
    }
}

if (part == "shell" || part == "both") {
    shell();
    if (show_parts && $preview) %placed_parts();
}
if (part == "lid") lid();
if (part == "both") translate([L + 15, 0, 0]) lid();

echo(str("speaker box volume ~", round(PI * pow((spk_d + 2)/2, 2) * (H - wall - spk_h) / 1000), " cm3 (sealed)"));
