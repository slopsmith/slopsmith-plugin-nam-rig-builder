/*
 * MarshallGuvnorPlus - Marshall GV-2/Guv'nor Plus style drive for Rocksmith.
 *
 * Local references: pedals/Marshall GV2_1.png and pedals/marshall gv2_2.gif.
 * The circuit uses TL072 gain stages, LED/diode clipping, Bass/Mid/Treble tone
 * stack, and a Deep low-end control. The real Volume control is internally
 * compensated because Rocksmith pedal slots generally do not expose output.
 */
#include "DistrhoPlugin.hpp"
#include "MarshallGuvnorPlusParams.h"
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
    return std::fmax(20.0f, std::fmin(hz, nyquist));
}

static inline float smoothstep(float v)
{
    v = clamp01(v);
    return v * v * (3.0f - 2.0f * v);
}

static inline float softClip(float x)
{
    return std::tanh(x);
}

static inline float ledClip(float x, float threshold)
{
    return threshold * std::tanh(x / threshold);
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

    void setLowShelf(float sr, float hz, float slope, float gainDb)
    {
        hz = clampFreq(hz, sr);
        const float a = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * hz / sr;
        const float c = std::cos(w0);
        const float s = std::sin(w0);
        const float rootA = std::sqrt(a);
        const float alpha = s * 0.5f * std::sqrt((a + 1.0f / a) * (1.0f / slope - 1.0f) + 2.0f);

        set(a * ((a + 1.0f) - (a - 1.0f) * c + 2.0f * rootA * alpha),
            2.0f * a * ((a - 1.0f) - (a + 1.0f) * c),
            a * ((a + 1.0f) - (a - 1.0f) * c - 2.0f * rootA * alpha),
            (a + 1.0f) + (a - 1.0f) * c + 2.0f * rootA * alpha,
            -2.0f * ((a - 1.0f) + (a + 1.0f) * c),
            (a + 1.0f) + (a - 1.0f) * c - 2.0f * rootA * alpha);
    }
};

// Passive Marshall (TMB) tone stack. Models the real interacting Bass/Mid/
// Treble network as the 3rd-order continuous-time transfer function of
// Yeh & Smith (DAFx-06, "Discretization of the '59 Fender Bassman Tone Stack";
// the FMV / Bassman / Marshall networks differ only by component values),
// discretized with the bilinear transform (c = 2*fs). Unlike three independent
// shelving/peaking filters, the controls here share RC nodes and INTERACT, so
// changing one knob shifts the others' bands -- the genuine Marshall behaviour
// (mid scoop, treble affecting the midrange, bass shifting the scoop).
class MarshallToneStack
{
    // Standard MARSHALL (JCM800-era) component values, in the symbol convention
    // of Yeh & Smith Fig. 1: R1 = treble pot total, R4 = slope resistor,
    // R2 = bass pot total, R3 = mid pot total, C1 = treble cap, C2 = bass cap,
    // C3 = mid cap.
    static constexpr double R1 = 250.0e3;   // treble pot
    static constexpr double R2 = 1.0e6;     // bass pot
    static constexpr double R3 = 25.0e3;    // mid pot
    static constexpr double R4 = 33.0e3;    // slope resistor (Marshall 33k)
    static constexpr double C1 = 0.47e-9;   // treble cap 470 pF
    static constexpr double C2 = 22.0e-9;   // bass cap   22 nF
    static constexpr double C3 = 22.0e-9;   // mid cap    22 nF

    float  sr     = 48000.0f;
    double makeup = 2.0;    // fixed +6 dB makeup (the passive stack has
                            // insertion loss; RBAutoMakeup re-levels anyway).

    // Direct-form-II transposed, 3rd order, double-precision state.
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, b3 = 0.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double z1 = 0.0, z2 = 0.0, z3 = 0.0;

    static double clampPot(double v)
    {
        if (v < 0.001) v = 0.001;     // keep pot resistances strictly nonzero
        if (v > 0.999) v = 0.999;
        return v;
    }

public:
    void setSampleRate(float s) { sr = (s > 1000.0f) ? s : 48000.0f; reset(); }

    void reset() { z1 = z2 = z3 = 0.0; }

