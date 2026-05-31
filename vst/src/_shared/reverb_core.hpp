/*
 * Shared Freeverb-style stereo reverb core for the bundled Rocksmith reverb
 * racks (Studio Verb / Chamber / Plate). Eight damped feedback combs in
 * parallel into four series all-pass diffusers per channel, plus a light post
 * modulation on the wet (the "Depth" knob) for a lush, slightly chorused tail.
 *
 * One voicing per rack via ReverbCore::setVoicing(sizeScale, dampBias, apFb):
 *   Verb    — hall: full-size combs, moderate damping
 *   Chamber — denser/darker: shorter combs, more damping
 *   Plate   — bright/metallic: short combs, light damping, high diffusion
 *
 * Knobs are the four every reverb rack shares:
 *   Time  -> decay (comb feedback)
 *   Tone  -> damping (dark .. bright)
 *   Depth -> wet tail modulation depth
 *   Mix   -> wet/dry blend
 */
#ifndef REVERB_CORE_HPP
#define REVERB_CORE_HPP
#include <cmath>
#include <cstring>

// Freeverb tunings (samples @ 44.1 kHz)
static const int kCombTune[8]    = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int kAllpassTune[4] = { 556, 441, 341, 225 };
static const int kStereoSpread   = 23;
static const int kCombMax        = 4000;   // headroom for 96 kHz + sizeScale
static const int kApMax          = 1500;

struct RvComb {
    float buf[kCombMax]; int size = 1116, p = 0; float store = 0.f, fb = 0.8f, d1 = 0.2f, d2 = 0.8f;
    void clear() { std::memset(buf, 0, sizeof(buf)); p = 0; store = 0.f; }
    void set(int s, float feedback, float damp) {
        size = (s < 1) ? 1 : (s > kCombMax ? kCombMax : s);
        fb = feedback; d1 = damp; d2 = 1.f - damp;
    }
    inline float process(float in) {
        float y = buf[p];
        store = y * d2 + store * d1;
        buf[p] = in + store * fb;
        if (++p >= size) p = 0;
        return y;
    }
};

struct RvAllpass {
    float buf[kApMax]; int size = 556, p = 0; float fb = 0.5f;
    void clear() { std::memset(buf, 0, sizeof(buf)); p = 0; }
    void set(int s, float feedback) { size = (s < 1) ? 1 : (s > kApMax ? kApMax : s); fb = feedback; }
    inline float process(float in) {
        float bufout = buf[p];
        buf[p] = in + bufout * fb;
        if (++p >= size) p = 0;
        return bufout - in;
    }
};

class ReverbCore {
    float fs = 48000.f;
    RvComb    combL[8],   combR[8];
    RvAllpass apL[4],     apR[4];
    // post modulation (Depth)
    float modBufL[2048], modBufR[2048]; int mw = 0;
    float lfoPh = 0.f, lfoInc = 0.f, modDepth = 0.f;
    float mix = 0.3f;
    // voicing
    float sizeScale = 1.f, dampBias = 0.f, apFb = 0.5f;

    inline float modRead(const float* buf, float delaySamp) {
        float rp = (float)mw - delaySamp;
        while (rp < 0.f) rp += 2048.f;
        int i0 = (int)rp; float fr = rp - (float)i0;
        int i1 = i0 + 1; if (i1 >= 2048) i1 -= 2048;
        return buf[i0] + fr * (buf[i1] - buf[i0]);
    }
public:
    void setVoicing(float sScale, float dBias, float apFeedback) {
        sizeScale = sScale; dampBias = dBias; apFb = apFeedback;
    }
    void setSampleRate(float s) {
        fs = (s > 0.f) ? s : 48000.f;
        lfoInc = 6.2831853f * 0.7f / fs;
        clear();
    }
    void clear() {
        for (int i = 0; i < 8; ++i) { combL[i].clear(); combR[i].clear(); }
        for (int i = 0; i < 4; ++i) { apL[i].clear();   apR[i].clear(); }
        std::memset(modBufL, 0, sizeof(modBufL)); std::memset(modBufR, 0, sizeof(modBufR));
        mw = 0; lfoPh = 0.f;
    }
    void setParams(float time, float tone, float depth, float mixP) {
        const float sr = fs / 44100.0f * sizeScale;
        const float feedback = 0.70f + time * 0.275f;                 // 0.70 .. 0.975
        float damp = (1.0f - tone) * 0.45f + dampBias;                // brighter tone → less damping
        if (damp < 0.f) damp = 0.f; if (damp > 0.95f) damp = 0.95f;
        for (int i = 0; i < 8; ++i) {
            combL[i].set((int)(kCombTune[i] * sr), feedback, damp);
            combR[i].set((int)((kCombTune[i] + kStereoSpread) * sr), feedback, damp);
        }
        for (int i = 0; i < 4; ++i) {
            apL[i].set((int)(kAllpassTune[i] * sr), apFb);
            apR[i].set((int)((kAllpassTune[i] + kStereoSpread) * sr), apFb);
        }
        modDepth = depth * 0.0022f * fs;                              // up to ~2.2 ms wet wobble
        mix = mixP;
    }
    inline void process(float xL, float xR, float& outL, float& outR) {
        const float in = (xL + xR) * 0.5f * 0.30f;                    // mono feed, scaled (Freeverb gain)
        float wL = 0.f, wR = 0.f;
        for (int i = 0; i < 8; ++i) { wL += combL[i].process(in); wR += combR[i].process(in); }
        for (int i = 0; i < 4; ++i) { wL = apL[i].process(wL);  wR = apR[i].process(wR); }

        // light wet modulation (Depth) — store then read with LFO offset
        modBufL[mw] = wL; modBufR[mw] = wR;
        lfoPh += lfoInc; if (lfoPh > 6.2831853f) lfoPh -= 6.2831853f;
        const float off = 4.0f + modDepth * (0.5f + 0.5f * std::sin(lfoPh));
        const float offR = 4.0f + modDepth * (0.5f + 0.5f * std::sin(lfoPh + 1.7f));
        float mL = modRead(modBufL, off), mR = modRead(modBufR, offR);
        if (++mw >= 2048) mw = 0;
        // blend a little modulated signal in (subtle)
        wL = wL * 0.5f + mL * 0.5f;
        wR = wR * 0.5f + mR * 0.5f;

        outL = xL * (1.0f - mix) + wL * mix;
        outR = xR * (1.0f - mix) + wR * mix;
    }
};

#endif // REVERB_CORE_HPP
