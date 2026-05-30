#ifndef FUZZ_WAS_HE_PARAMS_H
#define FUZZ_WAS_HE_PARAMS_H

enum FuzzWasHeParamId
{
    kGain = 0,
    kTone,
    kParamCount
};

static const char* const kFuzzWasHeNames[kParamCount] = {
    "Gain",
    "Tone",
};

static const char* const kFuzzWasHeSymbols[kParamCount] = {
    "gain",
    "tone",
};

static const float kFuzzWasHeMin[kParamCount] = { 0.0f, 0.0f };
static const float kFuzzWasHeMax[kParamCount] = { 1.0f, 1.0f };
static const float kFuzzWasHeDef[kParamCount] = { 0.70f, 0.50f };

#endif // FUZZ_WAS_HE_PARAMS_H
