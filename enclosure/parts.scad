// ==============================================================================
// ESP32-S3 Audio Kit - Component Models & Cutouts Library (parts.scad)
// ==============================================================================
// Based on the Minimal Audio Kit shortlist:
// 1. ESP32-S3 SuperMini (ESP32-S3FH4R2, USB-C at bottom edge)
// 2. PCM5102A I2S DAC (Purple board, 3.5mm jack facing SIDE wall)
//    - Sizes set in enclosure.scad (Customizer group "PCM5102A DAC board")
// 3. KY-023 Analog Joystick module with fitted pins and cap
// 4. EC11 Rotary Encoder (bare through-hole, vertical shaft with push button)
// 5. INMP441 I2S Microphone (round 6-pin board, mounted under top layer with grill)
// 6. SSD1306 0.96" OLED Display (128x64, I2C, 4 pins)
// Plus: MPU6050 GY-521 IMU module (6-DoF accelerometer/gyroscope)
//
// Optimized for:
//   - 0.8 mm 3D printer nozzle geometries (robust perimeters and wall widths)
//   - Heavy-duty brass heat-set threaded inserts (M3, M2.5, M2 options)
// ==============================================================================

$fn = $preview ? 36 : 72;
eps = 0.02;

// ------------------------------------------------------------------------------
// 1. ESP32-S3 SuperMini
// ------------------------------------------------------------------------------
esp_pcb_w    = 18.0;
esp_pcb_l    = 25.0;
esp_pcb_t    = 1.2;
esp_usb_w    = 9.2;
esp_usb_h    = 3.4;
esp_usb_len  = 7.5;
esp_can_w    = 15.0;
esp_can_l    = 16.0;
esp_can_h    = 2.4;

module esp32_s3_supermini(cut=false, clearance=0.4) {
    if (!cut) {
        // Deep blue / purple PCB
        color([0.15, 0.25, 0.5])
            cube([esp_pcb_w, esp_pcb_l, esp_pcb_t]);
        
        // RF shield can
        color([0.85, 0.85, 0.88])
            translate([(esp_pcb_w - esp_can_w)/2, 2.0, esp_pcb_t])
                cube([esp_can_w, esp_can_l, esp_can_h]);
        
        // USB-C receptacle
        color([0.92, 0.92, 0.94])
            translate([(esp_pcb_w - esp_usb_w)/2, -1.0, esp_pcb_t])
                hull() {
                    translate([1.2, 0, 1.2]) rotate([-90,0,0]) cylinder(r=1.2, h=esp_usb_len);
                    translate([esp_usb_w - 1.2, 0, 1.2]) rotate([-90,0,0]) cylinder(r=1.2, h=esp_usb_len);
                    translate([1.2, 0, esp_usb_h - 1.2]) rotate([-90,0,0]) cylinder(r=1.2, h=esp_usb_len);
                    translate([esp_usb_w - 1.2, 0, esp_usb_h - 1.2]) rotate([-90,0,0]) cylinder(r=1.2, h=esp_usb_len);
                }
        
        // Header pins (downward facing)
        color([0.2, 0.2, 0.2]) {
            for (y = [1.5 : 2.54 : 20]) {
                translate([1.27, y, -4.5]) cube([0.64, 0.64, 4.5]);
                translate([esp_pcb_w - 1.27 - 0.64, y, -4.5]) cube([0.64, 0.64, 4.5]);
            }
        }
    } else {
        // USB-C cable shroud clearance slot
        shroud_w = 13.0;
        shroud_h = 7.5;
        translate([(esp_pcb_w - shroud_w)/2, -15, esp_pcb_t - 1.0])
            hull() {
                translate([2, 0, 2]) rotate([-90,0,0]) cylinder(r=2, h=25);
                translate([shroud_w - 2, 0, 2]) rotate([-90,0,0]) cylinder(r=2, h=25);
                translate([2, 0, shroud_h - 2]) rotate([-90,0,0]) cylinder(r=2, h=25);
                translate([shroud_w - 2, 0, shroud_h - 2]) rotate([-90,0,0]) cylinder(r=2, h=25);
            }
        // PCB keepout volume
        translate([-clearance, -clearance, -clearance])
            cube([esp_pcb_w + 2*clearance, esp_pcb_l + 2*clearance, esp_pcb_t + esp_can_h + 2*clearance]);
    }
}

