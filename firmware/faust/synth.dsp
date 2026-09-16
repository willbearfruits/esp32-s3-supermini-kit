// Mic-following synth: the firmware writes "freq" and "gate" from its pitch
// tracker every block. Oscillator through a Moog ladder with an ADSR, mixed
// with the mic.
declare name "synth";
import("stdfaust.lib");
freq   = hslider("freq", 110, 30, 2000, 0.01);
gate   = hslider("gate", 0, 0, 1, 1);
wave   = hslider("wave", 0, 0, 2, 1);
cutoff = hslider("cutoff", 1500, 100, 8000, 1);
res    = hslider("res", 0.3, 0, 0.95, 0.01);
octave = hslider("octave", 0, -2, 1, 1);
mic    = hslider("mic", 0.3, 0, 1, 0.01);
f   = freq * pow(2, octave) : si.smoo;
osc = select3(wave, os.sawtooth(f), os.square(f), os.triangle(f));
env = en.adsr(0.01, 0.2, 0.7, 0.3, gate);
process(x) = osc * env : ve.moog_vcf(res, cutoff * (0.3 + 0.7 * env)) : *(0.35) : +(x * mic);
