/* BassOverdrive stompbox UI — shared pedal_ui template. Shape/colour/knob
 * layout from the real Darkglass Microtubes B3K: black body, 4 knobs in a 2x2
 * grid (Blend top-left, Tone top-right, Filter bottom-left, Gain/Drive
 * bottom-right), white value arcs. */
#include "BassOverdriveParams.h"
#define PEDAL_TITLE  "B3K"
#define PEDAL_NAMES  kBassOverdriveNames
#define PEDAL_DEFS   kBassOverdriveDef
#define PEDAL_ACR 30
#define PEDAL_ACG 31
#define PEDAL_ACB 35
#define PEDAL_ARCR 216
#define PEDAL_ARCG 220
#define PEDAL_ARCB 228
#define PEDAL_W 340
#define PEDAL_H 420
// index order: Blend, Gain, Filter, Tone  ->  TL, BR, BL, TR (B3K positions)
#define PEDAL_KNOBS { {0.30f,0.17f,0.10f}, {0.70f,0.42f,0.10f}, {0.30f,0.42f,0.10f}, {0.70f,0.17f,0.10f} }
#include "../_shared/pedal_ui.hpp"
