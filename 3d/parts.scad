// Component dimensions and placeholder models for the kit enclosure.
//
// EVERY NUMBER HERE IS A TYPICAL VALUE FROM DATASHEETS AND SELLER DRAWINGS,
// NOT A MEASUREMENT OF YOUR MODULES. Put calipers on each board and fix the
// values before printing. Boards from different sellers differ by a few mm.
//
// Conventions: all parts are drawn with their PCB bottom on z=0, centred on
// x/y, connectors pointing +y unless noted. `cut=true` returns the negative
// (through-holes, windows, clearance) to subtract from a shell.

$fn = $preview ? 32 : 64;
pitch = 2.54;
eps   = 0.01;

// ---- ESP32-S3-Zero style SuperMini (castellated, USB-C on the short edge)
esp_l = 25.0;  esp_w = 18.0;  esp_t = 1.0;   // VERIFY
esp_usb_w = 9.0; esp_usb_h = 3.3;            // USB-C plug clearance
esp_h_top = 3.2;                             // tallest part on top (USB-C, module can)

module esp32_zero(cut=false) {
    if (!cut) {
        color("navy") cube([esp_w, esp_l, esp_t], center=false);
        color("silver") translate([esp_w/2 - 4.5, esp_l - 7.5, esp_t]) cube([9, 7.5, esp_h_top]);
    } else {
        // USB-C slot through the wall, generous for the plug shroud
        translate([esp_w/2 - 6.5, esp_l - 1, -1]) cube([13, 30, esp_t + esp_h_top + 3]);
    }
}

// ---- 0.96" SSD1306 OLED module, 4-pin header on the top edge
oled_w = 27.3; oled_l = 27.8; oled_t = 1.2;            // VERIFY
oled_hole_dx = 23.5; oled_hole_dy = 23.8; oled_hole_d = 2.0;
oled_glass_w = 26.7; oled_glass_l = 19.3; oled_glass_t = 1.6;
oled_glass_y = 3.0;                                    // glass sits low; header at the top
oled_active_w = 21.7; oled_active_l = 10.9;            // visible pixels
oled_active_y = 6.6;                                   // from the PCB bottom edge to the active area

module oled_096(cut=false) {
    if (!cut) {
        color("darkblue") cube([oled_w, oled_l, oled_t]);
        color("black") translate([(oled_w - oled_glass_w)/2, oled_glass_y, oled_t]) cube([oled_glass_w, oled_glass_l, oled_glass_t]);
        color("cyan") translate([(oled_w - oled_active_w)/2, oled_active_y, oled_t + oled_glass_t]) cube([oled_active_w, oled_active_l, 0.2]);
    } else {
        // window over the active area, 1 mm margin, cut straight up
        translate([(oled_w - oled_active_w)/2 - 1, oled_active_y - 1, oled_t]) cube([oled_active_w + 2, oled_active_l + 2, 30]);
    }
}
module oled_posts(h=3, d=1.6) {   // screw posts for M2 self-tappers
    for (sx = [-1, 1], sy = [-1, 1])
        translate([oled_w/2 + sx*oled_hole_dx/2, oled_l/2 + sy*oled_hole_dy/2, 0]) cylinder(d=d, h=h);
}

// ---- rotary encoder, bare EC11 with a 6 mm D shaft (not the KY-040 board)
enc_body = 12.5; enc_body_h = 7.0; enc_shaft_d = 6.0; enc_shaft_l = 15.0; // VERIFY
enc_thread_d = 7.0;   // panel hole for the M7 bushing
enc_knob_d = 20;

module encoder(cut=false) {
    if (!cut) {
        color("gray") translate([-enc_body/2, -enc_body/2, 0]) cube([enc_body, enc_body, enc_body_h]);
        color("silver") translate([0, 0, enc_body_h]) cylinder(d=enc_shaft_d, h=enc_shaft_l);
    } else {
        translate([0, 0, enc_body_h - 1]) cylinder(d=enc_thread_d + 0.4, h=30);
    }
}

// ---- thumb joystick module (KY-023 style breakout with 5-pin header)
joy_w = 26.0; joy_l = 34.0; joy_t = 1.6;     // VERIFY. bare thumbstick: 16x16 base
joy_base = 16; joy_base_h = 10; joy_cap_d = 19; joy_cap_h = 9;
joy_hole_dx = 20.5; joy_hole_dy = 26.5; joy_hole_d = 2.5;
joy_cap_travel_d = 26;                       // opening so the cap can tilt

