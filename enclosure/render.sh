#!/usr/bin/env bash
# ==============================================================================
# ESP32-S3 Audio Kit Enclosure - Render Script
# ==============================================================================
# Generates PNG preview renders and exports STLs for 3D printing.
# Requires OpenSCAD on PATH.
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OUT_DIR="$SCRIPT_DIR/out"
mkdir -p "$OUT_DIR"

echo "=== Rendering Previews ==="
echo "1. Assembled Preview (Transparent shell with internal components)..."
openscad -o "$OUT_DIR/preview.png" --autocenter --viewall --imgsize=1600,1200 --colorscheme="Tomorrow Night" -D 'part="preview"' enclosure.scad

echo "2. Exploded View (Stack progression)..."
openscad -o "$OUT_DIR/exploded.png" --autocenter --viewall --imgsize=1600,1200 --colorscheme="Tomorrow Night" -D 'part="exploded"' enclosure.scad

echo "3. Solid Assembled Case..."
openscad -o "$OUT_DIR/assembled.png" --autocenter --viewall --imgsize=1600,1200 --colorscheme="Tomorrow Night" -D 'part="assembled"' enclosure.scad

echo "4. Print Bed Setup (Shell + Lid flat side-by-side)..."
openscad -o "$OUT_DIR/print_bed.png" --autocenter --viewall --imgsize=1600,1200 --colorscheme="Tomorrow Night" print_bed.scad

echo "=== Exporting 3D Printable STLs ==="
echo "5. Top Shell STL (oriented face-down for support-free printing)..."
openscad -o "$OUT_DIR/shell.stl" -D 'part="shell"' enclosure.scad

echo "6. Bottom Lid STL (oriented flat for support-free printing)..."
openscad -o "$OUT_DIR/lid.stl" -D 'part="lid"' enclosure.scad

echo "7. Combined Print Bed STL (Both parts on one print plate)..."
openscad -o "$OUT_DIR/print_bed.stl" print_bed.scad

echo "=== Done! All assets generated in $OUT_DIR ==="
ls -lh "$OUT_DIR"
