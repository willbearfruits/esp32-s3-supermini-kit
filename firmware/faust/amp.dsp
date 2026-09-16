// Amp sim: pre gain, cubic soft clipper, speaker band-pass, post gain.
declare name "amp";
import("stdfaust.lib");
drive = hslider("drive", 0.5, 0, 1, 0.01);
pre   = hslider("pre", 4, 1, 40, 0.5);
lo    = hslider("lo", 120, 60, 1500, 10);
hi    = hslider("hi", 5000, 800, 12000, 100);
post  = hslider("post", 0.5, 0, 1, 0.01);
process = *(pre) : ef.cubicnl(drive, 0) : ef.speakerbp(lo, hi) : *(post);
