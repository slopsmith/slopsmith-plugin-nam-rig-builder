/*
 * Bender Fumble 800 — Fender Rumble 800 (modern Class-D bass head), COMPONENT-
 * LEVEL-style model.
 *
 * The Rumble 800 is an all solid-state / Class-D head (no tubes), built from the
 * documented front panel + the Rumble preamp topology:
 *   • GAIN     — solid-state input gain
 *   • VOICING  — Bright (top lift) / Contour (mid-scoop "smile") / Vintage
 *                (warm soft-saturated, slightly compressed, top rolled off)
 *   • OVERDRIVE — Drive pushes an asymmetric soft-clip stage; Level sets its out
 *   • 4-BAND EQ — Bass (shelf) / Low Mid (peak) / High Mid (peak) / Treble (shelf)
 *   • MASTER   — into the Class-D power amp (~800 W, effectively unclippable;
 *                the only nonlinearity is the Overdrive + Vintage voicing)
 *
 * Voicing/EQ corners are white-boxed bass-amp values; the Overdrive is a
 * drive-into-asymmetric-clipper with a Level make-up, transparent at Drive 0.
 */
#include "DistrhoPlugin.hpp"
#include "RumbleParams.h"
#include <cmath>

START_NAMESPACE_DISTRHO

static inline float rbAmpLvl(float x){ const float t=0.90f,c=0.99f,a=(x<0.f?-x:x);
    if(a<=t) return x; return (x<0.f?-1.f:1.f)*(t+(c-t)*std::tanh((a-t)/(c-t))); }
static inline float softClip(float x) { return std::tanh(x); }
// asymmetric soft clip — the overdrive grinds harder on one polarity (bass grit)
static inline float asymClip(float x){ return (x<0.f) ? std::tanh(x*0.85f) : std::tanh(x*1.20f)*0.9f; }