// ------------------------------------------------------------------------------
// 2. PCM5102A I2S DAC ("purple board", 3.5 mm jack on one edge)
// ------------------------------------------------------------------------------
// The board sizes are set in enclosure.scad (Customizer group "PCM5102A DAC
// board"); the values here are only fallbacks when parts.scad is used alone.
// Local frame: the jack points to +X and pokes through the case wall, Y runs
// along the wall, the PCB underside is at Z = 0.
dac_pcb_long  = 32.0;   // longer PCB edge, outside to outside
dac_pcb_short = 17.0;   // shorter PCB edge
dac_jack_edge = "short"; // "long" or "short": which edge the jack sticks out of
dac_jack_pos  = 3.2;    // jack centre along that edge from the -Y corner, 0 = centred
dac_pcb_t     = 1.6;
dac_jack_w    = 6.0;    // jack body width along the edge
dac_jack_l    = 12.0;   // jack body length into the board
dac_jack_h    = 5.5;    // jack body height above the PCB
dac_jack_out  = 1.5;    // barrel nose past the PCB edge
dac_plug_d    = 8.8;    // clearance hole in the wall for a 3.5 mm plug barrel

// Derived: extent along the jack axis (X, into the case) and along the wall (Y)
dac_len     = (dac_jack_edge == "long") ? dac_pcb_short : dac_pcb_long;
dac_wid     = (dac_jack_edge == "long") ? dac_pcb_long  : dac_pcb_short;
dac_jack_cy = (dac_jack_pos > 0) ? dac_jack_pos : dac_wid / 2;
dac_h       = dac_pcb_t + dac_jack_h; // tallest point above the PCB underside

module pcm5102a_dac(cut=false, clearance=0.45) {
    if (!cut) {
        // Purple PCB
        color([0.5, 0.1, 0.5])
            cube([dac_len, dac_wid, dac_pcb_t]);

        // 3.5 mm jack body, flush with the +X edge
        color([0.15, 0.15, 0.15])
            translate([dac_len - min(dac_jack_l, dac_len), dac_jack_cy - dac_jack_w/2, dac_pcb_t])
                cube([min(dac_jack_l, dac_len), dac_jack_w, dac_jack_h]);

        // Barrel nose past the PCB edge
        color([0.85, 0.75, 0.3])
            translate([dac_len, dac_jack_cy, dac_pcb_t + dac_jack_h/2])
                rotate([0, 90, 0])
                    cylinder(d=5.5, h=dac_jack_out);

        // PCM5102A IC, on the far half of the board from the jack
        color([0.1, 0.1, 0.1])
            translate([1.5, dac_wid/2 - 3, dac_pcb_t])
                cube([min(6, dac_len - 3), 6, 1.5]);

        // 6-pin I2S header along the inward (-X) edge
        color([0.2, 0.2, 0.2])
            translate([0.5, dac_wid/2 - 3*2.54, dac_pcb_t])
                cube([2.5, 6*2.54, 2.5]);
    } else {
        // Plug clearance through the side wall (+X)
        translate([dac_len - 5, dac_jack_cy, dac_pcb_t + dac_jack_h/2])
            rotate([0, 90, 0])
                cylinder(d=dac_plug_d, h=25);

        // PCB keepout
        translate([-clearance, -clearance, -clearance])
            cube([dac_len + 2*clearance, dac_wid + 2*clearance, dac_h + 2*clearance]);
    }
}

// ------------------------------------------------------------------------------
// 3. KY-023 Analog Joystick Module
// ------------------------------------------------------------------------------
joy_pcb_w    = 26.5;
joy_pcb_l    = 34.0;
joy_pcb_t    = 1.6;
joy_hole_dx  = 20.5;
joy_hole_dy  = 26.5;
joy_hole_d   = 2.8;
joy_base_w   = 16.0;
joy_base_h   = 10.5;
joy_cap_d    = 19.5;
joy_cap_h    = 9.5;
joy_travel_d = 26.5; // diameter hole in panel allowing full tilt travel

module ky023_joystick(cut=false, clearance=0.4) {
    if (!cut) {
        // PCB
        color([0.1, 0.45, 0.2]) // green PCB
            translate([-joy_pcb_w/2, -joy_pcb_l/2, 0])
                difference() {
                    cube([joy_pcb_w, joy_pcb_l, joy_pcb_t]);
                    for (sx = [-1, 1], sy = [-1, 1])
                        translate([joy_pcb_w/2 + sx*joy_hole_dx/2, joy_pcb_l/2 + sy*joy_hole_dy/2, -eps])
                            cylinder(d=joy_hole_d, h=joy_pcb_t + 2*eps);
                }
        
        // Potentiometer metal gimbal base
        color([0.75, 0.75, 0.78])
            translate([-joy_base_w/2, -joy_base_w/2, joy_pcb_t])
                cube([joy_base_w, joy_base_w, joy_base_h]);
        
        // Ergonomic thumbstick rubber cap
        color([0.2, 0.2, 0.2]) {
            translate([0, 0, joy_pcb_t + joy_base_h])
                cylinder(d1=6.0, d2=12.0, h=4.0);
            translate([0, 0, joy_pcb_t + joy_base_h + 4.0])
                cylinder(d=joy_cap_d, h=3.0);
            translate([0, 0, joy_pcb_t + joy_base_h + 7.0])
                sphere(d=joy_cap_d);
        }
    } else {
        // Panel circular opening for stick tilt
        translate([0, 0, joy_pcb_t + joy_base_h - 1])
            cylinder(d=joy_travel_d, h=35);
        
        // Screw mounting pilot holes
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx*joy_hole_dx/2, sy*joy_hole_dy/2, -10])
                cylinder(d=joy_hole_d, h=25);
    }
}

