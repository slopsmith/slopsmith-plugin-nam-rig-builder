/*
 * CustomDrive - overdrive/distortion pedal for Rocksmith's Pedal_CustomDrive.
 * Reference: local "custom drive" schematic with op-amp gain, MOSFET/diode
 * clipping, a voice switch, and a passive tone network. Rocksmith exposes only
 * Gain, Tone, and Voice, so output level is internally normalized.
 */
#include "DistrhoPlugin.hpp"
#include "CustomDriveParams.h"
#include "../_shared/automakeup.hpp"
#include <cmath>

START_NAMESPACE_DISTRHO

namespace {

static constexpr float kPi = 3.14159265359f;

static inline float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline float clampFreq(float hz, float sr)
{
    const float nyquist = sr * 0.45f;
    if (hz < 20.0f)
        return 20.0f;
    return hz > nyquist ? nyquist : hz;
}

static inline float ocdShaper(float in)
{
    // Fulltone OCD waveshaper, transfer curve taken verbatim from the open-source
    // Schrammel OJD (github.com/janosgit/schrammel_ojd). Asymmetric soft clip: the
    // positive half stays linear up to +0.9 while the negative half begins bending
    // at -0.3, which is the OCD's even-harmonic, touch-dynamic, "vocal" asymmetry.
    // C1-continuous (slopes match at every breakpoint) so it stays smooth. The OJD
    // runs it at 16× oversampling; here the continuous slope plus the post-clip
    // low-pass keep aliasing in check without an oversampler.
    if (in <= -1.7f) return -1.0f;
    if (in <  -0.3f) { const float u = in + 0.3f; return u + (u * u) / 2.8f - 0.3f; }
    if (in <=  0.9f) return in;
    if (in <   1.1f) { const float u = in - 0.9f; return u - (u * u) / 0.4f + 0.9f; }
    return 1.0f;
}

class Biquad
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;

    void set(float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        if (std::fabs(na0) < 1.0e-12f)
            na0 = 1.0f;
        const float invA0 = 1.0f / na0;
        b0 = nb0 * invA0;
        b1 = nb1 * invA0;
        b2 = nb2 * invA0;
        a1 = na1 * invA0;
        a2 = na2 * invA0;
    }

public:
    void reset()
    {
        z1 = z2 = 0.0f;
    }

    float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setHighPass(float sr, float hz, float q)
    {
        hz = clampFreq(hz, sr);
        const float w0 = 2.0f * kPi * hz / sr;
        const float c = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * q);
        set((1.0f + c) * 0.5f, -(1.0f + c), (1.0f + c) * 0.5f,
            1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }

    void setLowPass(float sr, float hz, float q)
    {
        hz = clampFreq(hz, sr);
        const float w0 = 2.0f * kPi * hz / sr;
        const float c = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * q);
        set((1.0f - c) * 0.5f, 1.0f - c, (1.0f - c) * 0.5f,
            1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }

    void setPeaking(float sr, float hz, float q, float gainDb)
    {
        hz = clampFreq(hz, sr);
        const float a = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * hz / sr;
        const float c = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * q);
        set(1.0f + alpha * a, -2.0f * c, 1.0f - alpha * a,
            1.0f + alpha / a, -2.0f * c, 1.0f - alpha / a);
    }

    void setHighShelf(float sr, float hz, float slope, float gainDb)
    {
        hz = clampFreq(hz, sr);
        const float a = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * hz / sr;
        const float c = std::cos(w0);
        const float s = std::sin(w0);
        const float rootA = std::sqrt(a);
        const float alpha = s * 0.5f * std::sqrt((a + 1.0f / a) * (1.0f / slope - 1.0f) + 2.0f);

        set(a * ((a + 1.0f) + (a - 1.0f) * c + 2.0f * rootA * alpha),
            -2.0f * a * ((a - 1.0f) + (a + 1.0f) * c),
            a * ((a + 1.0f) + (a - 1.0f) * c - 2.0f * rootA * alpha),
            (a + 1.0f) - (a - 1.0f) * c + 2.0f * rootA * alpha,
            2.0f * ((a - 1.0f) - (a + 1.0f) * c),
            (a + 1.0f) - (a - 1.0f) * c - 2.0f * rootA * alpha);
    }
};

} // namespace

class CustomDriveCore
{
    float sampleRate = 48000.0f;
    float gain = kCustomDriveDef[kGain];
    float tone = kCustomDriveDef[kTone];
    float voice = kCustomDriveDef[kVoice];

    Biquad inputHp;
    Biquad preVoice;
    Biquad postMid;
    Biquad toneShelf;
    Biquad toneLowPass;

