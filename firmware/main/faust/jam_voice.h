/* ------------------------------------------------------------
name: "jam_voice"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_jam_voice -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_jam_voice_H__
#define  __kfx_jam_voice_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_jam_voice
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

static float kfx_jam_voice_faustpower4_f(float value) {
	return value * value * value * value;
}
static float kfx_jam_voice_faustpower2_f(float value) {
	return value * value;
}

class kfx_jam_voice : public dsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	float fVec1[2];
	int iRec1[2];
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	float fRec2[2];
	FAUSTFLOAT fHslider2;
	FAUSTFLOAT fHslider3;
	FAUSTFLOAT fHslider4;
	FAUSTFLOAT fHslider5;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider6;
	float fRec3[2];
	FAUSTFLOAT fHslider7;
	float fConst3;
	FAUSTFLOAT fHslider8;
	FAUSTFLOAT fHslider9;
	float fRec9[2];
	float fConst4;
	float fRec8[2];
	FAUSTFLOAT fHslider10;
	FAUSTFLOAT fHslider11;
	float fConst5;
	float fRec10[2];
	float fRec11[2];
	FAUSTFLOAT fHslider12;
	float fRec7[2];
	float fRec6[2];
	float fRec5[2];
	float fRec4[2];
	float fRec0[2];
	FAUSTFLOAT fHslider13;
	FAUSTFLOAT fHslider14;
	
 public:
	kfx_jam_voice() {
	}
	
	kfx_jam_voice(const kfx_jam_voice&) = default;
	
	virtual ~kfx_jam_voice() = default;
	
	kfx_jam_voice& operator=(const kfx_jam_voice&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_jam_voice -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("envelopes.lib/adsr:author", "Yann Orlarey and Andrey Bundin");
		m->declare("envelopes.lib/author", "GRAME");
		m->declare("envelopes.lib/copyright", "GRAME");
		m->declare("envelopes.lib/license", "LGPL with exception");
		m->declare("envelopes.lib/name", "Faust Envelope Library");
		m->declare("envelopes.lib/version", "1.3.0");
		m->declare("filename", "jam_voice.dsp");
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
		m->declare("misceffects.lib/cubicnl:author", "Julius O. Smith III");
		m->declare("misceffects.lib/cubicnl:license", "STK-4.3");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.5.2");
		m->declare("name", "jam_voice");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "STK-4.3");
		m->declare("oscillators.lib/lf_triangle:author", "Bart Brouns");
		m->declare("oscillators.lib/lf_triangle:licence", "STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/saw1:author", "Bart Brouns");
		m->declare("oscillators.lib/saw1:licence", "STK-4.3");
		m->declare("oscillators.lib/saw2ptr:author", "Julius O. Smith III");
		m->declare("oscillators.lib/saw2ptr:license", "STK-4.3");
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
		return 0;
	}
	virtual int getNumOutputs() {
		return 1;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = 44.1f / fConst0;
		fConst2 = 1.0f - fConst1;
		fConst3 = 6.2831855f / fConst0;
		fConst4 = 0.5f / fConst0;
		fConst5 = 1.0f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.2f);
		fHslider2 = static_cast<FAUSTFLOAT>(0.005f);
		fHslider3 = static_cast<FAUSTFLOAT>(0.2f);
		fHslider4 = static_cast<FAUSTFLOAT>(0.7f);
		fHslider5 = static_cast<FAUSTFLOAT>(0.5f);
		fHslider6 = static_cast<FAUSTFLOAT>(1.0f);
		fHslider7 = static_cast<FAUSTFLOAT>(1.5e+03f);
		fHslider8 = static_cast<FAUSTFLOAT>(0.3f);
		fHslider9 = static_cast<FAUSTFLOAT>(1.1e+02f);
		fHslider10 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider11 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider12 = static_cast<FAUSTFLOAT>(1.0f);
		fHslider13 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider14 = static_cast<FAUSTFLOAT>(0.5f);
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
			fRec3[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec9[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec8[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec10[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec11[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec7[l9] = 0.0f;
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
			fRec0[l13] = 0.0f;
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
	
	virtual kfx_jam_voice* clone() {
		return new kfx_jam_voice(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("jam_voice");
		ui_interface->addHorizontalSlider("att", &fHslider2, FAUSTFLOAT(0.005f), FAUSTFLOAT(0.001f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.001f));
		ui_interface->addHorizontalSlider("bright", &fHslider6, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.2f), FAUSTFLOAT(4.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("cutoff", &fHslider7, FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(6e+01f), FAUSTFLOAT(1.2e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("dec", &fHslider3, FAUSTFLOAT(0.2f), FAUSTFLOAT(0.01f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("drive", &fHslider13, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("envf", &fHslider5, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("freq", &fHslider9, FAUSTFLOAT(1.1e+02f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(4e+03f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("gain", &fHslider14, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("gate", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("rel", &fHslider1, FAUSTFLOAT(0.2f), FAUSTFLOAT(0.01f), FAUSTFLOAT(3.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("res", &fHslider8, FAUSTFLOAT(0.3f), FAUSTFLOAT(0.0f), FAUSTFLOAT(0.95f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("sub", &fHslider10, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("sus", &fHslider4, FAUSTFLOAT(0.7f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("vel", &fHslider12, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("wave", &fHslider11, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2.0f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = static_cast<float>(fHslider0);
		int iSlow1 = fSlow0 == 0.0f;
		float fSlow2 = 1.0f / std::max<float>(1.0f, fConst0 * static_cast<float>(fHslider1));
		float fSlow3 = std::max<float>(1.0f, fConst0 * static_cast<float>(fHslider2));
		float fSlow4 = 1.0f / fSlow3;
		float fSlow5 = static_cast<float>(fHslider4);
		float fSlow6 = (1.0f - fSlow5) / std::max<float>(1.0f, fConst0 * static_cast<float>(fHslider3));
		float fSlow7 = 6.0f * static_cast<float>(fHslider5);
		float fSlow8 = fConst1 * static_cast<float>(fHslider6);
		float fSlow9 = static_cast<float>(fHslider7);
		float fSlow10 = 4.0f * std::max<float>(0.0f, std::min<float>(static_cast<float>(fHslider8), 0.999999f));
		float fSlow11 = fConst1 * static_cast<float>(fHslider9);
		float fSlow12 = static_cast<float>(fHslider10);
		float fSlow13 = static_cast<float>(fHslider11);
		int iSlow14 = fSlow13 == 0.0f;
		int iSlow15 = fSlow13 == 1.0f;
		float fSlow16 = static_cast<float>(fHslider12);
		float fSlow17 = std::pow(1e+01f, 1.6f * static_cast<float>(fHslider13));
		float fSlow18 = static_cast<float>(fHslider14);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fVec1[0] = fSlow0;
			iRec1[0] = iSlow1 * (iRec1[1] + 1);
			float fTemp0 = fSlow0 + fRec2[1] * static_cast<float>(fVec1[1] >= fSlow0);
			fRec2[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::max<float>(0.0f, std::min<float>(fSlow4 * fRec2[0], std::max<float>(fSlow6 * (fSlow3 - fRec2[0]) + 1.0f, fSlow5)) * (1.0f - fSlow2 * static_cast<float>(iRec1[0])));
			float fTemp2 = fSlow8 + fConst2 * fRec3[1];
			fRec3[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = fConst3 * std::min<float>(1.2e+04f, fSlow9 * fRec3[0] * (fSlow7 * fTemp1 + 1.0f));
			float fTemp4 = 1.0f - fTemp3;
			int iTemp5 = 1 - iVec0[1];
			float fTemp6 = fSlow11 + fConst2 * fRec9[1];
			fRec9[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = ((iTemp5) ? 0.0f : fRec8[1] + fConst4 * fRec9[0]);
			float fTemp8 = fTemp7 - std::floor(fTemp7);
			fRec8[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = ((iTemp5) ? 0.0f : fRec10[1] + fConst5 * fRec9[0]);
			float fTemp10 = fTemp9 - std::floor(fTemp9);
			fRec10[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = std::max<float>(1.1920929e-07f, std::fabs(fRec9[0]));
			float fTemp12 = fRec11[1] + fConst5 * fTemp11;
			float fTemp13 = fTemp12 + -1.0f;
			int iTemp14 = fTemp13 < 0.0f;
			float fTemp15 = ((iTemp14) ? fTemp12 : fTemp13);
			fRec11[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = ((iTemp14) ? fTemp12 : fTemp12 + (1.0f - fConst0 / fTemp11) * fTemp13);
			float fRec12 = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			float fTemp17 = fTemp4 * fRec7[1] + fSlow16 * fTemp1 * (((iSlow14) ? 2.0f * fRec12 + -1.0f : ((iSlow15) ? 2.0f * static_cast<float>(fRec10[0] <= 0.5f) + -1.0f : 2.0f * (1.0f - std::fabs(2.0f * fRec10[0] + -1.0f)) + -1.0f)) + fSlow12 * (2.0f * static_cast<float>(fRec8[0] <= 0.5f) + -1.0f)) - fSlow10 * fRec0[1];
			fRec7[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fRec7[0] + fTemp4 * fRec6[1];
			fRec6[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			float fTemp19 = fRec6[0] + fTemp4 * fRec5[1];
			fRec5[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fRec5[0] + fRec4[1] * fTemp4;
			fRec4[0] = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			float fTemp21 = fRec4[0] * kfx_jam_voice_faustpower4_f(fTemp3);
			fRec0[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = std::max<float>(-1.0f, std::min<float>(1.0f, fSlow17 * fRec0[0]));
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow18 * fTemp22 * (1.0f - 0.33333334f * kfx_jam_voice_faustpower2_f(fTemp22)));
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			iRec1[1] = iRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec9[1] = fRec9[0];
			fRec8[1] = fRec8[0];
			fRec10[1] = fRec10[0];
			fRec11[1] = fRec11[0];
			fRec7[1] = fRec7[0];
			fRec6[1] = fRec6[0];
			fRec5[1] = fRec5[0];
			fRec4[1] = fRec4[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
