// ==============================================================================
// ESP32-S3 Audio Kit - Parametric 3D Printable Enclosure (Shortlist Edition)
// ==============================================================================
// Specifically tailored to the Minimal Audio Kit 6-Part Shortlist:
//   1. ESP32-S3 SuperMini (MCU, USB-C centered at bottom edge)
//   2. PCM5102A I2S DAC (Purple board along the right wall, jack out the front; Customizer group)
//   3. KY-023 Analog Joystick (Thumb control on lower-left, ±30° tilt dome)
//   4. EC11 Rotary Encoder (Parameter dial on lower-right)
//   5. INMP441 I2S Microphone (Round board next to OLED screen with acoustic rosette grill)
//   6. SSD1306 0.96" OLED Display (128x64 centered at upper-front)
//   +  MPU6050 GY-521 IMU (Internal motion sensor cradle)
//
// Key Manufacturing Specifications:
//   - Heavy-duty heat-set insert bosses (Ø11.5 mm, ~3.5 mm solid walls, 4+ perimeters)
//   - 0.8 mm nozzle optimized (integer wall/floor perimeters, 0.40mm sliding tolerances)
//   - Support-free printing (both Shell and Lid print flat on bed)
//
// Render options:
//   part = "preview"    : Semi-transparent assembled preview with internal parts
//   part = "exploded"   : Exploded assembly diagram
//   part = "print"      : Shell and Lid side-by-side on build plate (for slicing)
//   part = "shell"      : Top Shell only (print-oriented: top face down)
//   part = "lid"        : Bottom Lid only (print-oriented: bottom face down)
//   part = "assembled"  : Solid closed case
// ==============================================================================

include <parts.scad>

// ------------------------------------------------------------------------------
// Configuration Parameters
// ------------------------------------------------------------------------------
part             = is_undef(part) ? "preview" : part;  // "preview" | "exploded" | "print" | "shell" | "lid" | "assembled"
show_parts       = is_undef(show_parts) ? true : show_parts;
include_imu      = true;       // Include internal mounting cradle for MPU6050
show_labels      = true;       // Debossed bold labels (scaled for 0.8mm nozzle)

// 3D Printer Nozzle Optimization
nozzle_d         = 0.8;        // Nozzle diameter (mm)
wall_t           = 3 * nozzle_d; // 2.4 mm (exactly 3 perimeters of 0.8 mm)
floor_t          = 3 * nozzle_d; // 2.4 mm (solid floor/roof)
rim_t            = 2 * nozzle_d; // 1.6 mm (exactly 2 perimeters for interlocking lip)
tol              = 0.40;       // Sliding tolerance for 0.8 mm nozzle line-bulge

// Heavy-Duty Heat-Set Insert Configuration ("M3", "M2.5", "M2")
insert_type      = "M3";       // Recommended for 0.8mm nozzle: M3 (heavy duty)

// Substantially reinforced boss diameters (giving 3.5mm+ solid plastic all around)
boss_d           = (insert_type == "M3")   ? 11.5 :
                   (insert_type == "M2.5") ? 10.5 : 9.5;

insert_hole_d    = (insert_type == "M3")   ? 4.5 :
                   (insert_type == "M2.5") ? 3.9 : 3.3;

insert_depth     = (insert_type == "M3")   ? 5.8 :
                   (insert_type == "M2.5") ? 5.0 : 4.2;

lid_screw_d      = (insert_type == "M3")   ? 3.6 :
                   (insert_type == "M2.5") ? 3.0 : 2.5;

screw_head_d     = (insert_type == "M3")   ? 7.2 :
                   (insert_type == "M2.5") ? 6.2 : 5.2;

screw_head_h     = (insert_type == "M3")   ? 3.2 :
                   (insert_type == "M2.5") ? 2.6 : 2.2;

