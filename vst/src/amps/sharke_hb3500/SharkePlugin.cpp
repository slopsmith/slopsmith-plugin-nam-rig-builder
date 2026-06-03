/*
 * Sharke HB3500 — Hartke HA3500 bass head model.
 *
 * From the HA3500 circuit diagram (Samson/Hartke, main board 400518280):
 *   1. Input  : Passive (-20 dB) or Active (-34 dB) jack — Active pads hot basses.
 *   2. Tube   : a 12AX7 preamp path (warm, soft-clipping) at the Tube level.
 *   3. Solid State : a clean/punchy op-amp preamp path at its level. The two
 *               paths SUM — the HA3500's signature blendable dual preamp.
 *   4. Compression : the built-in compressor, amount = Compression.
 *   5. 10-band graphic EQ : 30/64/125/250/500/1k/2k/4k/8k/16k Hz, +/-12 dB,
 *               engaged by EQ In.
 *   6. High Pass / Low Pass : variable HPF (20..200 Hz) and LPF (2k..20 kHz).
 *   7. Volume : master output.
 */
#include "DistrhoPlugin.hpp"
#include "HartkeParams.h"
#include <cmath>

START_NAMESPACE_DISTRHO

class Biquad {
    float b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
public:
    void reset() { z1 = z2 = 0.f; }
    inline float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setPeak(float fc, float dB, float Q, float fs) {
        if (fc > fs * 0.49f) fc = fs * 0.49f;
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = (-2 * cw) / a0; b2 = (1 - alpha * A) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha / A) / a0;
    }
    void setLowPass(float fc, float Q, float fs) {
        if (fc > fs * 0.49f) fc = fs * 0.49f;
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 = ((1 - cw) * 0.5f) / a0; b1 = (1 - cw) / a0; b2 = ((1 - cw) * 0.5f) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
    }
    void setHighPass(float fc, float Q, float fs) {
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 = ((1 + cw) * 0.5f) / a0; b1 = -(1 + cw) / a0; b2 = ((1 + cw) * 0.5f) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
    }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; z1 = z2 = 0; }
};

class HartkeChannel {
    float fs = 48000.f;
    Biquad eq[kNumEq];
    Biquad lpf, hpf;
    float tubeGain=1, ssGain=1, master=1;
    bool  eqIn=true, lpfOn=false, hpfOn=false;
    // compressor
    bool compOn=false; float compThr=1, compRatio=1, compMk=1, env=0, atk=0, rel=0;
    static inline float msC(float ms, float fs){ return std::exp(-1.f/(0.001f*ms*fs)); }

    static inline float tube(float x){ return std::tanh(1.6f*x) * 0.625f; }      // warm, soft
    static inline float solid(float x){ const float a=std::fabs(x);             // punchy, harder knee
        return (a<0.7f) ? x : (x>0?1.f:-1.f)*(0.7f+(a-0.7f)/(1.f+(a-0.7f)*3.f)); }
public:
    void setSampleRate(float s){ fs=(s>0)?s:48000.f; atk=msC(6.f,fs); rel=msC(120.f,fs); }
    void reset(){ for(int i=0;i<kNumEq;++i) eq[i].reset(); lpf.reset(); hpf.reset(); env=0; }

    void setParams(const float* p) {
        const float padActive = (p[kActive] > 0.5f) ? 0.20f : 1.0f;   // Active jack ~ -14 dB
        tubeGain = p[kTube]  * 2.2f * padActive;
        ssGain   = p[kSolid] * 2.2f * padActive;

        compOn = p[kComp] > 0.001f;
        compThr = 1.0f - p[kComp]*0.6f; compRatio = 1.0f + p[kComp]*5.0f; compMk = 1.0f + p[kComp]*0.7f;

        eqIn = p[kEqIn] > 0.5f;
        for (int i=0;i<kNumEq;++i) {
            if (eqIn) eq[i].setPeak(kEqFreqs[i], (p[kFirstEq+i]-0.5f)*24.f, 1.4f, fs);
            else      eq[i].setBypass();
        }
        // High Pass: 20 .. 200 Hz (0 = open/off). Low Pass: 2k .. 20k (1 = open/off).
        hpfOn = p[kHighPass] > 0.02f;
        if (hpfOn) hpf.setHighPass(20.f * std::pow(10.f, p[kHighPass]), 0.707f, fs); else hpf.setBypass();
        lpfOn = p[kLowPass] < 0.98f;
        if (lpfOn) lpf.setLowPass(2000.f * std::pow(10.f, p[kLowPass]), 0.707f, fs); else lpf.setBypass();

        master = p[kVolume] / 0.7f;
    }

    inline float process(float x) {
        // dual preamp blend
        float s = tube(tubeGain * x) + solid(ssGain * x);
        s *= 0.5f;
        // compressor (downward, peak-following)
        if (compOn) {
            const float a = std::fabs(s);
            const float c = (a > env) ? atk : rel;
            env = c*env + (1.f-c)*a;
            if (env > compThr) s *= (compThr + (env-compThr)/compRatio) / (env + 1e-9f);
            s *= compMk;
        }
        // graphic EQ
        for (int i=0;i<kNumEq;++i) s = eq[i].process(s);
        // tone filters
        s = hpf.process(s);
        s = lpf.process(s);
        return s * master;
    }
};

class SharkePlugin : public Plugin {
    HartkeChannel L, R;
    float fParams[kParamCount];
    void recalc(){ L.setParams(fParams); R.setParams(fParams); }
public:
    SharkePlugin() : Plugin(kParamCount, 0, 0) {
        for (int i=0;i<kParamCount;++i) fParams[i]=kHartkeDef[i];
        const float sr=(float)getSampleRate();
        L.setSampleRate(sr); R.setSampleRate(sr); L.reset(); R.reset(); recalc();
    }
protected:
    const char* getLabel()       const override { return "SharkeHB3500"; }
    const char* getDescription() const override { return "Hartke HA3500 bass head model"; }
    const char* getMaker()       const override { return "RigBuilder"; }
    const char* getLicense()     const override { return "ISC"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('R', 'B', 'H', 'k'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if (i >= (uint32_t)kParamCount) return;
        p.hints = kParameterIsAutomatable;
        if (i >= (uint32_t)kActive) p.hints |= kParameterIsBoolean;
        p.name = kHartkeNames[i]; p.symbol = kHartkeSymbols[i];
        p.ranges.min = kHartkeMin[i]; p.ranges.max = kHartkeMax[i]; p.ranges.def = kHartkeDef[i];
    }
    float getParameterValue(uint32_t i) const override { return (i < (uint32_t)kParamCount) ? fParams[i] : 0.f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < (uint32_t)kParamCount) { fParams[i]=v; recalc(); } }
    void  sampleRateChanged(double r) override { L.setSampleRate((float)r); R.setSampleRate((float)r); L.reset(); R.reset(); recalc(); }

    void run(const float** in, float** out, uint32_t frames) override {
        const float* iL=in[0]; const float* iR=in[1]; float* oL=out[0]; float* oR=out[1];
        for (uint32_t i=0;i<frames;++i){ oL[i]=L.process(iL[i]); oR[i]=R.process(iR[i]); }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharkePlugin)
};

Plugin* createPlugin() { return new SharkePlugin(); }

END_NAMESPACE_DISTRHO
