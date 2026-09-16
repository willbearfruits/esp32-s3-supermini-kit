/* ------------------------------------------------------------
name: "synth"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_synth -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_synth_H__
#define  __kfx_synth_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_synth
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

static float kfx_synth_faustpower4_f(float value) {
	return value * value * value * value;
}
static float kfx_synth_faustpower2_f(float value) {
	return value * value;
}

class kfx_synth : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	int iVec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	int iRec1[2];
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fRec2[2];
	float fConst2;
	float fConst3;
	float fConst4;
	FAUSTFLOAT fHslider2;
	float fConst5;
	FAUSTFLOAT fHslider3;
	FAUSTFLOAT fHslider4;
	float fConst6;
	float fConst7;
	FAUSTFLOAT fHslider5;
	FAUSTFLOAT fHslider6;
	float fRec8[2];
	float fConst8;
	float fRec9[2];
	float fVec2[2];
	int IOTA0;
	float fVec3[4096];
	float fConst9;
	float fConst10;
	float fRec7[2];
	float fConst11;
	float fRec10[2];
	float fRec6[2];
	float fRec5[2];
	float fRec4[2];
	float fRec3[2];
	float fRec0[2];
	
 public:
	kfx_synth() {
	}
	
	kfx_synth(const kfx_synth&) = default;
	
	virtual ~kfx_synth() = default;
	
	kfx_synth& operator=(const kfx_synth&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_synth -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("envelopes.lib/adsr:author", "Yann Orlarey and Andrey Bundin");
		m->declare("envelopes.lib/author", "GRAME");
		m->declare("envelopes.lib/copyright", "GRAME");
		m->declare("envelopes.lib/license", "LGPL with exception");
		m->declare("envelopes.lib/name", "Faust Envelope Library");
		m->declare("envelopes.lib/version", "1.3.0");
		m->declare("filename", "synth.dsp");
		m->declare("filters.lib/lowpass0_highpass1", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/pole:author", "Julius O. Smith III");
		m->declare("filters.lib/pole:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/pole:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "synth");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/saw2ptr:author", "Julius O. Smith III");
		m->declare("oscillators.lib/saw2ptr:license", "STK-4.3");
		m->declare("oscillators.lib/sawN:author", "Julius O. Smith III");
		m->declare("oscillators.lib/sawN:license", "STK-4.3");
		m->declare("oscillators.lib/version", "1.7.0");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
		m->declare("vaeffects.lib/moog_vcf:author", "Julius O. Smith III");
		m->declare("vaeffects.lib/moog_vcf:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("vaeffects.lib/moog_vcf:license", "MIT-style STK-4.3 license");
		m->declare("vaeffects.lib/name", "Faust Virtual Analog Filter Effect Library");
		m->declare("vaeffects.lib/version", "1.5.0");
	}

	virtual int getNumInputs() {
		return 1;
	}
	virtual int getNumOutputs() {
		return 1;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = 1.0f / std::max<float>(1.0f, 0.3f * fConst0);
		fConst2 = std::max<float>(1.0f, 0.01f * fConst0);
		fConst3 = 1.0f / fConst2;
		fConst4 = 0.3f / std::max<float>(1.0f, 0.2f * fConst0);
		fConst5 = 6.2831855f / fConst0;
		fConst6 = 44.1f / fConst0;
		fConst7 = 1.0f - fConst6;
		fConst8 = 1.0f / fConst0;
		fConst9 = 0.5f * fConst0;
		fConst10 = 0.25f * fConst0;
		fConst11 = 4.0f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.3f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider2 = static_cast<FAUSTFLOAT>(1.5e+03f);
		fHslider3 = static_cast<FAUSTFLOAT>(0.3f);
		fHslider4 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider5 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider6 = static_cast<FAUSTFLOAT>(1.1e+02f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			iRec1[l2] = 0;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec8[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec9[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec2[l6] = 0.0f;
		}
		IOTA0 = 0;
		for (int l7 = 0; l7 < 4096; l7 = l7 + 1) {
			fVec3[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec7[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec10[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec6[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec5[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec4[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec3[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec0[l14] = 0.0f;
		}
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual kfx_synth* clone() {
		return new kfx_synth(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("synth");
		ui_interface->addHorizontalSlider("cutoff", &fHslider2, FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(8e+03f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("freq", &fHslider6, FAUSTFLOAT(1.1e+02f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(2e+03f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("gate", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("mic", &fHslider0, FAUSTFLOAT(0.3f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("octave", &fHslider5, FAUSTFLOAT(0.0f), FAUSTFLOAT(-2.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("res", &fHslider3, FAUSTFLOAT(0.3f), FAUSTFLOAT(0.0f), FAUSTFLOAT(0.95f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("wave", &fHslider4, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2.0f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = static_cast<float>(fHslider1);
		int iSlow2 = fSlow1 == 0.0f;
		float fSlow3 = fConst5 * static_cast<float>(fHslider2);
		float fSlow4 = 4.0f * std::max<float>(0.0f, std::min<float>(static_cast<float>(fHslider3), 0.999999f));
		float fSlow5 = static_cast<float>(fHslider4);
		int iSlow6 = fSlow5 == 0.0f;
		int iSlow7 = fSlow5 == 1.0f;
		float fSlow8 = fConst6 * static_cast<float>(fHslider6) * std::pow(2.0f, static_cast<float>(fHslider5));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fVec1[0] = fSlow1;
			iRec1[0] = iSlow2 * (iRec1[1] + 1);
			float fTemp0 = fSlow1 + fRec2[1] * static_cast<float>(fVec1[1] >= fSlow1);
			fRec2[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::max<float>(0.0f, std::min<float>(fConst3 * fRec2[0], std::max<float>(fConst4 * (fConst2 - fRec2[0]) + 1.0f, 0.7f)) * (1.0f - fConst1 * static_cast<float>(iRec1[0])));
			float fTemp2 = fSlow3 * (0.7f * fTemp1 + 0.3f);
			float fTemp3 = 1.0f - fTemp2;
			float fTemp4 = fSlow8 + fConst7 * fRec8[1];
			fRec8[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = std::max<float>(fRec8[0], 23.44895f);
			float fTemp6 = std::max<float>(2e+01f, std::fabs(fTemp5));
			float fTemp7 = ((1 - iVec0[1]) ? 0.0f : fRec9[1] + fConst8 * fTemp6);
			float fTemp8 = fTemp7 - std::floor(fTemp7);
			fRec9[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = kfx_synth_faustpower2_f(2.0f * fRec9[0] + -1.0f);
			fVec2[0] = fTemp9;
			float fTemp10 = static_cast<float>(iVec0[1]) * (fTemp9 - fVec2[1]) / fTemp6;
			fVec3[IOTA0 & 4095] = fTemp10;
			float fTemp11 = std::max<float>(0.0f, std::min<float>(2047.0f, fConst9 / fTemp5));
			int iTemp12 = static_cast<int>(fTemp11);
			float fTemp13 = std::floor(fTemp11);
			float fTemp14 = fConst10 * (fTemp10 - fVec3[(IOTA0 - iTemp12) & 4095] * (fTemp13 + (1.0f - fTemp11)) - (fTemp11 - fTemp13) * fVec3[(IOTA0 - (iTemp12 + 1)) & 4095]);
			float fTemp15 = 0.999f * fRec7[1] + fTemp14;
			fRec7[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = std::max<float>(1.1920929e-07f, std::fabs(fRec8[0]));
			float fTemp17 = fRec10[1] + fConst8 * fTemp16;
			float fTemp18 = fTemp17 + -1.0f;
			int iTemp19 = fTemp18 < 0.0f;
			float fTemp20 = ((iTemp19) ? fTemp17 : fTemp18);
			fRec10[0] = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			float fTemp21 = ((iTemp19) ? fTemp17 : fTemp17 + (1.0f - fConst0 / fTemp16) * fTemp18);
			float fRec11 = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fTemp3 * fRec6[1] + fTemp1 * ((iSlow6) ? 2.0f * fRec11 + -1.0f : ((iSlow7) ? fTemp14 : fConst11 * fRec8[0] * fRec7[0])) - fSlow4 * fRec0[1];
			fRec6[0] = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = fRec6[0] + fTemp3 * fRec5[1];
			fRec5[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = fRec5[0] + fTemp3 * fRec4[1];
			fRec4[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fRec4[0] + fRec3[1] * fTemp3;
			fRec3[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = fRec3[0] * kfx_synth_faustpower4_f(fTemp2);
			fRec0[0] = ((std::fabs(fTemp26) > 1.1754944e-38f) ? fTemp26 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(0.35f * fRec0[0] + fSlow0 * static_cast<float>(input0[i0]));
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			iRec1[1] = iRec1[0];
			fRec2[1] = fRec2[0];
			fRec8[1] = fRec8[0];
			fRec9[1] = fRec9[0];
			fVec2[1] = fVec2[0];
			IOTA0 = IOTA0 + 1;
			fRec7[1] = fRec7[0];
			fRec10[1] = fRec10[0];
			fRec6[1] = fRec6[0];
			fRec5[1] = fRec5[0];
			fRec4[1] = fRec4[0];
			fRec3[1] = fRec3[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
