/* ------------------------------------------------------------
name: "flanger"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_flanger -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_flanger_H__
#define  __kfx_flanger_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_flanger
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


class kfx_flanger : public dsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	int IOTA0;
	float fVec1[4096];
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fRec1[2];
	FAUSTFLOAT fHslider2;
	FAUSTFLOAT fHslider3;
	float fConst2;
	float fRec0[2];
	FAUSTFLOAT fHslider4;
	
 public:
	kfx_flanger() {
	}
	
	kfx_flanger(const kfx_flanger&) = default;
	
	virtual ~kfx_flanger() = default;
	
	kfx_flanger& operator=(const kfx_flanger&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_flanger -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.2.0");
		m->declare("filename", "flanger.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "flanger");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.7.0");
		m->declare("phaflangers.lib/name", "Faust Phaser and Flanger Library");
		m->declare("phaflangers.lib/version", "1.1.0");
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
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = 1.0f / fConst0;
		fConst2 = 0.001f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.5f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.3f);
		fHslider2 = static_cast<FAUSTFLOAT>(6.0f);
		fHslider3 = static_cast<FAUSTFLOAT>(1.0f);
		fHslider4 = static_cast<FAUSTFLOAT>(0.5f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		IOTA0 = 0;
		for (int l1 = 0; l1 < 4096; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
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
	
	virtual kfx_flanger* clone() {
		return new kfx_flanger(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("flanger");
		ui_interface->addHorizontalSlider("base", &fHslider3, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("depth", &fHslider2, FAUSTFLOAT(6.0f), FAUSTFLOAT(0.5f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("fb", &fHslider0, FAUSTFLOAT(0.5f), FAUSTFLOAT(-0.95f), FAUSTFLOAT(0.95f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("mix", &fHslider4, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("rate", &fHslider1, FAUSTFLOAT(0.3f), FAUSTFLOAT(0.02f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = fConst1 * static_cast<float>(fHslider1);
		float fSlow2 = 0.5f * static_cast<float>(fHslider2);
		float fSlow3 = static_cast<float>(fHslider3);
		float fSlow4 = static_cast<float>(fHslider4);
		float fSlow5 = 0.5f * fSlow4;
		float fSlow6 = 1.0f - fSlow4;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = static_cast<float>(input0[i0]);
			iVec0[0] = 1;
			float fTemp1 = fSlow0 * fRec0[1] - fTemp0;
			fVec1[IOTA0 & 4095] = fTemp1;
			float fTemp2 = ((1 - iVec0[1]) ? 0.0f : fSlow1 + fRec1[1]);
			float fTemp3 = fTemp2 - std::floor(fTemp2);
			fRec1[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = std::min<float>(2047.0f, fConst2 * (fSlow3 + fSlow2 * (std::sin(6.2831855f * fRec1[0]) + 1.0f)));
			int iTemp5 = static_cast<int>(fTemp4);
			float fTemp6 = std::floor(fTemp4);
			float fTemp7 = fVec1[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp5))) & 4095] * (fTemp6 + (1.0f - fTemp4)) + (fTemp4 - fTemp6) * fVec1[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp5 + 1))) & 4095];
			fRec0[0] = ((std::fabs(fTemp7) > 1.1754944e-38f) ? fTemp7 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow6 * fTemp0 + fSlow5 * (fTemp0 + fRec0[0]));
			iVec0[1] = iVec0[0];
			IOTA0 = IOTA0 + 1;
			fRec1[1] = fRec1[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
