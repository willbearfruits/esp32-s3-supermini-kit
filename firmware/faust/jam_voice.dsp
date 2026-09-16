// JAM synth voice: one oscillator (saw/square/tri/pulse) plus sub, ADSR,
// Moog ladder with envelope sweep, soft drive. Mono, retriggered by the
// engine through "freq" / "gate" / "vel". "bright" is the live expression
// (joystick), a multiplier on the cutoff.
declare name "jam_voice";
import("stdfaust.lib");
freq   = hslider("freq", 110, 20, 4000, 0.01) : si.smoo;
gate   = hslider("gate", 0, 0, 1, 1);
vel    = hslider("vel", 1, 0, 1, 0.01);
wave   = hslider("wave", 0, 0, 2, 1);
cutoff = hslider("cutoff", 1500, 60, 12000, 1);
res    = hslider("res", 0.3, 0, 0.95, 0.01);
envf   = hslider("envf", 0.5, 0, 1, 0.01);
att    = hslider("att", 0.005, 0.001, 1, 0.001);
dec    = hslider("dec", 0.2, 0.01, 2, 0.01);
sus    = hslider("sus", 0.7, 0, 1, 0.01);
rel    = hslider("rel", 0.2, 0.01, 3, 0.01);
sub    = hslider("sub", 0, 0, 1, 0.01);
drive  = hslider("drive", 0, 0, 1, 0.01);
bright = hslider("bright", 1, 0.2, 4, 0.01) : si.smoo;
gain   = hslider("gain", 0.5, 0, 1, 0.01);
env = en.adsr(att, dec, sus, rel, gate);
// saw and square are band-limited, the triangle and the sub are naive (cheap, low harmonics anyway)
osc = select3(wave, os.sawtooth(freq), os.lf_squarewave(freq), os.lf_triangle(freq)) + sub * os.lf_squarewave(freq / 2);
fc  = min(12000, cutoff * bright * (1 + envf * 6 * env));
process = osc * env * vel : ve.moog_vcf(res, fc) : ef.cubicnl(drive * 0.8, 0) : *(gain);
