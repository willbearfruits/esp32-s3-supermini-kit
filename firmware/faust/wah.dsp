// Crybaby wah driven by the mic envelope (auto-wah) or by an LFO.
declare name "wah";
import("stdfaust.lib");
mode  = hslider("mode", 0, 0, 1, 1);      // 0 envelope, 1 LFO
sens  = hslider("sens", 6, 1, 30, 0.1);
rate  = hslider("rate", 0.5, 0.05, 8, 0.01);
depth = hslider("depth", 1, 0, 1, 0.01);
env   = an.amp_follower_ud(0.01, 0.15) : *(sens) : min(1);
lfo   = (sin(2 * ma.PI * os.lf_sawpos(rate)) + 1) * 0.5 * depth;
ctrl(x) = select2(mode, env(x), lfo);
process(x) = ve.crybaby(ctrl(x), x);
