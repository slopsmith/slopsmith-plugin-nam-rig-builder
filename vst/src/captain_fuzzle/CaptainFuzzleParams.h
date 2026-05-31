#ifndef CAPTAIN_FUZZLE_PARAMS_H
#define CAPTAIN_FUZZLE_PARAMS_H

enum CaptainFuzzleParamId
{
    kGain = 0,
    kTone,
    kParamCount
};

static const char* const kCaptainFuzzleNames[kParamCount] = {
    "Gain",
    "Tone",
};

static const char* const kCaptainFuzzleSymbols[kParamCount] = {
    "gain",
    "tone",
};

static const float kCaptainFuzzleMin[kParamCount] = { 0.0f, 0.0f };
static const float kCaptainFuzzleMax[kParamCount] = { 1.0f, 1.0f };
static const float kCaptainFuzzleDef[kParamCount] = { 0.78f, 0.62f };

#endif // CAPTAIN_FUZZLE_PARAMS_H
