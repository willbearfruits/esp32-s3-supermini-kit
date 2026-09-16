/* ------------------------------------------------------------
name: "shift"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_shift -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_shift_H__
#define  __kfx_shift_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_shift
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


class kfx_shift : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fRec0[2];
	int IOTA0;
	float fVec0[131072];
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	
 public:
	kfx_shift() {
	}
	
	kfx_shift(const kfx_shift&) = default;
	
	virtual ~kfx_shift() = default;
	
	kfx_shift& operator=(const kfx_shift&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_shift -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.2.0");
		m->declare("filename", "shift.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.5.2");
		m->declare("name", "shift");
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
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(12.0f);
		fHslider1 = static_cast<FAUSTFLOAT>(1.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fRec0[l0] = 0.0f;
		}
		IOTA0 = 0;
		for (int l1 = 0; l1 < 131072; l1 = l1 + 1) {
			fVec0[l1] = 0.0f;
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
	
	virtual kfx_shift* clone() {
		return new kfx_shift(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("shift");
		ui_interface->addHorizontalSlider("mix", &fHslider1, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("semi", &fHslider0, FAUSTFLOAT(12.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(24.0f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(2.0f, 0.083333336f * static_cast<float>(fHslider0));
		float fSlow1 = static_cast<float>(fHslider1);
		float fSlow2 = 1.0f - fSlow1;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = std::fmod(fRec0[1] + 1025.0f - fSlow0, 1024.0f);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::min<float>(0.00390625f * fRec0[0], 1.0f);
			float fTemp2 = fRec0[0] + 1024.0f;
			float fTemp3 = std::floor(fTemp2);
			float fTemp4 = static_cast<float>(input0[i0]);
			fVec0[IOTA0 & 131071] = fTemp4;
			int iTemp5 = static_cast<int>(fTemp2);
			int iTemp6 = static_cast<int>(fRec0[0]);
			float fTemp7 = std::floor(fRec0[0]);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow2 * fTemp4 + fSlow1 * ((fVec0[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp6))) & 131071] * (fTemp7 + (1.0f - fRec0[0])) + (fRec0[0] - fTemp7) * fVec0[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp6 + 1))) & 131071]) * fTemp1 + (fVec0[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp5))) & 131071] * (fTemp3 + (-1023.0f - fRec0[0])) + fVec0[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp5 + 1))) & 131071] * (fRec0[0] + (1024.0f - fTemp3))) * (1.0f - fTemp1)));
			fRec0[1] = fRec0[0];
			IOTA0 = IOTA0 + 1;
		}
	}

};

#endif