    void updateFilters()
    {
        const float voiceOn = voice >= 0.5f ? 1.0f : 0.0f;
        inputHp.setHighPass(sampleRate, 70.0f + 55.0f * voiceOn + 35.0f * gain, 0.70f);
        preVoice.setPeaking(sampleRate, 720.0f + 420.0f * voiceOn, 0.82f,
                            2.1f - 2.2f * voiceOn + 2.8f * gain);
        postMid.setPeaking(sampleRate, 980.0f + 520.0f * voiceOn, 0.74f,
                           -1.0f + 3.0f * voiceOn - 0.8f * tone);
        toneShelf.setHighShelf(sampleRate, 2400.0f + 1200.0f * tone, 0.72f,
                               -5.0f + 10.5f * tone + 1.4f * voiceOn);
        toneLowPass.setLowPass(sampleRate, 4200.0f + 6700.0f * tone + 1400.0f * voiceOn, 0.68f);
    }

public:
    void reset()
    {
        inputHp.reset();
        preVoice.reset();
        postMid.reset();
        toneShelf.reset();
        toneLowPass.reset();
        updateFilters();
    }

    void setSampleRate(float sr)
    {
        sampleRate = sr > 1000.0f ? sr : 48000.0f;
        reset();
    }

    void setGain(float v)
    {
        gain = clamp01(v);
        updateFilters();
    }

    void setTone(float v)
    {
        tone = clamp01(v);
        updateFilters();
    }

    void setVoice(float v)
    {
        voice = clamp01(v);
        updateFilters();
    }

    float process(float in)
    {
        const float voiceOn = voice >= 0.5f ? 1.0f : 0.0f;

        float x = inputHp.process(in);
        x = preVoice.process(x);

        // Op-amp gain into the OCD waveshaper. Drive scales the signal into the
        // shaper's clipping zones; the Voice (HP/LP) switch shifts the EQ around
        // it (see updateFilters) and adds a little asymmetry bias. Kept well below
        // the old ~15× so the signal sits in the shaper's soft/asymmetric region
        // rather than railing flat (and therefore symmetric) at every setting.
        const float g = gain * gain;
        const float drive = 1.0f + 2.2f * gain + 2.5f * g + 1.0f * voiceOn * gain;
        const float bias = voiceOn ? (-0.05f - 0.04f * gain) : (0.03f * gain);
        float y = ocdShaper(x * drive + bias);

        // Blend back a little unclipped path at low gain so Gain=0 does not
        // behave like a permanently distorted pedal.
        const float cleanBlend = 0.16f * (1.0f - gain);
        y = y * (1.0f - cleanBlend) + x * cleanBlend;

        y = postMid.process(y);
        y = toneShelf.process(y);
        y = toneLowPass.process(y);

        // No Rocksmith volume knob: compensate the pre-gain so presets do not
        // turn into level jumps.
        const float level = 0.80f / (1.0f + 0.55f * gain) * (voiceOn ? 0.94f : 0.99f);
        return y * level;
    }
};

class CustomDrivePlugin : public Plugin
{
    CustomDriveCore left;
    CustomDriveCore right;
    RBAutoMakeup makeup;
    float params[kParamCount];

    void applyAll()
    {
        left.setGain(params[kGain]);
        right.setGain(params[kGain]);
        left.setTone(params[kTone]);
        right.setTone(params[kTone]);
        left.setVoice(params[kVoice]);
        right.setVoice(params[kVoice]);
    }

public:
    CustomDrivePlugin()
        : Plugin(kParamCount, 0, 0)
    {
        for (int i = 0; i < kParamCount; ++i)
            params[i] = kCustomDriveDef[i];
        left.setSampleRate((float)getSampleRate());
        right.setSampleRate((float)getSampleRate());
        makeup.setSampleRate((float)getSampleRate());
        applyAll();
    }

protected:
    const char* getLabel() const override { return "CustomDrive"; }
    const char* getDescription() const override { return "Custom overdrive pedal"; }
    const char* getMaker() const override { return "RigBuilder"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 0, 1); }
    int64_t getUniqueId() const override { return d_cconst('C', 'd', 'r', 'v'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index >= (uint32_t)kParamCount)
            return;
        parameter.hints = kParameterIsAutomatable;
        parameter.name = kCustomDriveNames[index];
        parameter.symbol = kCustomDriveSymbols[index];
        parameter.ranges.min = kCustomDriveMin[index];
        parameter.ranges.max = kCustomDriveMax[index];
        parameter.ranges.def = kCustomDriveDef[index];
    }

    float getParameterValue(uint32_t index) const override
    {
        return index < (uint32_t)kParamCount ? params[index] : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= (uint32_t)kParamCount)
            return;
        params[index] = clamp01(value);
        applyAll();
        makeup.snap();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        left.setSampleRate((float)newSampleRate);
        right.setSampleRate((float)newSampleRate);
        makeup.setSampleRate((float)newSampleRate);
        applyAll();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];
        for (uint32_t i = 0; i < frames; ++i)
        {
            // Auto makeup-gain: match output loudness to the dry input so the
            // drive's controls change only the amount of clip, not the level.
            makeup.processStereo(inL[i], inR[i], left.process(inL[i]), right.process(inR[i]), outL[i], outR[i]);
        }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomDrivePlugin)
};

Plugin* createPlugin()
{
    return new CustomDrivePlugin();
}

END_NAMESPACE_DISTRHO
