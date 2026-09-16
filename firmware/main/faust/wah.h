/* ------------------------------------------------------------
name: "wah"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_wah -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_wah_H__
#define  __kfx_wah_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_wah
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

static float kfx_wah_faustpower2_f(float value) {
	return value * value;
}

class kfx_wah : public dsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fConst2;
	float fConst3;
	float fRec3[2];
	float fConst4;
	float fRec2[2];
	FAUSTFLOAT fHslider1;
	FAUSTFLOAT fHslider2;
	float fConst5;
	float fRec4[2];
	FAUSTFLOAT fHslider3;
	float fConst6;
	float fRec1[2];
	float fConst7;
	float fRec5[2];
	float fRec6[2];
	float fRec0[3];
	
 public:
	kfx_wah() {
	}
	
	kfx_wah(const kfx_wah&) = default;
	
	virtual ~kfx_wah() = default;
	
	kfx_wah& operator=(const kfx_wah&) = default;
	
	void metadata(Meta* m) { 
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.3.0");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_wah -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "wah.dsp");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1", "MIT-style STK-4.3 license");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/tf2:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "wah");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.7.0");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
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
		fConst1 = std::exp(-(1e+02f / fConst0));
		fConst2 = std::exp(-(6.6666665f / fConst0));
		fConst3 = 1.0f - fConst2;
		fConst4 = 1.0f - fConst1;
		fConst5 = 1.0f / fConst0;
		fConst6 = 1413.7167f / fConst0;
		fConst7 = 2827.4333f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider1 = static_cast<FAUSTFLOAT>(6.0f);
		fHslider2 = static_cast<FAUSTFLOAT>(0.5f);
		fHslider3 = static_cast<FAUSTFLOAT>(1.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec3[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec2[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec4[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec1[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec5[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec6[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 3; l7 = l7 + 1) {
			fRec0[l7] = 0.0f;
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
	
	virtual kfx_wah* clone() {
		return new kfx_wah(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("wah");
		ui_interface->addHorizontalSlider("depth", &fHslider3, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("mode", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("rate", &fHslider2, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.05f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("sens", &fHslider1, FAUSTFLOAT(6.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		int iSlow0 = static_cast<int>(static_cast<float>(fHslider0));
		float fSlow1 = static_cast<float>(fHslider1);
		float fSlow2 = fConst5 * static_cast<float>(fHslider2);
		float fSlow3 = 0.5f * static_cast<float>(fHslider3);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			float fTemp0 = static_cast<float>(input0[i0]);
			float fTemp1 = std::fabs(fTemp0);
			float fTemp2 = std::max<float>(fTemp1, fConst2 * fRec3[1] + fConst3 * fTemp1);
			fRec3[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = fConst4 * fRec3[0] + fConst1 * fRec2[1];
			fRec2[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = ((1 - iVec0[1]) ? 0.0f : fSlow2 + fRec4[1]);
			float fTemp5 = fTemp4 - std::floor(fTemp4);
			fRec4[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = ((iSlow0) ? fSlow3 * (std::sin(6.2831855f * fRec4[0]) + 1.0f) : std::min<float>(1.0f, fSlow1 * fRec2[0]));
			float fTemp7 = std::pow(2.0f, 2.3f * fTemp6);
			float fTemp8 = 1.0f - fConst6 * (fTemp7 / std::pow(2.0f, 2.0f * (1.0f - fTemp6) + 1.0f));
			float fTemp9 = 0.001f * kfx_wah_faustpower2_f(fTemp8) + 0.999f * fRec1[1];
			fRec1[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = 0.999f * fRec5[1] - 0.002f * fTemp8 * std::cos(fConst7 * fTemp7);
			fRec5[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = 0.0001f * std::pow(4.0f, fTemp6) + 0.999f * fRec6[1];
			fRec6[0] = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = fTemp0 * fRec6[0] - (fRec5[0] * fRec0[1] + fRec1[0] * fRec0[2]);
			fRec0[0] = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(fRec0[0] - fRec0[1]);
			iVec0[1] = iVec0[0];
			fRec3[1] = fRec3[0];
			fRec2[1] = fRec2[0];
			fRec4[1] = fRec4[0];
			fRec1[1] = fRec1[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec0[2] = fRec0[1];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
