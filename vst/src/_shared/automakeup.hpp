#ifndef RB_AUTOMAKEUP_HPP
#define RB_AUTOMAKEUP_HPP

#include <cmath>

/*
 * RBAutoMakeup — loudness-matched auto makeup-gain (stereo-linked).
 *
 * Tracks the slow RMS of the dry input and of the wet (processed) output and
 * scales the wet so that RMS_out == RMS_in. This decouples a drive pedal's
 * Gain knob from its output level: Gain changes how hard the signal clips, NOT
 * how loud the pedal is, so every distortion / overdrive / fuzz sits at the
 * same level as the bypassed (dry) signal — and therefore at the same level as
 * each other.
 *
 * The RMS window is symmetric (~200 ms) so the loudness match is UNBIASED — at
 * any fixed Gain the output settles to exactly the dry level. That slow window
 * alone would leave a brief loud "blip" the instant the Gain knob is turned up
 * (the envelope needs ~200 ms to notice the louder output). To kill that blip
 * WITHOUT biasing the steady-state match, the plugin calls snap() whenever a
 * parameter changes: snap() opens a short window during which the envelopes
 * track with a fast (~8 ms) coefficient, so the makeup catches the new level
 * almost immediately while the knob is moving, then reverts to the accurate slow
 * window once the knob settles. Normal playing dynamics never trigger snap(), so
 * the makeup never acts like a compressor. Near silence the ratio is frozen so
 * the noise floor is never boosted.
 *
 * STEREO LINKING — use processStereo() for anything with two correlated
 * channels (every drive/distortion/fuzz: L and R are the same mono guitar).
 * It drives ONE shared envelope/gain from the average of both channels and
 * applies that single gain to both, so the makeup can never make gainL != gainR.
 * Two *independent* makeup instances (the old per-channel process()) would let
 * the envelopes drift apart whenever the host feeds L != R (dither, an upstream
 * stereo block, DC offset). A drifting L/R gain difference is a slowly moving
 * inter-channel level imbalance — which is heard as a slow PHASER/sweep riding
 * on top of the tone. Linking removes it. process() (mono, per-channel) is kept
 * only for genuinely mono-style users (wahs, octavers) where it is acceptable.
 *
 * Usage (stereo, preferred):
 *     run():               makeup.processStereo(inL[i], inR[i],
 *                                               coreL.process(inL[i]),
 *                                               coreR.process(inR[i]),
 *                                               outL[i], outR[i]);
 *     setParameterValue(): makeup.snap();
 */
struct RBAutoMakeup
{
    float slowCoef = 0.0f;  // accurate one-pole coefficient (~200 ms)
    float fastCoef = 0.0f;  // fast coefficient used right after a knob change
    float gainSlew = 0.0f;  // one-pole smoothing of the applied gain (~30 ms)
    float inEnv    = 0.0f;  // mean-square of the dry signal
    float outEnv   = 0.0f;  // mean-square of the wet signal
    float gain     = 1.0f;  // makeup gain currently applied
    int   fast     = 0;     // samples remaining in the fast (snap) window
    int   fastLen  = 0;     // length of the fast window in samples
    int   cool     = 0;     // refractory samples before snap() may re-arm
    int   coolLen  = 0;     // length of the refractory period in samples

    void setSampleRate(float sr)
    {
        if (sr < 1000.0f)
            sr = 48000.0f;
        slowCoef = std::exp(-1.0f / (0.200f * sr));   // ~200 ms RMS window
        fastCoef = std::exp(-1.0f / (0.008f * sr));   // ~8 ms during a snap
        gainSlew = std::exp(-1.0f / (0.030f * sr));   // ~30 ms glide on the gain
        fastLen  = (int)(0.040f * sr);                // ~40 ms snap window
        coolLen  = (int)(0.250f * sr);                // ~250 ms refractory
        reset();
    }

    void reset()
    {
        inEnv = outEnv = 0.0f;
        gain = 1.0f;
        fast = 0;
        cool = 0;
    }

    // Call when a parameter (Gain/Tone/…) changes so the makeup re-levels fast.
    // A refractory period after each snap window keeps a host that automates a
    // knob at control rate from re-arming the fast window back-to-back, which
    // would pin the detector in fast mode and turn the makeup into a pumping
    // compressor. A single knob move still gets its full ~40 ms fast re-level.
    void snap()
    {
        if (fast == 0 && cool == 0)
        {
            fast = fastLen;
            cool = coolLen;
        }
    }

    // Update gain from the current envelopes. The applied gain is glided (~30 ms)
    // so a regime change (clipped -> linear as a note decays) can never step the
    // level; during a snap window it tracks the target immediately so a knob move
    // re-levels without a blip.
    inline void updateGain()
    {
        // Only chase a new target when there is real output AND input energy;
        // this freezes the ratio during silence so hiss is not amplified. The
        // thresholds sit well below normal playing level (~-50/-70 dBFS RMS) but
        // high enough that a decaying note actually reaches the freeze.
        if (outEnv > 1.0e-5f && inEnv > 1.0e-7f)
        {
            float target = std::sqrt(inEnv / outEnv);
            if (target > 4.0f)
                target = 4.0f;                       // safety ceiling (+12 dB)
            const float gc = (fast > 0) ? 0.0f : gainSlew;
            gain = gc * gain + (1.0f - gc) * target;
        }
    }

    // MONO, per-channel. Kept for mono-style users (wahs, octavers). Two
    // separate instances will drift apart on L != R — prefer processStereo().
    float process(float dry, float wet)
    {
        const float c = (fast > 0) ? fastCoef : slowCoef;
        if (fast > 0)
            --fast;
        if (cool > 0)
            --cool;

        inEnv  = c * inEnv  + (1.0f - c) * dry * dry;
        outEnv = c * outEnv + (1.0f - c) * wet * wet;
        updateGain();
        return wet * gain;
    }

    // STEREO-LINKED. One shared envelope/gain from the average of both channels,
    // applied identically to L and R — gainL == gainR by construction, so the
    // makeup can never introduce an inter-channel imbalance (no phaser/sweep).
    void processStereo(float dryL, float dryR, float wetL, float wetR,
                       float& outL, float& outR)
    {
        const float c = (fast > 0) ? fastCoef : slowCoef;
        if (fast > 0)
            --fast;
        if (cool > 0)
            --cool;

        inEnv  = c * inEnv  + (1.0f - c) * 0.5f * (dryL * dryL + dryR * dryR);
        outEnv = c * outEnv + (1.0f - c) * 0.5f * (wetL * wetL + wetR * wetR);
        updateGain();
        outL = wetL * gain;
        outR = wetR * gain;
    }
};

#endif // RB_AUTOMAKEUP_HPP