module joystick(cut=false) {
    if (!cut) {
        color("green") translate([-joy_w/2, -joy_l/2, 0]) cube([joy_w, joy_l, joy_t]);
        color("gray") translate([-joy_base/2, -joy_base/2, joy_t]) cube([joy_base, joy_base, joy_base_h]);
        color("black") translate([0, 0, joy_t + joy_base_h]) cylinder(d=joy_cap_d, h=joy_cap_h);
    } else {
        translate([0, 0, joy_t + joy_base_h - 2]) cylinder(d=joy_cap_travel_d, h=30);
    }
}
module joystick_posts(h=3, d=2.0) {
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx*joy_hole_dx/2, sy*joy_hole_dy/2, 0]) cylinder(d=d, h=h);
}

// ---- INMP441 breakout. The MEMS port is a hole THROUGH the PCB under the
// can, so the board mounts face-down over a port in the shell.
mic_w = 11.0; mic_l = 15.0; mic_t = 1.0;     // VERIFY
mic_port_d = 1.0;                            // hole in the PCB
mic_port_xy = [mic_w/2, 5.0];                // where that hole is, from the PCB corner
mic_hole_d = 1.5;                            // port in the shell (slightly bigger)

module inmp441(cut=false) {
    if (!cut) {
        color("purple") cube([mic_w, mic_l, mic_t]);
        color("silver") translate([mic_port_xy[0] - 2, mic_port_xy[1] - 1.5, mic_t]) cube([4, 3, 1.2]);
    } else {
        translate([mic_port_xy[0], mic_port_xy[1], -10]) cylinder(d=mic_hole_d, h=10 + eps);
    }
}

// ---- PCM5102A "purple" DAC with 3.5 mm jack on the short edge
dac_w = 21.0; dac_l = 33.0; dac_t = 1.6; dac_h = 6.0;   // VERIFY
dac_jack_d = 6.5; dac_jack_x = 6.5;                    // jack centre from the board edge
module pcm5102a(cut=false) {
    if (!cut) { color("purple") cube([dac_w, dac_l, dac_t]); color("black") translate([dac_jack_x - 3, dac_l - 6, dac_t]) cube([6, 6, dac_h]); }
    else translate([dac_jack_x, dac_l - 3, dac_t + 3]) rotate([-90, 0, 0]) cylinder(d=dac_jack_d + 0.6, h=30);
}

// ---- MAX98357A breakout
amp_w = 17.8; amp_l = 15.2; amp_t = 1.6; amp_h = 3;      // VERIFY
module max98357a() { color("darkgreen") cube([amp_w, amp_l, amp_t]); }

// ---- MPU6050 GY-521
imu_w = 16.0; imu_l = 21.0; imu_t = 1.6;                  // VERIFY
module mpu6050() { color("darkblue") cube([imu_w, imu_l, imu_t]); }

// ---- speaker, round 40 mm 4 ohm 3 W (28 mm is the other common size)
spk_d = 40; spk_h = 5.5; spk_rim = 2.0;                   // VERIFY
module speaker(cut=false) {
    if (!cut) color("dimgray") cylinder(d=spk_d, h=spk_h);
    else cylinder(d=spk_d + 0.6, h=spk_h + 0.5);
}
module speaker_grille(d=spk_d - 4, hole=2.2, step=4, t=10) {
    // hex-ish field of holes, cut through a wall of thickness t
    for (x = [-d/2 : step : d/2], y = [-d/2 : step*0.866 : d/2]) {
        xo = x + ((round(y / (step*0.866)) % 2 == 0) ? 0 : step/2);
        if (sqrt(xo*xo + y*y) < d/2) translate([xo, y, -eps]) cylinder(d=hole, h=t + 2*eps);
    }
}

// ---- 1S LiPo and a TP4056 charger board
bat_w = 34; bat_l = 50; bat_t = 6.5;        // ~1000 mAh, VERIFY
chg_w = 17; chg_l = 26; chg_t = 1.2;
module lipo() { color("silver") cube([bat_w, bat_l, bat_t]); }
module tp4056() { color("blue") cube([chg_w, chg_l, chg_t]); }

// ---- helpers
module rounded_box(size, r) {
    hull() for (x = [r, size[0]-r], y = [r, size[1]-r]) translate([x, y, 0]) cylinder(r=r, h=size[2]);
}
module screw_post(h, d_out=5, d_in=1.7) {     // M2 self-tapping post
    difference() { cylinder(d=d_out, h=h); translate([0, 0, 1]) cylinder(d=d_in, h=h); }
}
module pop_grille(w, l, slot=1.0, step=2.2, t=10) {
    for (y = [-l/2 + slot : step : l/2 - slot]) translate([-w/2, y - slot/2, -eps]) cube([w, slot, t + 2*eps]);
}