// ------------------------------------------------------------------------------
// 4. EC11 Bare Rotary Encoder
// ------------------------------------------------------------------------------
enc_body_s   = 12.2;
enc_body_h   = 6.8;
enc_bush_d   = 7.0;  // M7 threaded collar
enc_bush_h   = 6.5;
enc_shaft_d  = 6.0;  // D-shaft
enc_shaft_h  = 14.0;
enc_knob_d   = 19.0;
enc_knob_h   = 13.0;

module ec11_encoder(cut=false, with_knob=true) {
    if (!cut) {
        // Metal / plastic encoder body
        color([0.45, 0.45, 0.48])
            translate([-enc_body_s/2, -enc_body_s/2, 0])
                cube([enc_body_s, enc_body_s, enc_body_h]);
        
        // M7 threaded bushing collar
        color([0.8, 0.8, 0.85])
            translate([0, 0, enc_body_h])
                cylinder(d=enc_bush_d, h=enc_bush_h);
        
        // Anti-rotation tab on body
        color([0.7, 0.7, 0.75])
            translate([-1.2, -enc_body_s/2 - 1.2, 0])
                cube([2.4, 1.2, enc_body_h]);
        
        // 6mm D-shaft
        color([0.88, 0.88, 0.9])
            translate([0, 0, enc_body_h + enc_bush_h])
                cylinder(d=enc_shaft_d, h=enc_shaft_h);
        
        // Knob visualization
        if (with_knob) {
            color([0.25, 0.25, 0.28])
                translate([0, 0, enc_body_h + enc_bush_h + 2.0])
                    cylinder(d=enc_knob_d, h=enc_knob_h);
        }
    } else {
        // Bushing hole through panel
        translate([0, 0, enc_body_h - 1])
            cylinder(d=enc_bush_d + 0.6, h=30);
        // Anti-rotation tab notch
        translate([-1.6, -enc_body_s/2 - 2.0, enc_body_h - 1])
            cube([3.2, 2.4, 8.0]);
    }
}

// ------------------------------------------------------------------------------
// 5. INMP441 Round 6-Pin I2S Microphone (Top Layer Mounting)
// ------------------------------------------------------------------------------
// Round board confirmed in minimal-kit.md ("Small round six-pin board")
mic_round_d  = 14.5;
mic_round_t  = 1.2;
mic_port_d   = 1.2;  // Acoustic port on MEMS IC underside

// Acoustic rosette grille module:
// Central hole + 6 perimeter holes for smooth perimeters with a 0.8mm nozzle.
module mic_acoustic_grill(depth=10, hole_d=2.2, outer_d=1.8, r=3.4) {
    cylinder(d=hole_d, h=depth, center=true);
    for (a = [0 : 60 : 300]) {
        rotate([0, 0, a])
            translate([r, 0, 0])
                cylinder(d=outer_d, h=depth, center=true);
    }
}

module inmp441_mic_round(cut=false, clearance=0.35) {
    if (!cut) {
        // Circular purple board
        color([0.45, 0.15, 0.55])
            cylinder(d=mic_round_d, h=mic_round_t);
        
        // MEMS mic metallic can (bottom ported)
        color([0.9, 0.9, 0.92])
            translate([-2.0, -1.5, mic_round_t])
                cube([4.0, 3.0, 1.2]);
        
        // 6 solder pads on perimeter
        color([0.8, 0.7, 0.2])
            for (i = [-2.5 : 1 : 2.5])
                translate([i*2.0, -mic_round_d/2 + 2.2, mic_round_t])
                    cylinder(d=1.0, h=0.2);
    } else {
        // Acoustic rosette grille cutting through top layer
        translate([0, 0, 0])
            mic_acoustic_grill(depth=20);
        
        // Board pocket recess on the underside of top layer
        translate([0, 0, -mic_round_t - clearance])
            cylinder(d=mic_round_d + 2*clearance, h=mic_round_t + 2.0);
    }
}