// ── RBJ biquad — voicing + 4-band EQ ─────────────────────────────────────────
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
    void setLowShelf(float fc, float dB, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw * 0.5f * 1.4142135f;
        const float sA = std::sqrt(A), tsAa = 2.f * sA * alpha;
        const float a0 =       (A + 1) + (A - 1) * cw + tsAa;
        b0 =  A * ((A + 1) - (A - 1) * cw + tsAa) / a0;
        b1 = 2*A * ((A - 1) - (A + 1) * cw)        / a0;
        b2 =  A * ((A + 1) - (A - 1) * cw - tsAa)  / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cw)         / a0;
        a2 =      ((A + 1) + (A - 1) * cw - tsAa)  / a0;
    }
    void setHighShelf(float fc, float dB, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw * 0.5f * 1.4142135f;
        const float sA = std::sqrt(A), tsAa = 2.f * sA * alpha;
        const float a0 =       (A + 1) - (A - 1) * cw + tsAa;
        b0 =  A * ((A + 1) + (A - 1) * cw + tsAa) / a0;
        b1 = -2*A * ((A - 1) + (A + 1) * cw)      / a0;
        b2 =  A * ((A + 1) + (A - 1) * cw - tsAa) / a0;
        a1 =  2 * ((A - 1) - (A + 1) * cw)        / a0;
        a2 =      ((A + 1) - (A - 1) * cw - tsAa) / a0;
    }
    void setPeak(float fc, float dB, float Q, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = (-2 * cw) / a0; b2 = (1 - alpha * A) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha / A) / a0;
    }
    void setLowpassQ(float fc, float Q, float fs) {
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 =  (1 - cw) * 0.5f / a0; b1 = (1 - cw) / a0; b2 = (1 - cw) * 0.5f / a0;
        a1 =  -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

class RumbleChannel {
    float fs = 48000.f;
    Biquad voBright, voConLo, voConMid, voConHi;     // Bright + Contour voicing
    Biquad vintLP, vintMid;                          // Vintage voicing
    Biquad bqBass, bqLowMid, bqHighMid, bqTreble;    // 4-band EQ
    float gain=1, driveG=1, levelG=1, master=1;
    bool bright=false, contour=false, vintage=false;
public:
    void setSampleRate(float s){ fs=(s>0.f)?s:48000.f; }
    void reset(){ voBright.reset(); voConLo.reset(); voConMid.reset(); voConHi.reset();
        vintLP.reset(); vintMid.reset(); bqBass.reset(); bqLowMid.reset(); bqHighMid.reset(); bqTreble.reset(); }

    void setParams(const float* p) {
        gain   = 0.4f + p[kGain] * 1.8f;                 // solid-state input gain
        bright = p[kBright]  > 0.5f;
        contour= p[kContour] > 0.5f;
        vintage= p[kVintage] > 0.5f;

        voBright.setHighShelf(3000.f, 6.0f, fs);
        voConLo.setLowShelf(80.f, 3.0f, fs);             // Contour = mid scoop + low/high lift
        voConMid.setPeak(500.f, -8.0f, 0.7f, fs);
        voConHi.setHighShelf(4000.f, 3.0f, fs);
        vintLP.setLowpassQ(6000.f, 0.7f, fs);            // Vintage = warm top rolloff + low-mid bump
        vintMid.setPeak(220.f, 2.5f, 0.8f, fs);

        // Overdrive: Drive into the asym clipper (transparent at 0), Level = out.
        driveG = 1.0f + p[kDrive] * p[kDrive] * 14.0f;
        levelG = 0.3f + p[kLevel] * 1.4f;                // 0.5 -> ~unity

        // 4-band EQ (+/-15 dB shelves, +/-14 dB peaks)
        bqBass.setLowShelf (70.f,   (p[kBass]   - 0.5f) * 30.f, fs);
        bqLowMid.setPeak   (250.f,  (p[kLowMid] - 0.5f) * 28.f, 0.9f, fs);
        bqHighMid.setPeak  (1000.f, (p[kHighMid]- 0.5f) * 28.f, 0.9f, fs);
        bqTreble.setHighShelf(5000.f,(p[kTreble]- 0.5f) * 30.f, fs);

        master = p[kMaster] / 0.7f;
    }

    inline float process(float x) {
        float s = x * gain;
        // 1. Vintage voicing — warm soft saturation + top rolloff + low-mid bump
        if (vintage) { s = std::tanh(s * 1.4f) * 0.7143f; s = vintLP.process(s); s = vintMid.process(s); }
        // 2. Bright
        if (bright) s = voBright.process(s);
        // 3. Contour (mid-scoop "smile")
        if (contour) { s = voConLo.process(s); s = voConMid.process(s); s = voConHi.process(s); }
        // 4. Overdrive: Drive -> asym clip -> Level
        s = asymClip(s * driveG) * levelG;
        // 5. 4-band EQ
        s = bqBass.process(s); s = bqLowMid.process(s); s = bqHighMid.process(s); s = bqTreble.process(s);
        // 6. Master (Class-D power: clean, the shared output stage is the only ceiling)
        return s * master;
    }
};

// kMakeup lifts the preamp into the shared output stage; kLvl matches the amp to
// the common multitone loudness (~-15 dBFS @ noon).
static constexpr float kRumbleMakeup = 8.60f;   // tuned offline
static constexpr float kRumbleLvl    = 0.3113f;

class RumblePlugin : public Plugin {
    RumbleChannel L, R;
    float fParams[kParamCount];
    void recalc() { L.setParams(fParams); R.setParams(fParams); }
public:
    RumblePlugin() : Plugin(kParamCount, 0, 0) {
        for (int i = 0; i < kParamCount; ++i) fParams[i] = kRumbleDef[i];
        const float sr = (float)getSampleRate();
        L.setSampleRate(sr); R.setSampleRate(sr); L.reset(); R.reset(); recalc();
    }
protected:
    const char* getLabel()       const override { return "BenderFumble800"; }
    const char* getDescription() const override { return "Fender Rumble 800 Class-D bass head — component-level model"; }
    const char* getMaker()       const override { return "RigBuilder"; }
    const char* getLicense()     const override { return "ISC"; }
    uint32_t    getVersion()     const override { return d_version(1, 1, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('R', 'B', 'R', '8'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if (i >= (uint32_t)kParamCount) return;
        p.hints = kParameterIsAutomatable;
        if (i >= (uint32_t)kBright) p.hints |= kParameterIsBoolean;
        p.name = kRumbleNames[i]; p.symbol = kRumbleSymbols[i];
        p.ranges.min = kRumbleMin[i]; p.ranges.max = kRumbleMax[i]; p.ranges.def = kRumbleDef[i];
    }
    float getParameterValue(uint32_t i) const override { return (i < (uint32_t)kParamCount) ? fParams[i] : 0.f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < (uint32_t)kParamCount) { fParams[i] = v; recalc(); } }
    void  sampleRateChanged(double r) override { L.setSampleRate((float)r); R.setSampleRate((float)r); L.reset(); R.reset(); recalc(); }

    void run(const float** in, float** out, uint32_t frames) override {
        const float* iL = in[0]; const float* iR = in[1]; float* oL = out[0]; float* oR = out[1];
        for (uint32_t i = 0; i < frames; ++i) { oL[i] = rbAmpLvl(kRumbleLvl * softClip(kRumbleMakeup * L.process(iL[i])) * 0.98f); oR[i] = rbAmpLvl(kRumbleLvl * softClip(kRumbleMakeup * R.process(iR[i])) * 0.98f); }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RumblePlugin)
};

Plugin* createPlugin() { return new RumblePlugin(); }

END_NAMESPACE_DISTRHO