/* [PCM5102A DAC board] */
// Measure your board, outside edge to outside edge. The jack edge sits
// against the chosen wall and the jack pokes out through it.
dac_wall      = "front"; // [front, right, back] which case wall the jack pokes through
dac_along     = 68.5;   // board position along that wall: X of its left edge for front/back, Y of its lower edge for right. Stay ~14 mm off the corners (screw bosses).
dac_pcb_long  = 32.0;   // longer PCB edge (mm)
dac_pcb_short = 17.0;   // shorter PCB edge (mm)
dac_jack_edge = "short"; // [long, short] which edge the 3.5 mm jack sticks out of
dac_jack_pos  = 3.2;    // jack centre along that edge from the board's -Y corner (photo estimate: flush with one long edge), 0 = centred
dac_pcb_t     = 1.6;    // PCB thickness
dac_jack_w    = 6.0;    // jack body width along the edge
dac_jack_l    = 12.0;   // jack body length into the board
dac_jack_h    = 5.5;    // jack body height above the PCB
dac_jack_out  = 1.5;    // barrel nose past the PCB edge
dac_plug_d    = 8.8;    // hole in the wall for a 3.5 mm plug barrel
dac_gap       = 0.5;    // PCB jack edge to the inside face of the wall
dac_lift      = 1.2;    // PCB underside above the lid floor (room for header pins)
dac_fit       = 0.3;    // slot clearance each side of the PCB in the cradle
dac_cradle_h  = 3.8;    // cradle wall height above the lid floor

/* [Enclosure] */
// Enclosure Overall Dimensions (mm)
case_w           = 100.0;      // Outer width (X) - provides generous clearance for joystick, ESP32, & encoder
case_l           = 98.0;       // Outer length (Y)
case_h           = 24.0;       // Outer height (Z)
corner_r         = 8.0;        // Outer corner radius
lid_h            = 7.0;        // Bottom lid height (parting line at Z = lid_h)
shell_h          = case_h - lid_h; // Top shell height = 17.0 mm
rim_h            = 1.8;        // Interlocking lip height

// ------------------------------------------------------------------------------
// Component Placements
// ------------------------------------------------------------------------------
z_top_in = case_h - floor_t;

// 1. OLED Display (upper center)
oled_x = (case_w - oled_pcb_w) / 2; // centered at X = 50.0 (runs X = 36.35 to 63.65)
oled_y = 58.0;
oled_z = z_top_in - oled_pcb_t - oled_glass_t;

// 2. KY-023 Joystick (lower-left quadrant - ZERO OVERLAP with ESP32 or corner boss)
joy_cx = 22.5;                 // Shifted to clear ESP32 cradle with >3.7mm boss clearance & >5.2mm PCB clearance
joy_cy = 32.5;                 // Positioned for ergonomic thumb reach with >3.1mm corner boss clearance
joy_z  = z_top_in - joy_pcb_t - joy_base_h + 1.2;

// 3. EC11 Rotary Encoder (lower-right quadrant)
enc_cx = 70.0;
enc_cy = 32.5;
enc_z  = z_top_in - enc_body_h;

// 4. INMP441 Microphone (Round board NEXT TO OLED SCREEN - NO OVERLAP)
mic_side         = "right";    // "right" | "left" (next to OLED)
mic_cx           = (mic_side == "right") ? 78.0 : 18.0;
mic_cy           = 65.0;
mic_z            = z_top_in - mic_round_t;

// 5. ESP32-S3 SuperMini (centered on bottom edge, in lid)
esp_x  = (case_w - esp_pcb_w) / 2; // centered at X = 50.0 (runs 41.0 to 59.0 mm)
esp_y  = wall_t + 1.0;
esp_z  = floor_t + 1.2;

// 6. PCM5102A I2S DAC: jack edge against the wall chosen by dac_wall. Sizes
// come from the "PCM5102A DAC board" group above; parts.scad derives dac_len
// (along the jack axis) and dac_wid (across it). The board, its cradle and
// the wall cutouts are all drawn in the board's own frame (jack towards +X,
// PCB underside at Z = 0) and dropped into the case by dac_place().
dac_cradle_wall = 2 * nozzle_d;    // 1.6 mm cradle walls
dac_z      = floor_t + dac_lift;
dac_jack_z = dac_z + dac_pcb_t + dac_jack_h/2;  // jack axis height, shared by shell and lid cutouts
dac_x = (dac_wall == "right") ? case_w - wall_t - dac_gap - dac_len : dac_along;
dac_y = (dac_wall == "right") ? dac_along :
        (dac_wall == "front") ? wall_t + dac_gap : case_l - wall_t - dac_gap - dac_len;
dac_span_x = (dac_wall == "right") ? dac_len : dac_wid;   // footprint in the case
dac_span_y = (dac_wall == "right") ? dac_wid : dac_len;

