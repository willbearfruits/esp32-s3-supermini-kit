/* ------------------------------------------------------------
name: "dyn"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_dyn -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_dyn_H__
#define  __kfx_dyn_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_dyn
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


class kfx_dyn : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	int fSampleRate;
	float fConst0;
	float fConst1;
	FAUSTFLOAT fHslider1;
	FAUSTFLOAT fHslider2;
	float fRec3[2];
	FAUSTFLOAT fHslider3;
	float fRec2[2];
	FAUSTFLOAT fHslider4;
	float fConst2;
	float fConst3;
	float fRec1[2];
	float fConst4;
	float fConst5;
	float fRec0[2];
	
 public:
	kfx_dyn() {
	}
	
	kfx_dyn(const kfx_dyn&) = default;
	
	virtual ~kfx_dyn() = default;
	
	kfx_dyn& operator=(const kfx_dyn&) = default;
	
	void metadata(Meta* m) { 
		m->declare("analyzers.lib/amp_follower_ar:author", "Jonatan Liljedahl, revised by Romain Michon");
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.3.0");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_dyn -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("compressors.lib/compression_gain_mono:author", "Julius O. Smith III");
		m->declare("compressors.lib/compression_gain_mono:copyright", "Copyright (C) 2014-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("compressors.lib/compression_gain_mono:license", "MIT-style STK-4.3 license");
		m->declare("compressors.lib/compressor_lad_mono:author", "Julius O. Smith III");
		m->declare("compressors.lib/compressor_lad_mono:copyright", "Copyright (C) 2014-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("compressors.lib/compressor_lad_mono:license", "MIT-style STK-4.3 license");
		m->declare("compressors.lib/compressor_mono:author", "Julius O. Smith III");
		m->declare("compressors.lib/compressor_mono:copyright", "Copyright (C) 2014-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("compressors.lib/compressor_mono:license", "MIT-style STK-4.3 license");
		m->declare("compressors.lib/limiter_1176_R4_mono:author", "Julius O. Smith III");
		m->declare("compressors.lib/limiter_1176_R4_mono:copyright", "Copyright (C) 2014-2020 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("compressors.lib/limiter_1176_R4_mono:license", "MIT-style STK-4.3 license");
		m->declare("compressors.lib/name", "Faust Compressor Effect Library");
		m->declare("compressors.lib/version", "1.6.0");
		m->declare("filename", "dyn.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "dyn");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/onePoleSwitching:author", "Jonatan Liljedahl, revised by Dario Sanfilippo");
		m->declare("signals.lib/onePoleSwitching:licence", "STK-4.3");
		m->declare("signals.lib/version", "1.6.0");
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
		fConst1 = 1.0f / fConst0;
		fConst2 = std::exp(-(2.0f / fConst0));
		fConst3 = std::exp(-(1.25e+03f / fConst0));
		fConst4 = std::exp(-(2.5e+03f / fConst0));
		fConst5 = 0.75f * (1.0f - fConst4);
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.005f);
		fHslider1 = static_cast<FAUSTFLOAT>(-3e+01f);
		fHslider2 = static_cast<FAUSTFLOAT>(0.15f);
		fHslider3 = static_cast<FAUSTFLOAT>(4.0f);
		fHslider4 = static_cast<FAUSTFLOAT>(12.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fRec3[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec2[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec0[l3] = 0.0f;
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
	
	virtual kfx_dyn* clone() {
		return new kfx_dyn(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("dyn");
		ui_interface->addHorizontalSlider("att", &fHslider0, FAUSTFLOAT(0.005f), FAUSTFLOAT(0.001f), FAUSTFLOAT(0.1f), FAUSTFLOAT(0.001f));
		ui_interface->addHorizontalSlider("gain", &fHslider4, FAUSTFLOAT(12.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(4e+01f), FAUSTFLOAT(0.5f));
		ui_interface->addHorizontalSlider("ratio", &fHslider3, FAUSTFLOAT(4.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("rel", &fHslider2, FAUSTFLOAT(0.15f), FAUSTFLOAT(0.01f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("thresh", &fHslider1, FAUSTFLOAT(-3e+01f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = 0.5f * fSlow0;
		int iSlow2 = std::fabs(fSlow1) < 1.1920929e-07f;
		float fSlow3 = ((iSlow2) ? 0.0f : std::exp(-(fConst1 / ((iSlow2) ? 1.0f : fSlow1))));
		float fSlow4 = 1.0f - fSlow3;
		float fSlow5 = static_cast<float>(fHslider1);
		float fSlow6 = static_cast<float>(fHslider2);
		int iSlow7 = std::fabs(fSlow6) < 1.1920929e-07f;
		float fSlow8 = ((iSlow7) ? 0.0f : std::exp(-(fConst1 / ((iSlow7) ? 1.0f : fSlow6))));
		int iSlow9 = std::fabs(fSlow0) < 1.1920929e-07f;
		float fSlow10 = ((iSlow9) ? 0.0f : std::exp(-(fConst1 / ((iSlow9) ? 1.0f : fSlow0))));
		float fSlow11 = 1.0f / std::max<float>(1.1920929e-07f, static_cast<float>(fHslider3)) + -1.0f;
		float fSlow12 = std::pow(1e+01f, 0.05f * static_cast<float>(fHslider4));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = static_cast<float>(input0[i0]);
			float fTemp1 = std::fabs(fTemp0);
			float fTemp2 = ((fTemp1 > fRec3[1]) ? fSlow10 : fSlow8);
			float fTemp3 = fTemp1 * (1.0f - fTemp2) + fRec3[1] * fTemp2;
			fRec3[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = fSlow11 * std::max<float>(2e+01f * std::log10(std::max<float>(1.1754944e-38f, fRec3[0])) - fSlow5, 0.0f) * fSlow4 + fSlow3 * fRec2[1];
			fRec2[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fTemp0 * std::pow(1e+01f, 0.05f * fRec2[0]);
			float fTemp6 = std::fabs(fSlow12 * fTemp5);
			float fTemp7 = ((fTemp6 > fRec1[1]) ? fConst3 : fConst2);
			float fTemp8 = fTemp6 * (1.0f - fTemp7) + fRec1[1] * fTemp7;
			fRec1[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = fConst4 * fRec0[1] - fConst5 * std::max<float>(2e+01f * std::log10(std::max<float>(1.1754944e-38f, fRec1[0])) + 6.0f, 0.0f);
			fRec0[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow12 * fTemp5 * std::pow(1e+01f, 0.05f * fRec0[0]));
			fRec3[1] = fRec3[0];
			fRec2[1] = fRec2[0];
			fRec1[1] = fRec1[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