    void setParams(float bassN, float midN, float trebleN)
    {
        const double t = clampPot(trebleN);
        const double m = clampPot(midN);
        double l = clampPot(bassN);
        l = l * l * (3.0 - 2.0 * l);   // gentle log-ish taper on the bass pot

        const double R1d=R1, R2d=R2, R3d=R3, R4d=R4, C1d=C1, C2d=C2, C3d=C3;
        const double R3sq = R3d * R3d;

        // ---- Continuous-time coefficients (Yeh & Smith eq. 1, verbatim) ----
        const double b1c = t*C1d*R1d + m*C3d*R3d + l*(C1d*R2d + C2d*R2d)
                         + (C1d*R3d + C2d*R3d);

        const double b2c = t*(C1d*C2d*R1d*R4d + C1d*C3d*R1d*R4d)
                         - m*m*(C1d*C3d*R3sq + C2d*C3d*R3sq)
                         + m*(C1d*C3d*R1d*R3d + C1d*C3d*R3sq + C2d*C3d*R3sq)
                         + l*(C1d*C2d*R1d*R2d + C1d*C2d*R2d*R4d + C1d*C3d*R2d*R4d)
                         + l*m*(C1d*C3d*R2d*R3d + C2d*C3d*R2d*R3d)
                         + (C1d*C2d*R1d*R3d + C1d*C2d*R3d*R4d + C1d*C3d*R3d*R4d);

        const double b3c = l*m*(C1d*C2d*C3d*R1d*R2d*R3d + C1d*C2d*C3d*R2d*R3d*R4d)
                         - m*m*(C1d*C2d*C3d*R1d*R3sq + C1d*C2d*C3d*R3sq*R4d)
                         + m*(C1d*C2d*C3d*R1d*R3sq + C1d*C2d*C3d*R3sq*R4d)
                         + t*C1d*C2d*C3d*R1d*R3d*R4d
                         - t*m*C1d*C2d*C3d*R1d*R3d*R4d
                         + t*l*C1d*C2d*C3d*R1d*R2d*R4d;

        const double a0c = 1.0;

        const double a1c = (C1d*R1d + C1d*R3d + C2d*R3d + C2d*R4d + C3d*R4d)
                         + m*C3d*R3d + l*(C1d*R2d + C2d*R2d);

        const double a2c = m*(C1d*C3d*R1d*R3d - C2d*C3d*R3d*R4d + C1d*C3d*R3sq + C2d*C3d*R3sq)
                         + l*m*(C1d*C3d*R2d*R3d + C2d*C3d*R2d*R3d)
                         - m*m*(C1d*C3d*R3sq + C2d*C3d*R3sq)
                         + l*(C1d*C2d*R2d*R4d + C1d*C2d*R1d*R2d + C1d*C3d*R2d*R4d + C2d*C3d*R2d*R4d)
                         + (C1d*C2d*R1d*R4d + C1d*C3d*R1d*R4d + C1d*C2d*R3d*R4d
                            + C1d*C2d*R1d*R3d + C1d*C3d*R3d*R4d + C2d*C3d*R3d*R4d);

        const double a3c = l*m*(C1d*C2d*C3d*R1d*R2d*R3d + C1d*C2d*C3d*R2d*R3d*R4d)
                         - m*m*(C1d*C2d*C3d*R1d*R3sq + C1d*C2d*C3d*R3sq*R4d)
                         + m*(C1d*C2d*C3d*R3sq*R4d + C1d*C2d*C3d*R1d*R3sq - C1d*C2d*C3d*R1d*R3d*R4d)
                         + l*C1d*C2d*C3d*R1d*R2d*R4d + C1d*C2d*C3d*R1d*R3d*R4d;

        // ---- Bilinear transform, c = 2*fs (Yeh & Smith eq. 2, verbatim) ----
        const double c  = 2.0 * (double)sr;
        const double c2 = c * c;
        const double c3 = c2 * c;

        const double B0 = -b1c*c - b2c*c2 - b3c*c3;       // b0c == 0
        const double B1 = -b1c*c + b2c*c2 + 3.0*b3c*c3;
        const double B2 =  b1c*c + b2c*c2 - 3.0*b3c*c3;
        const double B3 =  b1c*c - b2c*c2 + b3c*c3;
        const double A0 = -a0c - a1c*c - a2c*c2 - a3c*c3;
        const double A1 = -3.0*a0c - a1c*c + a2c*c2 + 3.0*a3c*c3;
        const double A2 = -3.0*a0c + a1c*c + a2c*c2 - 3.0*a3c*c3;
        const double A3 = -a0c + a1c*c - a2c*c2 + a3c*c3;

        const double inv = 1.0 / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv; b3 = B3 * inv;
        a1 = A1 * inv; a2 = A2 * inv; a3 = A3 * inv;
    }

    float process(float x)
    {
        const double xn = (double)x;
        const double y = b0 * xn + z1;
        z1 = b1 * xn - a1 * y + z2;
        z2 = b2 * xn - a2 * y + z3;
        z3 = b3 * xn - a3 * y;
        return (float)(y * makeup);
    }
};

} // namespace

class MarshallGuvnorPlusCore
{
    float sampleRate = 48000.0f;
    float gain = kMarshallGuvnorPlusDef[kGain];
    float bass = kMarshallGuvnorPlusDef[kBass];
    float mid = kMarshallGuvnorPlusDef[kMid];
    float treble = kMarshallGuvnorPlusDef[kTreble];
    float deep = kMarshallGuvnorPlusDef[kDeep];

