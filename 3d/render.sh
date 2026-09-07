#!/bin/sh
# Preview PNGs and STLs for every option. Needs openscad on PATH.
set -e
cd "$(dirname "$0")"
mkdir -p out
for f in handheld ocarina cup; do
    openscad -o out/$f.png --autocenter --viewall --imgsize=1200,800 $f.scad
done
openscad -o out/handheld_shell.stl -D 'part="shell"' handheld.scad
openscad -o out/handheld_lid.stl   -D 'part="lid"'   handheld.scad
openscad -o out/ocarina_body.stl   -D 'part="body"'  ocarina.scad
openscad -o out/ocarina_bay.stl    -D 'part="bay"'   ocarina.scad
openscad -o out/cup.stl cup.scad
