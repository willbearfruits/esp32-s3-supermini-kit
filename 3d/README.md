# 3D-printable bodies

Parametric OpenSCAD (tested with 2021.01). Three options that share one
parts library:

| File | Option | Idea |
|------|--------|------|
| `handheld.scad` | A, slab | Game-Boy-shaped box: OLED, encoder, joystick on top, sealed speaker sub-box, mic on the top edge, battery under the lid. |
| `ocarina.scad` | B, ocarina | A Helmholtz chamber sized for a chosen lowest note, fipple and four finger holes; the mic listens inside the chamber. Electronics in a bay underneath. |
| `cup.scad` | C, cup on a wand | Mic at the bottom of a cup you press to your mouth, controls along the handle. Maximum isolation for beatboxing. |

`parts.scad` holds every module's size. **All of them are typical values,
not measurements.** Measure your boards and fix the numbers before printing.
`acoustics.scad` has the resonance formulas, explained in
[docs/acoustics.md](../docs/acoustics.md).

```sh
./render.sh            # previews in out/*.png and STLs in out/*.stl
openscad handheld.scad # or open interactively
```
