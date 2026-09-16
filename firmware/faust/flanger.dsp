// Flanger / chorus: LFO-modulated delay with feedback, dry/wet mix.
declare name "flanger";
import("stdfaust.lib");
rate  = hslider("rate", 0.3, 0.02, 8, 0.01);
depth = hslider("depth", 6, 0.5, 20, 0.1);    // ms
base  = hslider("base", 1, 0.1, 30, 0.1);     // ms
fb    = hslider("fb", 0.5, -0.95, 0.95, 0.01);
mix   = hslider("mix", 0.5, 0, 1, 0.01);
dmax  = 2048;
curdel = (base + depth * (1 + sin(2 * ma.PI * os.lf_sawpos(rate))) * 0.5) * ma.SR / 1000 : min(dmax - 1);
process = _ <: *(1 - mix), (pf.flanger_mono(dmax, curdel, 1, fb, 0) : *(mix)) :> _;
