/* ------------------------------------------------------------
name: "amp"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_amp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_amp_H__
#define  __kfx_amp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_amp
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

static float kfx_amp_faustpower2_f(float value) {
	return value * value;
}

class kfx_amp : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	int fSampleRate;
	float fConst0;
	FAUSTFLOAT fHslider1;
	FAUSTFLOAT fHslider2;
	FAUSTFLOAT fHslider3;
	float fVec0[2];
	float fRec3[2];
	float fVec1[2];
	float fRec2[2];
	float fRec1[3];
	float fRec0[3];
	FAUSTFLOAT fHslider4;
	
 public:
	kfx_amp() {
	}
	
	kfx_amp(const kfx_amp&) = default;
	
	virtual ~kfx_amp() = default;
	
	kfx_amp& operator=(const kfx_amp&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_amp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "amp.dsp");
		m->declare("filters.lib/dcblockerat:author", "Julius O. Smith III");
		m->declare("filters.lib/dcblockerat:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/dcblockerat:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/pole:author", "Julius O. Smith III");
		m->declare("filters.lib/pole:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/pole:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf2:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf2s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2s:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("filters.lib/zero:author", "Julius O. Smith III");
		m->declare("filters.lib/zero:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/zero:license", "MIT-style STK-4.3 license");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("misceffects.lib/cubicnl:author", "Julius O. Smith III");
		m->declare("misceffects.lib/cubicnl:license", "STK-4.3");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/speakerbp:author", "Julius O. Smith III");
		m->declare("misceffects.lib/speakerbp:license", "STK-4.3");
		m->declare("misceffects.lib/version", "2.5.2");
		m->declare("name", "amp");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
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
		fConst0 = 3.1415927f / std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(5e+03f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.5f);
		fHslider2 = static_cast<FAUSTFLOAT>(4.0f);
		fHslider3 = static_cast<FAUSTFLOAT>(1.2e+02f);
		fHslider4 = static_cast<FAUSTFLOAT>(0.5f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec3[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 3; l4 = l4 + 1) {
			fRec1[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 3; l5 = l5 + 1) {
			fRec0[l5] = 0.0f;
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
	
	virtual kfx_amp* clone() {
		return new kfx_amp(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("amp");
		ui_interface->addHorizontalSlider("drive", &fHslider1, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("hi", &fHslider0, FAUSTFLOAT(5e+03f), FAUSTFLOAT(8e+02f), FAUSTFLOAT(1.2e+04f), FAUSTFLOAT(1e+02f));
		ui_interface->addHorizontalSlider("lo", &fHslider3, FAUSTFLOAT(1.2e+02f), FAUSTFLOAT(6e+01f), FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(1e+01f));
		ui_interface->addHorizontalSlider("post", &fHslider4, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("pre", &fHslider2, FAUSTFLOAT(4.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(4e+01f), FAUSTFLOAT(0.5f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::tan(fConst0 * static_cast<float>(fHslider0));
		float fSlow1 = 2.0f * (1.0f - 1.0f / kfx_amp_faustpower2_f(fSlow0));
		float fSlow2 = 1.0f / fSlow0;
		float fSlow3 = (fSlow2 + -0.76536685f) / fSlow0 + 1.0f;
		float fSlow4 = (fSlow2 + 0.76536685f) / fSlow0 + 1.0f;
		float fSlow5 = 1.0f / fSlow4;
		float fSlow6 = (fSlow2 + -1.847759f) / fSlow0 + 1.0f;
		float fSlow7 = 1.0f / ((fSlow2 + 1.847759f) / fSlow0 + 1.0f);
		float fSlow8 = static_cast<float>(fHslider2) * std::pow(1e+01f, 2.0f * static_cast<float>(fHslider1));
		float fSlow9 = fConst0 * static_cast<float>(fHslider3);
		float fSlow10 = 1.0f / (fSlow9 + 1.0f);
		float fSlow11 = 1.0f - fSlow9;
		float fSlow12 = static_cast<float>(fHslider4) / fSlow4;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = std::max<float>(-1.0f, std::min<float>(1.0f, fSlow8 * static_cast<float>(input0[i0])));
			float fTemp1 = fTemp0 * (1.0f - 0.33333334f * kfx_amp_faustpower2_f(fTemp0));
			fVec0[0] = fSlow10 * fTemp1;
			float fTemp2 = fSlow10 * (fTemp1 + fSlow11 * fRec3[1]) - fVec0[1];
			fRec3[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			fVec1[0] = fSlow10 * fRec3[0];
			float fTemp3 = fSlow10 * (fRec3[0] + fSlow11 * fRec2[1]) - fVec1[1];
			fRec2[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = fRec2[0] - fSlow7 * (fSlow6 * fRec1[2] + fSlow1 * fRec1[1]);
			fRec1[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fSlow7 * (fRec1[2] + fRec1[0] + 2.0f * fRec1[1]) - fSlow5 * (fSlow3 * fRec0[2] + fSlow1 * fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow12 * (fRec0[2] + fRec0[0] + 2.0f * fRec0[1]));
			fVec0[1] = fVec0[0];
			fRec3[1] = fRec3[0];
			fVec1[1] = fVec1[0];
			fRec2[1] = fRec2[0];
			fRec1[2] = fRec1[1];
			fRec1[1] = fRec1[0];
			fRec0[2] = fRec0[1];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