module dac_place() {
    if (dac_wall == "right")
        translate([dac_x, dac_y, dac_z]) children();
    else if (dac_wall == "front")
        translate([dac_x, dac_y + dac_len, dac_z]) rotate([0, 0, -90]) children();
    else
        translate([dac_x + dac_wid, dac_y, dac_z]) rotate([0, 0, 90]) children();
}

// Plug hole through the wall plus a slot between case heights z0 and z1, so the
// shell and the lid each get an arch that meets at the parting line.
module dac_jack_cut(z0, z1) {
    dac_place() {
        translate([dac_len - 2, dac_jack_cy, dac_pcb_t + dac_jack_h/2])
            rotate([0, 90, 0])
                cylinder(d=dac_plug_d, h=dac_gap + wall_t + 4);
        translate([dac_len - 2, dac_jack_cy - dac_plug_d/2, z0 - dac_z])
            cube([dac_gap + wall_t + 4, dac_plug_d, max(z1 - z0, eps)]);
    }
}

// 7. MPU6050 IMU (internal tray in lid on left side)
imu_x  = 13.5;
imu_y  = 54.0;
imu_z  = floor_t + 0.6;

// Corner Screw Posts (offset by corner_r)
screw_pos = [
    [corner_r, corner_r],
    [case_w - corner_r, corner_r],
    [corner_r, case_l - corner_r],
    [case_w - corner_r, case_l - corner_r]
];

// ==============================================================================
// Component Placement Helper
// ==============================================================================
module placed_components(cut=false) {
    // 1. OLED
    translate([oled_x, oled_y, oled_z])
        oled_096_ssd1306(cut=cut);
    
    // 2. Joystick
    translate([joy_cx, joy_cy, joy_z])
        ky023_joystick(cut=cut);
    
    // 3. Encoder
    translate([enc_cx, enc_cy, enc_z])
        ec11_encoder(cut=cut, with_knob=true);
    
    // 4. Round Microphone (mounted under top layer NEXT TO OLED)
    translate([mic_cx, mic_cy, z_top_in])
        rotate([180, 0, 0])
            inmp441_mic_round(cut=cut);
    
    // 5. ESP32-S3 SuperMini (centered on bottom edge)
    translate([esp_x, esp_y, esp_z])
        esp32_s3_supermini(cut=cut);
    
    // 6. PCM5102A DAC (jack through the wall chosen by dac_wall)
    dac_place()
        pcm5102a_dac(cut=cut);
    
    // 7. MPU6050 IMU
    if (include_imu) {
        translate([imu_x, imu_y, imu_z])
            mpu6050_gy521(cut=cut);
    }
}

