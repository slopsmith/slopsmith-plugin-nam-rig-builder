#ifndef QTRON_PARAMS_H
#define QTRON_PARAMS_H

// Shared parameter metadata for the plugin + its UI (keeps them in sync).
enum QTronParamId { kMode = 0, kAttack, kRelease, kRange, kPeak, kMix, kGain, kBoost, kParamCount };

static const char* const kQTronNames[kParamCount]   = { "Mode", "Attack", "Release", "Range", "Peak", "Mix", "Gain", "Boost" };
static const char* const kQTronSymbols[kParamCount] = { "mode", "attack", "release", "range", "peak", "mix", "gain", "boost" };
static const float kQTronMin[kParamCount] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
static const float kQTronMax[kParamCount] = { 2.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
// Defaults: Mode=Band Pass(1), Attack≈3.7ms, Release≈42ms, Range=High,
// Peak=0.4, Mix=0.5, Gain=0.8, Boost=0.2
static const float kQTronDef[kParamCount] = { 1.0f, 0.25f, 0.40f, 0.9f, 0.4f, 0.5f, 0.8f, 0.2f };

#endif // QTRON_PARAMS_H
