#ifndef V4B_PARAMS_H
#define V4B_PARAMS_H

// "Sampleg V-4B" — Ampeg V-4B front panel, 1:1:
//   Inputs : Normal / -15 dB pad (the V-4B's second, padded input jack).
//   Gain   : drives the 12AX7 preamp (V1) -> grit/growl when pushed.
//   Ultra Lo : fixed loudness contour (boost deep lows + highs, scoop low-mids).
//   Ultra Hi : presence/treble boost (adds bite).
//   Bass   : low shelf (~70 Hz).
//   Midrange : peaking cut/boost at the Frequency-selected centre.
//   Frequency: 3-position midrange selector — 220 / 800 / 3000 Hz (V-4B mid switch).
//   Treble : high shelf (~5 kHz).
//   Master : output level into the 4x 7027A push-pull (~100W).
enum V4bParamId {
    kGain = 0, kBass, kMidrange, kFreq, kTreble, kMaster,   // knobs
    kPad, kUltraLo, kUltraHi,                               // switches
    kParamCount
};

static const char* const kV4bNames[kParamCount] = {
    "Gain", "Bass", "Midrange", "Frequency", "Treble", "Master",
    "-15dB", "Ultra Lo", "Ultra Hi"
};
static const char* const kV4bSymbols[kParamCount] = {
    "gain", "bass", "midrange", "frequency", "treble", "master",
    "pad", "ultralo", "ultrahi"
};
static const float kV4bMin[kParamCount] = { 0,0,0,0,0,0, 0,0,0 };
static const float kV4bMax[kParamCount] = { 1,1,1,1,1,1, 1,1,1 };
// Tone knobs flat at 0.5; Frequency 0.5 -> the centre (800 Hz) detent; Gain
// 0.5; Master 0.7 ~ unity; switches off.
static const float kV4bDef[kParamCount] = {
    0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.70f,
    0.00f, 0.00f, 0.00f
};

// The 3 midrange-selector centre frequencies (Hz), in panel order
// (V-4B mid switch: low / mid / high mid).
static const float kV4bMidFreqs[3] = { 220.f, 800.f, 3000.f };

#endif // V4B_PARAMS_H