// ==============================================================================
// TOP SHELL MODULE
// ==============================================================================
module top_shell_geometry() {
    difference() {
        union() {
            // Main outer body
            difference() {
                translate([0, 0, lid_h])
                    rounded_box_3d([case_w, case_l, shell_h], corner_r);
                
                // Cavity
                translate([wall_t, wall_t, lid_h - eps])
                    rounded_box_3d([case_w - 2*wall_t, case_l - 2*wall_t, shell_h - floor_t + eps], corner_r - wall_t);
            }
            
            // 4 Extra-Thick Corner Screw Bosses for Heat-Set Inserts
            for (p = screw_pos) {
                translate([p[0], p[1], lid_h])
                    difference() {
                        // Heavy-duty structural column (Ø11.5mm)
                        cylinder(d=boss_d, h=shell_h - floor_t);
                        // Heat-set insert pilot hole
                        translate([0, 0, -eps])
                            cylinder(d=insert_hole_d, h=insert_depth + eps);
                        // Conical lead-in chamfer for easy alignment of hot brass insert
                        translate([0, 0, -eps])
                            cylinder(d1=insert_hole_d + 1.4, d2=insert_hole_d, h=0.8);
                    }
            }
            
            // OLED Mounting Bosses (4 posts from top plate down to OLED PCB)
            for (sx = [-1, 1], sy = [-1, 1]) {
                ox = oled_x + oled_pcb_w/2 + sx*oled_hole_dx/2;
                oy = oled_y + oled_pcb_l/2 + sy*oled_hole_dy/2;
                post_h = floor_t + oled_glass_t;
                translate([ox, oy, z_top_in - post_h])
                    difference() {
                        cylinder(d=5.2, h=post_h);
                        translate([0, 0, -eps])
                            cylinder(d=1.8, h=post_h + 2*eps);
                    }
            }
            
            // Joystick Mounting Bosses (4 posts matching KY-023 holes)
            for (sx = [-1, 1], sy = [-1, 1]) {
                jx = joy_cx + sx*joy_hole_dx/2;
                jy = joy_cy + sy*joy_hole_dy/2;
                post_h = z_top_in - joy_z;
                translate([jx, jy, joy_z])
                    difference() {
                        cylinder(d=5.8, h=post_h);
                        translate([0, 0, -eps])
                            cylinder(d=2.0, h=post_h + 2*eps);
                    }
            }
            
            // Round Microphone Housing (under top layer NEXT TO OLED, with gasket seat)
            translate([mic_cx, mic_cy, z_top_in - 3.2])
                difference() {
                    cylinder(d=mic_round_d + 3.6, h=3.2);
                    translate([0, 0, -eps])
                        cylinder(d=mic_round_d + 2*tol, h=3.2 - 1.0);
                    translate([0, 0, 3.2 - 1.0 - eps])
                        cylinder(d=10.0, h=1.0 + 2*eps);
                }
            
            // Encoder anti-rotation locator pocket
            translate([enc_cx - 1.6, enc_cy - enc_body_s/2 - 2.0, z_top_in - 2.0])
                cube([3.2, 2.0, 2.0]);
            
            // Ergonomic side grip ribs
            // Left wall ribs
            for (i = [0 : 4]) {
                translate([0, 36 + i*6.0, lid_h + 3.0])
                    rotate([0, 90, 0])
                        cylinder(r=1.2, h=wall_t, $fn=16);
            }
            // Right wall ribs (placed above the 3.5mm audio jack to avoid clash)
            for (i = [0 : 3]) {
                translate([case_w - wall_t, 64 + i*5.5, lid_h + 3.0])
                    rotate([0, 90, 0])
                        cylinder(r=1.2, h=wall_t, $fn=16);
            }
        }
        
        // --- Negative Features (Cutouts) ---
        // 1. Placed component cutouts (OLED window, joystick dome, encoder shaft, mic acoustic grill)
        placed_components(cut=true);
        
        // 2. Alignment lip rebate / groove along parting perimeter (sized for 0.8mm nozzle)
        translate([wall_t - 0.6 - tol, wall_t - 0.6 - tol, lid_h - eps])
            difference() {
                rounded_box_3d([case_w - 2*(wall_t - 0.6 - tol), case_l - 2*(wall_t - 0.6 - tol), rim_h + eps], corner_r - (wall_t - 0.6 - tol));
                translate([rim_t + 2*tol, rim_t + 2*tol, -eps])
                    rounded_box_3d([case_w - 2*(wall_t - 0.6 + rim_t + tol), case_l - 2*(wall_t - 0.6 + rim_t + tol), rim_h + 3*eps], corner_r - (wall_t - 0.6 + rim_t + tol));
            }
        
        // 3. Port Cutouts across Parting Line:
        // USB-C upper arch on BOTTOM wall center
        translate([esp_x + (esp_pcb_w - 13.0)/2, -eps, lid_h - eps])
            cube([13.0, wall_t + 2*eps, 4.5]);
        
        // 3.5mm Audio Jack upper arch (open down to the parting line)
        dac_jack_cut(lid_h - eps, dac_jack_z);
        
        // 4. Debossed Labels & Aesthetics
        if (show_labels) {
            // Main title centered above OLED
            translate([case_w/2, 89.0, case_h - 0.5])
                linear_extrude(height=0.6)
                    text("AUDIO KIT S3", size=4.0, font="Liberation Sans:style=Bold", halign="center", valign="center");
            
            // "MIC" label above grill next to OLED
            translate([mic_cx, mic_cy + 8.8, case_h - 0.5])
                linear_extrude(height=0.6)
                    text("MIC", size=2.6, font="Liberation Sans:style=Bold", halign="center", valign="center");
            
            // "JOYSTICK" and "ENCODER" labels
            translate([joy_cx, 11.5, case_h - 0.5])
                linear_extrude(height=0.5)
                    text("JOYSTICK", size=2.4, font="Liberation Sans:style=Bold", halign="center", valign="center");
            translate([enc_cx, 11.5, case_h - 0.5])
                linear_extrude(height=0.5)
                    text("ENCODER", size=2.4, font="Liberation Sans:style=Bold", halign="center", valign="center");
            
            // "AUDIO" label debossed in the outer wall face above the jack
            dac_place()
                translate([dac_len + dac_gap + wall_t - 0.5, dac_jack_cy, lid_h + 5.5 - dac_z])
                    rotate([90, 0, 90])
                        linear_extrude(height=0.5 + eps)
                            text("AUDIO", size=2.2, font="Liberation Sans:style=Bold", halign="center", valign="center");
        }
        
        // USB-C label on bottom edge
        translate([esp_x + esp_pcb_w/2, wall_t - 0.4, lid_h + 5.5])
            rotate([90, 0, 0])
                linear_extrude(height=0.6)
                    text("USB-C", size=2.2, font="Liberation Sans:style=Bold", halign="center", valign="center");
    }
}

