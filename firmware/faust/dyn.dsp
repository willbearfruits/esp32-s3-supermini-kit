// Dynamics: compressor, make-up gain, then an 1176-style limiter.
declare name "dyn";
import("stdfaust.lib");
ratio  = hslider("ratio", 4, 1, 20, 0.1);
thresh = hslider("thresh", -30, -60, 0, 1);
att    = hslider("att", 0.005, 0.001, 0.1, 0.001);
rel    = hslider("rel", 0.15, 0.01, 1, 0.01);
gain   = hslider("gain", 12, 0, 40, 0.5);
process = co.compressor_mono(ratio, thresh, att, rel) : *(ba.db2linear(gain)) : co.limiter_1176_R4_mono;
