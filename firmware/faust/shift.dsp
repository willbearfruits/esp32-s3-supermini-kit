// Granular pitch shifter (ef.transpose) with dry/wet mix for harmonies.
declare name "shift";
import("stdfaust.lib");
semi = hslider("semi", 12, -24, 24, 1);
mix  = hslider("mix", 1, 0, 1, 0.01);
process = _ <: *(1 - mix), (ef.transpose(1024, 256, semi) : *(mix)) :> _;