// ==============================================================================
// BOTTOM LID MODULE
// ==============================================================================
module bottom_lid_geometry() {
    difference() {
        union() {
            // Main bottom tub
            difference() {
                rounded_box_3d([case_w, case_l, lid_h], corner_r);
                translate([wall_t, wall_t, floor_t])
                    rounded_box_3d([case_w - 2*wall_t, case_l - 2*wall_t, lid_h], corner_r - wall_t);
            }
            
            // Interlocking alignment rim (protrudes upward into shell groove)
            translate([wall_t - 0.6, wall_t - 0.6, lid_h - 0.5])
                difference() {
                    rounded_box_3d([case_w - 2*(wall_t - 0.6), case_l - 2*(wall_t - 0.6), rim_h + 0.5], corner_r - (wall_t - 0.6));
                    translate([rim_t, rim_t, -eps])
                        rounded_box_3d([case_w - 2*(wall_t - 0.6 + rim_t), case_l - 2*(wall_t - 0.6 + rim_t), rim_h + 0.5 + 2*eps], corner_r - (wall_t - 0.6 + rim_t));
                }
            
            // 4 Extra-Thick Corner Screw Bosses (anchored into solid floor)
            for (p = screw_pos) {
                translate([p[0], p[1], floor_t - eps])
                    cylinder(d=boss_d, h=lid_h - floor_t + eps);
            }
            
            // --- ESP32-S3 SuperMini Retention Cradle (Bottom Center) ---
            translate([esp_x - 1.6, esp_y, floor_t - eps])
                difference() {
                    cube([esp_pcb_w + 3.2, esp_pcb_l + 2.0, 3.5 + eps]);
                    translate([1.6, -eps, 1.2])
                        cube([esp_pcb_w, esp_pcb_l + 2*eps, 4.0]);
                }
            
            // --- PCM5102A DAC Retention Cradle (jack end against its wall) ---
            // U-shaped in the board's frame: walls on the inward end and both
            // sides, open towards the wall so the jack nose reaches the hole.
            // The PCB rests on a dac_lift ledge so header pins have room.
            dac_place()
                translate([-dac_cradle_wall, -dac_cradle_wall, -dac_lift - eps])
                    difference() {
                        cube([dac_len + dac_cradle_wall + dac_gap + 1.0, dac_wid + 2*dac_cradle_wall, dac_cradle_h + eps]);
                        translate([dac_cradle_wall - dac_fit, dac_cradle_wall - dac_fit, dac_lift])
                            cube([dac_len + 2*dac_fit + 5.0, dac_wid + 2*dac_fit, 10.0]);
                    }
            
            // --- MPU6050 IMU Retention Cradle ---
            if (include_imu) {
                translate([imu_x - 1.6, imu_y - 1.6, floor_t - eps])
                    difference() {
                        cube([imu_pcb_w + 3.2, imu_pcb_l + 3.2, 3.2 + eps]);
                        translate([1.6, 1.6, 0.6])
                            cube([imu_pcb_w, imu_pcb_l, 4.0]);
                    }
                translate([imu_x + 2, imu_y + 2, floor_t - eps])
                    cube([imu_pcb_w - 4, imu_pcb_l - 4, 0.6 + eps]);
            }
            
            // Internal wire tie anchor loop
            translate([case_w/2 - 6, 44, floor_t - eps])
                difference() {
                    cube([12, 4.0, 3.6 + eps]);
                    translate([2, -eps, 1.2])
                        cube([8, 4.0 + 2*eps, 1.6]);
                }
        }
        
        // --- Negative Features (Cutouts) ---
        // 1. 4 Corner Counterbored Screw Holes for selected insert size
        for (p = screw_pos) {
            translate([p[0], p[1], -eps]) {
                cylinder(d=lid_screw_d, h=lid_h + rim_h + 2.0);
                cylinder(d=screw_head_d, h=screw_head_h + eps);
                translate([0, 0, screw_head_h])
                    cylinder(d1=screw_head_d, d2=lid_screw_d, h=0.8);
            }
        }
        
        // 2. USB-C Lower Port Cutout on BOTTOM wall center
        translate([esp_x + (esp_pcb_w - 13.0)/2, -eps, floor_t + 1.2])
            cube([13.0, wall_t + 2.0, lid_h + rim_h]);
        
        // 3. 3.5mm Headphone Jack Lower Cutout (open up through the parting line and rim)
        dac_jack_cut(dac_jack_z, lid_h + rim_h + 1.0);
        
        // 4. Subtle recessed feet / rubber bumper pads on bottom
        for (sx = [-1, 1], sy = [-1, 1]) {
            fx = case_w/2 + sx*(case_w/2 - 16);
            fy = case_l/2 + sy*(case_l/2 - 16);
            translate([fx, fy, -eps])
                cylinder(d=8.0, h=0.8);
        }
        
        // 5. Debossed branding mark on underside floor
        if (show_labels) {
            translate([case_w/2, case_l/2, -eps])
                mirror([1, 0, 0])
                    linear_extrude(height=0.5 + eps)
                        text("ESP32-S3 AUDIO KIT", size=3.2, font="Liberation Sans:style=Bold", halign="center", valign="center");
        }
    }
}

