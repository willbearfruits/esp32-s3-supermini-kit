// ==============================================================================
// ESP32-S3 Audio Kit Enclosure - Print Bed Layout (print_bed.scad)
// ==============================================================================
// Pre-configured to render both the Top Shell and Bottom Lid side-by-side
// flat on the 3D printer build plate (Z = 0) for 100% support-free printing.
//
// Optimized for:
//   - 0.8 mm nozzle (3 solid perimeters, 2.4mm walls)
//   - M3 brass heat-set inserts (Ø4.4mm holes in top shell bosses)
//   - PCM5102A purple DAC board (dac_len x dac_wid in parts.scad, default 32 x 17 mm)
//   - Round INMP441 microphone on top layer next to OLED (no overlap)
//
// Usage:
//   - Open directly in OpenSCAD to view the print plate layout
//   - Export as STL: File -> Export as STL (or run ./render.sh)
// ==============================================================================

parent_rendering = true;
include <enclosure.scad>

render_enclosure("print");
