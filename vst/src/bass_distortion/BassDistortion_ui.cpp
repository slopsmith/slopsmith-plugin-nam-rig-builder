/* BassDistortion stompbox UI — shared pedal_ui template. Shape/colour/knob
 * layout from the real Pro Co RAT 2: matte-black body, 3 knobs in a top row
 * (Distortion/Filter/Volume positions -> Gain, Filter, Tone), white value arcs,
 * big RAT logo centred. */
#include "BassDistortionParams.h"
#define PEDAL_TITLE  "RAT"
#define PEDAL_NAMES  kBassDistortionNames
#define PEDAL_DEFS   kBassDistortionDef
#define PEDAL_ACR 32
#define PEDAL_ACG 33
#define PEDAL_ACB 36
#define PEDAL_ARCR 216
#define PEDAL_ARCG 220
#define PEDAL_ARCB 228
#define PEDAL_W 340
#define PEDAL_H 420
// index order: Gain, Tone, Filter -> left, right, centre (RAT knob row)
#define PEDAL_KNOBS { {0.22f,0.20f,0.105f}, {0.78f,0.20f,0.105f}, {0.50f,0.20f,0.105f} }
#include "../_shared/pedal_ui.hpp"
