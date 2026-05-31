#ifndef BUZZ_TOO_PARAMS_H
#define BUZZ_TOO_PARAMS_H

enum BuzzTooParamId
{
    kGain = 0,
    kTone,
    kParamCount
};

static const char* const kBuzzTooNames[kParamCount] = {
    "Gain",
    "Tone",
};

static const char* const kBuzzTooSymbols[kParamCount] = {
    "gain",
    "tone",
};

static const float kBuzzTooMin[kParamCount] = { 0.0f, 0.0f };
static const float kBuzzTooMax[kParamCount] = { 1.0f, 1.0f };
static const float kBuzzTooDef[kParamCount] = { 0.64f, 0.46f };

#endif // BUZZ_TOO_PARAMS_H