// ==============================================================================
// RENDER SELECTOR & VIEW MODES
// ==============================================================================
module render_enclosure(p=part) {
    if (p == "preview") {
        // Semi-transparent shell showing internal part layout
        %top_shell_geometry();
        bottom_lid_geometry();
        if (show_parts)
            placed_components(cut=false);
    }
    else if (p == "exploded") {
        // Exploded view showing assembly progression
        color([0.88, 0.88, 0.90])
            bottom_lid_geometry();
        
        if (show_parts) {
            translate([0, 0, 10])
                placed_components(cut=false);
        }
        
        translate([0, 0, 38])
            color([0.22, 0.24, 0.28])
                top_shell_geometry();
    }
    else if (p == "assembled") {
        color([0.22, 0.24, 0.28]) top_shell_geometry();
        color([0.85, 0.85, 0.88]) bottom_lid_geometry();
        if (show_parts) placed_components(cut=false);
    }
    else if (p == "shell") {
        // Top Shell ready for 3D printing (oriented face-down on build plate at Z=0)
        translate([0, case_l, case_h])
            rotate([180, 0, 0])
                top_shell_geometry();
    }
    else if (p == "lid") {
        // Bottom Lid ready for 3D printing (oriented flat on build plate at Z=0)
        bottom_lid_geometry();
    }
    else if (p == "print") {
        // Both parts side-by-side on the print bed for single-run slicing
        translate([0, case_l, case_h])
            rotate([180, 0, 0])
                top_shell_geometry();
        
        translate([case_w + 14.0, 0, 0])
            bottom_lid_geometry();
    }
}

if (is_undef(parent_rendering)) {
    render_enclosure(part);
}

// ==============================================================================
// Terminal Diagnostics
// ==============================================================================
echo(str("--- ESP32-S3 Audio Kit Enclosure (Side Audio Jack / Thick Bosses) ---"));
echo(str("Nozzle: ", nozzle_d, "mm | Walls: ", wall_t, "mm | Rim: ", rim_t, "mm"));
echo(str("Inserts: ", insert_type, " (Hole: ", insert_hole_d, "mm, Depth: ", insert_depth, "mm, Boss: ", boss_d, "mm)"));
echo(str("Boss Solid Wall: ", (boss_d - insert_hole_d)/2, " mm (~4.5 solid perimeters!)"));
echo(str("PCM5102A DAC: ", dac_pcb_long, " x ", dac_pcb_short, " mm, X ", dac_x, "..", dac_x + dac_span_x, " Y ", dac_y, "..", dac_y + dac_span_y, ", jack through the ", dac_wall, " wall"));
echo(str("ESP32-S3: Centered on bottom edge (USB-C exits bottom wall)"));
echo(str("Microphone: Round 14.5mm board NEXT TO OLED on top layer"));
echo(str("Dimensions: ", case_w, " x ", case_l, " x ", case_h, " mm"));
