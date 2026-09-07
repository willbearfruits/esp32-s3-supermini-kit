// Acoustic helpers. Units: mm for lengths, mm^2 area, mm^3 volume, Hz.
// See docs/acoustics.md for where the formulas come from and their limits.

c_sound = 343000;   // mm/s at 20 C

// Effective neck length: physical length plus end corrections on both ends
function neck_eff(L, d) = L + 1.7 * d / 2;

// Helmholtz resonance of a cavity V with one opening of area A and neck length L
function helmholtz_f(V, A, L, d) = c_sound / (2 * PI) * sqrt(A / (V * neck_eff(L, d)));

// Cavity volume that puts the resonance at f for a given opening
function helmholtz_V(f, A, L, d) = A / (neck_eff(L, d) * pow(2 * PI * f / c_sound, 2));

// Ocarina: several openings in parallel add their A/L_eff terms
function conductance(A, L, d) = A / neck_eff(L, d);
function ocarina_f(V, cond_sum) = c_sound / (2 * PI) * sqrt(cond_sum / V);

// Tube resonances
function tube_closed_f(L, n=1) = (2*n - 1) * c_sound / (4 * L);   // closed one end, odd harmonics
function tube_open_f(L, n=1)   = n * c_sound / (2 * L);            // open both ends

// Round hole area
function circle_area(d) = PI * d * d / 4;

// Diameter of a hole with area A
function circle_d(A) = 2 * sqrt(A / PI);

// Hole diameter that raises an ocarina from f_closed to f_target when opened,
// given the chamber volume V and the wall thickness t. Solved numerically
// because the end correction depends on the diameter.
function ocarina_hole_d(V, cond_closed, f_target, t, d=3, i=0) =
    i > 40 ? d :
    let(f = ocarina_f(V, cond_closed + conductance(circle_area(d), t, d)))
    abs(f - f_target) < 0.5 ? d :
    ocarina_hole_d(V, cond_closed, f_target, t, d * (f < f_target ? 1.03 : 0.97), i + 1);

function note_freq(midi) = 440 * pow(2, (midi - 69) / 12);
