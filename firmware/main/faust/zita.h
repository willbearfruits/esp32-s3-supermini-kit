/* ------------------------------------------------------------
name: "zita"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_zita -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __kfx_zita_H__
#define  __kfx_zita_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS kfx_zita
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

static float kfx_zita_faustpower2_f(float value) {
	return value * value;
}

class kfx_zita : public dsp {
	
 private:
	
	int IOTA0;
	float fVec0[16384];
	FAUSTFLOAT fHslider0;
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider2;
	float fConst3;
	float fRec10[2];
	float fVec1[16384];
	float fConst4;
	int iConst5;
	FAUSTFLOAT fHslider3;
	float fConst6;
	float fVec2[2048];
	int iConst7;
	float fRec8[2];
	float fConst8;
	float fConst9;
	float fRec13[2];
	float fVec3[16384];
	float fConst10;
	int iConst11;
	float fVec4[2048];
	int iConst12;
	float fRec11[2];
	float fConst13;
	float fConst14;
	float fRec16[2];
	float fVec5[8192];
	float fConst15;
	int iConst16;
	float fVec6[2048];
	int iConst17;
	float fRec14[2];
	float fConst18;
	float fConst19;
	float fRec19[2];
	float fVec7[8192];
	float fConst20;
	int iConst21;
	float fVec8[1024];
	int iConst22;
	float fRec17[2];
	float fConst23;
	float fConst24;
	float fRec22[2];
	float fVec9[16384];
	float fConst25;
	int iConst26;
	float fVec10[2048];
	int iConst27;
	float fRec20[2];
	float fConst28;
	float fConst29;
	float fRec25[2];
	float fVec11[16384];
	float fConst30;
	int iConst31;
	float fVec12[2048];
	int iConst32;
	float fRec23[2];
	float fConst33;
	float fConst34;
	float fRec28[2];
	float fVec13[16384];
	float fConst35;
	int iConst36;
	float fVec14[2048];
	int iConst37;
	float fRec26[2];
	float fConst38;
	float fConst39;
	float fRec31[2];
	float fVec15[16384];
	float fConst40;
	int iConst41;
	float fVec16[2048];
	int iConst42;
	float fRec29[2];
	float fRec0[2];
	float fRec1[2];
	float fRec2[2];
	float fRec3[2];
	float fRec4[2];
	float fRec5[2];
	float fRec6[2];
	float fRec7[2];
	
 public:
	kfx_zita() {
	}
	
	kfx_zita(const kfx_zita&) = default;
	
	virtual ~kfx_zita() = default;
	
	kfx_zita& operator=(const kfx_zita&) = default;
	
	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn kfx_zita -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.2.0");
		m->declare("filename", "zita.dsp");
		m->declare("filters.lib/allpass_comb:author", "Julius O. Smith III");
		m->declare("filters.lib/allpass_comb:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/allpass_comb:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/tf1:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "zita");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/version", "1.5.1");
		m->declare("routes.lib/hadamard:author", "Remy Muller, revised by Romain Michon");
		m->declare("routes.lib/name", "Faust Signal Routing Library");
		m->declare("routes.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
	}

	virtual int getNumInputs() {
		return 1;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = std::floor(0.174713f * fConst0 + 0.5f);
		fConst2 = 6.9077554f * (fConst1 / fConst0);
		fConst3 = 6.2831855f / fConst0;
		fConst4 = std::floor(0.022904f * fConst0 + 0.5f);
		iConst5 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst1 - fConst4)));
		fConst6 = 0.001f * fConst0;
		iConst7 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst4 + -1.0f)));
		fConst8 = std::floor(0.153129f * fConst0 + 0.5f);
		fConst9 = 6.9077554f * (fConst8 / fConst0);
		fConst10 = std::floor(0.020346f * fConst0 + 0.5f);
		iConst11 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst8 - fConst10)));
		iConst12 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst10 + -1.0f)));
		fConst13 = std::floor(0.127837f * fConst0 + 0.5f);
		fConst14 = 6.9077554f * (fConst13 / fConst0);
		fConst15 = std::floor(0.031604f * fConst0 + 0.5f);
		iConst16 = static_cast<int>(std::min<float>(4096.0f, std::max<float>(0.0f, fConst13 - fConst15)));
		iConst17 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst15 + -1.0f)));
		fConst18 = std::floor(0.125f * fConst0 + 0.5f);
		fConst19 = 6.9077554f * (fConst18 / fConst0);
		fConst20 = std::floor(0.013458f * fConst0 + 0.5f);
		iConst21 = static_cast<int>(std::min<float>(4096.0f, std::max<float>(0.0f, fConst18 - fConst20)));
		iConst22 = static_cast<int>(std::min<float>(512.0f, std::max<float>(0.0f, fConst20 + -1.0f)));
		fConst23 = std::floor(0.210389f * fConst0 + 0.5f);
		fConst24 = 6.9077554f * (fConst23 / fConst0);
		fConst25 = std::floor(0.024421f * fConst0 + 0.5f);
		iConst26 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst23 - fConst25)));
		iConst27 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst25 + -1.0f)));
		fConst28 = std::floor(0.192303f * fConst0 + 0.5f);
		fConst29 = 6.9077554f * (fConst28 / fConst0);
		fConst30 = std::floor(0.029291f * fConst0 + 0.5f);
		iConst31 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst28 - fConst30)));
		iConst32 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst30 + -1.0f)));
		fConst33 = std::floor(0.256891f * fConst0 + 0.5f);
		fConst34 = 6.9077554f * (fConst33 / fConst0);
		fConst35 = std::floor(0.027333f * fConst0 + 0.5f);
		iConst36 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst33 - fConst35)));
		iConst37 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst35 + -1.0f)));
		fConst38 = std::floor(0.219991f * fConst0 + 0.5f);
		fConst39 = 6.9077554f * (fConst38 / fConst0);
		fConst40 = std::floor(0.019123f * fConst0 + 0.5f);
		iConst41 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst38 - fConst40)));
		iConst42 = static_cast<int>(std::min<float>(1024.0f, std::max<float>(0.0f, fConst40 + -1.0f)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(0.4f);
		fHslider1 = static_cast<FAUSTFLOAT>(2.5f);
		fHslider2 = static_cast<FAUSTFLOAT>(5e+03f);
		fHslider3 = static_cast<FAUSTFLOAT>(3e+01f);
	}
	
	virtual void instanceClear() {
		IOTA0 = 0;
		for (int l0 = 0; l0 < 16384; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec10[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 16384; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2048; l3 = l3 + 1) {
			fVec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec8[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec13[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 16384; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2048; l7 = l7 + 1) {
			fVec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec11[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec16[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 8192; l10 = l10 + 1) {
			fVec5[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2048; l11 = l11 + 1) {
			fVec6[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec14[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec19[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 8192; l14 = l14 + 1) {
			fVec7[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 1024; l15 = l15 + 1) {
			fVec8[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec17[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec22[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 16384; l18 = l18 + 1) {
			fVec9[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2048; l19 = l19 + 1) {
			fVec10[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec20[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec25[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 16384; l22 = l22 + 1) {
			fVec11[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2048; l23 = l23 + 1) {
			fVec12[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec23[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec28[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 16384; l26 = l26 + 1) {
			fVec13[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2048; l27 = l27 + 1) {
			fVec14[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec26[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec31[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 16384; l30 = l30 + 1) {
			fVec15[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2048; l31 = l31 + 1) {
			fVec16[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec29[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec0[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec1[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec2[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec3[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec4[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec5[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec6[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec7[l40] = 0.0f;
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
	
	virtual kfx_zita* clone() {
		return new kfx_zita(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("zita");
		ui_interface->addHorizontalSlider("damp", &fHslider2, FAUSTFLOAT(5e+03f), FAUSTFLOAT(1e+03f), FAUSTFLOAT(1.2e+04f), FAUSTFLOAT(1e+02f));
		ui_interface->addHorizontalSlider("predelay", &fHslider3, FAUSTFLOAT(3e+01f), FAUSTFLOAT(5.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("t60", &fHslider1, FAUSTFLOAT(2.5f), FAUSTFLOAT(0.3f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("wet", &fHslider0, FAUSTFLOAT(0.4f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = 1.0f - fSlow0;
		float fSlow2 = static_cast<float>(fHslider1);
		float fSlow3 = std::exp(-(fConst2 / fSlow2));
		float fSlow4 = kfx_zita_faustpower2_f(fSlow3);
		float fSlow5 = 1.0f - fSlow4;
		float fSlow6 = std::cos(fConst3 * static_cast<float>(fHslider2));
		float fSlow7 = 1.0f - fSlow6 * fSlow4;
		float fSlow8 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow7) / kfx_zita_faustpower2_f(fSlow5) + -1.0f));
		float fSlow9 = fSlow7 / fSlow5;
		float fSlow10 = fSlow9 - fSlow8;
		float fSlow11 = fSlow3 * (fSlow8 + (1.0f - fSlow9));
		int iSlow12 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst6 * static_cast<float>(fHslider3))));
		float fSlow13 = std::exp(-(fConst9 / fSlow2));
		float fSlow14 = kfx_zita_faustpower2_f(fSlow13);
		float fSlow15 = 1.0f - fSlow14;
		float fSlow16 = 1.0f - fSlow14 * fSlow6;
		float fSlow17 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow16) / kfx_zita_faustpower2_f(fSlow15) + -1.0f));
		float fSlow18 = fSlow16 / fSlow15;
		float fSlow19 = fSlow18 - fSlow17;
		float fSlow20 = fSlow13 * (fSlow17 + (1.0f - fSlow18));
		float fSlow21 = std::exp(-(fConst14 / fSlow2));
		float fSlow22 = kfx_zita_faustpower2_f(fSlow21);
		float fSlow23 = 1.0f - fSlow22;
		float fSlow24 = 1.0f - fSlow6 * fSlow22;
		float fSlow25 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow24) / kfx_zita_faustpower2_f(fSlow23) + -1.0f));
		float fSlow26 = fSlow24 / fSlow23;
		float fSlow27 = fSlow26 - fSlow25;
		float fSlow28 = fSlow21 * (fSlow25 + (1.0f - fSlow26));
		float fSlow29 = std::exp(-(fConst19 / fSlow2));
		float fSlow30 = kfx_zita_faustpower2_f(fSlow29);
		float fSlow31 = 1.0f - fSlow30;
		float fSlow32 = 1.0f - fSlow6 * fSlow30;
		float fSlow33 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow32) / kfx_zita_faustpower2_f(fSlow31) + -1.0f));
		float fSlow34 = fSlow32 / fSlow31;
		float fSlow35 = fSlow34 - fSlow33;
		float fSlow36 = fSlow29 * (fSlow33 + (1.0f - fSlow34));
		float fSlow37 = std::exp(-(fConst24 / fSlow2));
		float fSlow38 = kfx_zita_faustpower2_f(fSlow37);
		float fSlow39 = 1.0f - fSlow38;
		float fSlow40 = 1.0f - fSlow6 * fSlow38;
		float fSlow41 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow40) / kfx_zita_faustpower2_f(fSlow39) + -1.0f));
		float fSlow42 = fSlow40 / fSlow39;
		float fSlow43 = fSlow42 - fSlow41;
		float fSlow44 = fSlow37 * (fSlow41 + (1.0f - fSlow42));
		float fSlow45 = std::exp(-(fConst29 / fSlow2));
		float fSlow46 = kfx_zita_faustpower2_f(fSlow45);
		float fSlow47 = 1.0f - fSlow46;
		float fSlow48 = 1.0f - fSlow6 * fSlow46;
		float fSlow49 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow48) / kfx_zita_faustpower2_f(fSlow47) + -1.0f));
		float fSlow50 = fSlow48 / fSlow47;
		float fSlow51 = fSlow50 - fSlow49;
		float fSlow52 = fSlow45 * (fSlow49 + (1.0f - fSlow50));
		float fSlow53 = std::exp(-(fConst34 / fSlow2));
		float fSlow54 = kfx_zita_faustpower2_f(fSlow53);
		float fSlow55 = 1.0f - fSlow54;
		float fSlow56 = 1.0f - fSlow6 * fSlow54;
		float fSlow57 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow56) / kfx_zita_faustpower2_f(fSlow55) + -1.0f));
		float fSlow58 = fSlow56 / fSlow55;
		float fSlow59 = fSlow58 - fSlow57;
		float fSlow60 = fSlow53 * (fSlow57 + (1.0f - fSlow58));
		float fSlow61 = std::exp(-(fConst39 / fSlow2));
		float fSlow62 = kfx_zita_faustpower2_f(fSlow61);
		float fSlow63 = 1.0f - fSlow62;
		float fSlow64 = 1.0f - fSlow6 * fSlow62;
		float fSlow65 = std::sqrt(std::max<float>(0.0f, kfx_zita_faustpower2_f(fSlow64) / kfx_zita_faustpower2_f(fSlow63) + -1.0f));
		float fSlow66 = fSlow64 / fSlow63;
		float fSlow67 = fSlow66 - fSlow65;
		float fSlow68 = fSlow61 * (fSlow65 + (1.0f - fSlow66));
		float fSlow69 = 0.37f * fSlow0;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = static_cast<float>(input0[i0]);
			fVec0[IOTA0 & 16383] = fTemp0;
			float fTemp1 = fSlow1 * fTemp0;
			float fTemp2 = fSlow11 * fRec4[1] + fSlow10 * fRec10[1];
			fRec10[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			fVec1[IOTA0 & 16383] = 0.35355338f * fRec10[0] + 1e-20f;
			float fTemp3 = 0.3f * fVec0[(IOTA0 - iSlow12) & 16383];
			float fTemp4 = fTemp3 + fVec1[(IOTA0 - iConst5) & 16383] - 0.6f * fRec8[1];
			fVec2[IOTA0 & 2047] = fTemp4;
			float fTemp5 = fVec2[(IOTA0 - iConst7) & 2047];
			fRec8[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = 0.6f * fTemp4;
			float fRec9 = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = fSlow20 * fRec0[1] + fSlow19 * fRec13[1];
			fRec13[0] = ((std::fabs(fTemp7) > 1.1754944e-38f) ? fTemp7 : 0.0f);
			fVec3[IOTA0 & 16383] = 0.35355338f * fRec13[0] + 1e-20f;
			float fTemp8 = fVec3[(IOTA0 - iConst11) & 16383] + fTemp3 - 0.6f * fRec11[1];
			fVec4[IOTA0 & 2047] = fTemp8;
			float fTemp9 = fVec4[(IOTA0 - iConst12) & 2047];
			fRec11[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = 0.6f * fTemp8;
			float fRec12 = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = fRec12 + fRec9;
			float fTemp12 = fSlow28 * fRec2[1] + fSlow27 * fRec16[1];
			fRec16[0] = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			fVec5[IOTA0 & 8191] = 0.35355338f * fRec16[0] + 1e-20f;
			float fTemp13 = fVec5[(IOTA0 - iConst16) & 8191] - (fTemp3 + 0.6f * fRec14[1]);
			fVec6[IOTA0 & 2047] = fTemp13;
			float fTemp14 = fVec6[(IOTA0 - iConst17) & 2047];
			fRec14[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			float fTemp15 = 0.6f * fTemp13;
			float fRec15 = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = fSlow36 * fRec6[1] + fSlow35 * fRec19[1];
			fRec19[0] = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			fVec7[IOTA0 & 8191] = 0.35355338f * fRec19[0] + 1e-20f;
			float fTemp17 = fVec7[(IOTA0 - iConst21) & 8191] - (fTemp3 + 0.6f * fRec17[1]);
			fVec8[IOTA0 & 1023] = fTemp17;
			float fTemp18 = fVec8[(IOTA0 - iConst22) & 1023];
			fRec17[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			float fTemp19 = 0.6f * fTemp17;
			float fRec18 = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fRec18 + fRec15 + fTemp11;
			float fTemp21 = fSlow44 * fRec1[1] + fSlow43 * fRec22[1];
			fRec22[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			fVec9[IOTA0 & 16383] = 0.35355338f * fRec22[0] + 1e-20f;
			float fTemp22 = fVec9[(IOTA0 - iConst26) & 16383] + fTemp3 + 0.6f * fRec20[1];
			fVec10[IOTA0 & 2047] = fTemp22;
			float fTemp23 = fVec10[(IOTA0 - iConst27) & 2047];
			fRec20[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = 0.6f * fTemp22;
			float fRec21 = ((std::fabs(-fTemp24) > 1.1754944e-38f) ? -fTemp24 : 0.0f);
			float fTemp25 = fSlow52 * fRec5[1] + fSlow51 * fRec25[1];
			fRec25[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			fVec11[IOTA0 & 16383] = 0.35355338f * fRec25[0] + 1e-20f;
			float fTemp26 = fVec11[(IOTA0 - iConst31) & 16383] + fTemp3 + 0.6f * fRec23[1];
			fVec12[IOTA0 & 2047] = fTemp26;
			float fTemp27 = fVec12[(IOTA0 - iConst32) & 2047];
			fRec23[0] = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			float fTemp28 = 0.6f * fTemp26;
			float fRec24 = ((std::fabs(-fTemp28) > 1.1754944e-38f) ? -fTemp28 : 0.0f);
			float fTemp29 = fSlow60 * fRec3[1] + fSlow59 * fRec28[1];
			fRec28[0] = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			fVec13[IOTA0 & 16383] = 0.35355338f * fRec28[0] + 1e-20f;
			float fTemp30 = 0.6f * fRec26[1] + fVec13[(IOTA0 - iConst36) & 16383];
			fVec14[IOTA0 & 2047] = fTemp30 - fTemp3;
			float fTemp31 = fVec14[(IOTA0 - iConst37) & 2047];
			fRec26[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = 0.6f * (fTemp3 - fTemp30);
			float fRec27 = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fSlow68 * fRec7[1] + fSlow67 * fRec31[1];
			fRec31[0] = ((std::fabs(fTemp33) > 1.1754944e-38f) ? fTemp33 : 0.0f);
			fVec15[IOTA0 & 16383] = 0.35355338f * fRec31[0] + 1e-20f;
			float fTemp34 = 0.6f * fRec29[1] + fVec15[(IOTA0 - iConst41) & 16383];
			fVec16[IOTA0 & 2047] = fTemp34 - fTemp3;
			float fTemp35 = fVec16[(IOTA0 - iConst42) & 2047];
			fRec29[0] = ((std::fabs(fTemp35) > 1.1754944e-38f) ? fTemp35 : 0.0f);
			float fTemp36 = 0.6f * (fTemp3 - fTemp34);
			float fRec30 = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = fRec29[1] + fRec26[1] + fRec23[1] + fRec20[1] + fRec17[1] + fRec14[1] + fRec8[1] + fRec11[1] + fRec30 + fRec27 + fRec24 + fRec21 + fTemp20;
			fRec0[0] = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec17[1] + fRec14[1] + fRec8[1] + fRec11[1] + fTemp20 - (fRec29[1] + fRec26[1] + fRec23[1] + fRec20[1] + fRec30 + fRec27 + fRec21 + fRec24);
			fRec1[0] = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = fRec15 + fRec18;
			float fTemp40 = fRec23[1] + fRec20[1] + fRec8[1] + fRec11[1] + fRec24 + fRec21 + fTemp11 - (fRec29[1] + fRec26[1] + fRec17[1] + fRec14[1] + fRec30 + fRec27 + fTemp39);
			fRec2[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fRec29[1] + fRec26[1] + fRec8[1] + fRec11[1] + fRec30 + fRec27 + fTemp11 - (fRec23[1] + fRec20[1] + fRec17[1] + fRec14[1] + fRec24 + fRec21 + fTemp39);
			fRec3[0] = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			float fTemp42 = fRec9 + fRec18;
			float fTemp43 = fRec12 + fRec15;
			float fTemp44 = fRec26[1] + fRec20[1] + fRec14[1] + fRec11[1] + fRec27 + fRec21 + fTemp43 - (fRec29[1] + fRec23[1] + fRec17[1] + fRec8[1] + fRec30 + fRec24 + fTemp42);
			fRec4[0] = ((std::fabs(fTemp44) > 1.1754944e-38f) ? fTemp44 : 0.0f);
			float fTemp45 = fRec29[1] + fRec23[1] + fRec14[1] + fRec11[1] + fRec30 + fRec24 + fTemp43 - (fRec26[1] + fRec20[1] + fRec17[1] + fRec8[1] + fRec27 + fRec21 + fTemp42);
			fRec5[0] = ((std::fabs(fTemp45) > 1.1754944e-38f) ? fTemp45 : 0.0f);
			float fTemp46 = fRec9 + fRec15;
			float fTemp47 = fRec12 + fRec18;
			float fTemp48 = fRec29[1] + fRec20[1] + fRec17[1] + fRec11[1] + fRec30 + fRec21 + fTemp47 - (fRec26[1] + fRec23[1] + fRec14[1] + fRec8[1] + fRec27 + fRec24 + fTemp46);
			fRec6[0] = ((std::fabs(fTemp48) > 1.1754944e-38f) ? fTemp48 : 0.0f);
			float fTemp49 = fRec26[1] + fRec23[1] + fRec17[1] + fRec11[1] + fRec27 + fRec24 + fTemp47 - (fRec29[1] + fRec20[1] + fRec14[1] + fRec8[1] + fRec30 + fRec21 + fTemp46);
			fRec7[0] = ((std::fabs(fTemp49) > 1.1754944e-38f) ? fTemp49 : 0.0f);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow69 * (fRec1[0] + fRec2[0]) + fTemp1);
			output1[i0] = static_cast<FAUSTFLOAT>(fTemp1 + fSlow69 * (fRec1[0] - fRec2[0]));
			IOTA0 = IOTA0 + 1;
			fRec10[1] = fRec10[0];
			fRec8[1] = fRec8[0];
			fRec13[1] = fRec13[0];
			fRec11[1] = fRec11[0];
			fRec16[1] = fRec16[0];
			fRec14[1] = fRec14[0];
			fRec19[1] = fRec19[0];
			fRec17[1] = fRec17[0];
			fRec22[1] = fRec22[0];
			fRec20[1] = fRec20[0];
			fRec25[1] = fRec25[0];
			fRec23[1] = fRec23[0];
			fRec28[1] = fRec28[0];
			fRec26[1] = fRec26[0];
			fRec31[1] = fRec31[0];
			fRec29[1] = fRec29[0];
			fRec0[1] = fRec0[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec7[1] = fRec7[0];
		}
	}

};

#endif