// ------------------------------------------------------------------------------
// 6. SSD1306 0.96" OLED Display (128x64 I2C)
// ------------------------------------------------------------------------------
oled_pcb_w   = 27.3;
oled_pcb_l   = 27.8;
oled_pcb_t   = 1.2;
oled_hole_dx = 23.8;
oled_hole_dy = 23.8;
oled_hole_d  = 2.2;
oled_glass_w = 26.7;
oled_glass_l = 19.3;
oled_glass_t = 1.6;
oled_glass_y = 2.8;
oled_act_w   = 21.74; // 128 pixels
oled_act_l   = 10.86; // 64 pixels
oled_act_y   = 6.5;

module oled_096_ssd1306(cut=false, clearance=0.4) {
    if (!cut) {
        // Blue PCB
        color([0.1, 0.25, 0.6])
            difference() {
                cube([oled_pcb_w, oled_pcb_l, oled_pcb_t]);
                for (sx = [-1, 1], sy = [-1, 1])
                    translate([oled_pcb_w/2 + sx*oled_hole_dx/2, oled_pcb_l/2 + sy*oled_hole_dy/2, -eps])
                        cylinder(d=oled_hole_d, h=oled_pcb_t + 2*eps);
            }
        
        // OLED glass panel
        color([0.05, 0.05, 0.08])
            translate([(oled_pcb_w - oled_glass_w)/2, oled_glass_y, oled_pcb_t])
                cube([oled_glass_w, oled_glass_l, oled_glass_t]);
        
        // Active display area (cyan glow in preview)
        color([0.0, 0.85, 0.95])
            translate([(oled_pcb_w - oled_act_w)/2, oled_act_y, oled_pcb_t + oled_glass_t])
                cube([oled_act_w, oled_act_l, 0.1]);
        
        // 4-pin 2.54mm header at top edge
        color([0.2, 0.2, 0.2])
            translate([oled_pcb_w/2 - 1.5*2.54, oled_pcb_l - 3.0, oled_pcb_t])
                cube([4*2.54, 2.5, 2.5]);
    } else {
        // Display viewing window through front panel (with chamfer)
        win_w = oled_act_w + 1.8;
        win_l = oled_act_l + 1.8;
        translate([(oled_pcb_w - win_w)/2, oled_act_y - 0.9, oled_pcb_t])
            hull() {
                cube([win_w, win_l, 1.0]);
                translate([-1.4, -1.4, 10])
                    cube([win_w + 2.8, win_l + 2.8, 1.0]);
            }
        
        // Screw mounting holes
        for (sx = [-1, 1], sy = [-1, 1])
            translate([oled_pcb_w/2 + sx*oled_hole_dx/2, oled_pcb_l/2 + sy*oled_hole_dy/2, -5])
                cylinder(d=oled_hole_d, h=25);
    }
}

// ------------------------------------------------------------------------------
// Bonus: MPU6050 GY-521 IMU Breakout
// ------------------------------------------------------------------------------
imu_pcb_w    = 15.8;
imu_pcb_l    = 20.8;
imu_pcb_t    = 1.5;
imu_hole_d   = 3.0;
imu_hole_xy  = [7.9, 17.5];

module mpu6050_gy521(cut=false, clearance=0.35) {
    if (!cut) {
        // Blue PCB
        color([0.1, 0.25, 0.7])
            difference() {
                cube([imu_pcb_w, imu_pcb_l, imu_pcb_t]);
                translate([imu_hole_xy[0], imu_hole_xy[1], -eps])
                    cylinder(d=imu_hole_d, h=imu_pcb_t + 2*eps);
            }
        // MPU6050 QFN chip
        color([0.12, 0.12, 0.12])
            translate([imu_pcb_w/2 - 2, 7.5, imu_pcb_t])
                cube([4, 4, 1.0]);
    } else {
        translate([-clearance, -clearance, -clearance])
            cube([imu_pcb_w + 2*clearance, imu_pcb_l + 2*clearance, imu_pcb_t + 2.5]);
    }
}

// ------------------------------------------------------------------------------
// Geometric & 3D Printing Helpers
// ------------------------------------------------------------------------------
module rounded_box_2d(size, r) {
    hull() {
        translate([r, r]) circle(r=r);
        translate([size[0]-r, r]) circle(r=r);
        translate([r, size[1]-r]) circle(r=r);
        translate([size[0]-r, size[1]-r]) circle(r=r);
    }
}

module rounded_box_3d(size, r) {
    linear_extrude(height=size[2])
        rounded_box_2d([size[0], size[1]], r);
}

// Heat-Set Insert Boss Module:
module heat_set_boss(h, hole_d=4.5, hole_depth=5.8, boss_d=11.5, chamfer=0.8) {
    difference() {
        cylinder(d=boss_d, h=h);
        translate([0, 0, -eps])
            cylinder(d=hole_d, h=hole_depth + eps);
        if (chamfer > 0)
            translate([0, 0, -eps])
                cylinder(d1=hole_d + 2*chamfer, d2=hole_d, h=chamfer + eps);
    }
}