    Biquad inputHp;
    Biquad deepShelf;
    Biquad preVoice;
    Biquad clipRollOff;
    MarshallToneStack toneStack;
    Biquad outputLp;

    void updateFilters()
    {
        const float g = smoothstep(gain);
        inputHp.setHighPass(sampleRate, 58.0f + 60.0f * gain, 0.70f);
        deepShelf.setLowShelf(sampleRate, 95.0f + 45.0f * deep, 0.78f,
                              -2.0f + 9.5f * deep);
        preVoice.setPeaking(sampleRate, 680.0f + 420.0f * mid, 0.78f,
                            1.4f + 2.6f * g);
        clipRollOff.setLowPass(sampleRate, 7200.0f - 1800.0f * g + 1000.0f * treble, 0.68f);
        // Real passive Marshall TMB tone stack: the Bass/Mid/Treble controls
        // share RC nodes and interact (vs. the old three independent filters).
        toneStack.setParams(bass, mid, treble);
        outputLp.setLowPass(sampleRate, 3900.0f + 7600.0f * treble, 0.62f);
    }

public:
    void reset()
    {
        inputHp.reset();
        deepShelf.reset();
        preVoice.reset();
        clipRollOff.reset();
        toneStack.reset();
        outputLp.reset();
        updateFilters();
    }

    void setSampleRate(float sr)
    {
        sampleRate = sr > 1000.0f ? sr : 48000.0f;
        toneStack.setSampleRate(sampleRate);
        reset();
    }

    void setGain(float v) { gain = clamp01(v); updateFilters(); }
    void setBass(float v) { bass = clamp01(v); updateFilters(); }
    void setMid(float v) { mid = clamp01(v); updateFilters(); }
    void setTreble(float v) { treble = clamp01(v); updateFilters(); }
    void setDeep(float v) { deep = clamp01(v); updateFilters(); }

    float process(float in)
    {
        const float g = smoothstep(gain);
        float x = inputHp.process(in);
        x = deepShelf.process(x);
        x = preVoice.process(x);

        const float drive = 1.25f + 6.0f * gain + 12.0f * g;
        float y = x * drive;
        y = ledClip(y, 0.68f - 0.18f * gain);
        y = 0.82f * y + 0.18f * softClip(y * (1.7f + 2.0f * gain));
        y = clipRollOff.process(y);

        const float cleanLeak = 0.12f * (1.0f - gain);
        y = y * (1.0f - cleanLeak) + x * cleanLeak;

        y = toneStack.process(y);
        y = outputLp.process(y);

        const float level = 0.74f / (1.0f + 0.36f * gain + 0.20f * g + 0.12f * deep);
        return softClip(y * level) * 0.98f;
    }
};

class MarshallGuvnorPlusPlugin : public Plugin
{
    MarshallGuvnorPlusCore left;
    MarshallGuvnorPlusCore right;
    RBAutoMakeup makeup;
    float params[kParamCount];

    void applyAll()
    {
        left.setGain(params[kGain]);
        right.setGain(params[kGain]);
        left.setBass(params[kBass]);
        right.setBass(params[kBass]);
        left.setMid(params[kMid]);
        right.setMid(params[kMid]);
        left.setTreble(params[kTreble]);
        right.setTreble(params[kTreble]);
        left.setDeep(params[kDeep]);
        right.setDeep(params[kDeep]);
    }

public:
    MarshallGuvnorPlusPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        for (int i = 0; i < kParamCount; ++i)
            params[i] = kMarshallGuvnorPlusDef[i];
        left.setSampleRate((float)getSampleRate());
        right.setSampleRate((float)getSampleRate());
        makeup.setSampleRate((float)getSampleRate());
        applyAll();
    }

protected:
    const char* getLabel() const override { return "MarshallGuvnorPlus"; }
    const char* getDescription() const override { return "Marshall GV-2 style drive"; }
    const char* getMaker() const override { return "RigBuilder"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('M', 'r', 'G', 'v'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index >= (uint32_t)kParamCount)
            return;
        parameter.hints = kParameterIsAutomatable;
        parameter.name = kMarshallGuvnorPlusNames[index];
        parameter.symbol = kMarshallGuvnorPlusSymbols[index];
        parameter.ranges.min = kMarshallGuvnorPlusMin[index];
        parameter.ranges.max = kMarshallGuvnorPlusMax[index];
        parameter.ranges.def = kMarshallGuvnorPlusDef[index];
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

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarshallGuvnorPlusPlugin)
};

Plugin* createPlugin()
{
    return new MarshallGuvnorPlusPlugin();
}

END_NAMESPACE_DISTRHO
