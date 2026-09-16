// Zita-Rev1 feedback delay network reverb, stereo out, with dry/wet mix.
declare name "zita";
import("stdfaust.lib");
wet  = hslider("wet", 0.4, 0, 1, 0.01);
t60  = hslider("t60", 2.5, 0.3, 12, 0.1);
pre  = hslider("predelay", 30, 5, 100, 1);
damp = hslider("damp", 5000, 1000, 12000, 100);
rev  = re.zita_rev1_stereo(pre, 200, damp, t60, t60, 32000);
process = _ <: (_,_ : rev : par(i, 2, *(wet))), (_,_ : par(i, 2, *(1 - wet))) :> _,_;
