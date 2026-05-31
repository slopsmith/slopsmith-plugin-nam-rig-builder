/* BassFuzz stompbox UI — shared pedal_ui template. Shape/colour/knob layout
 * based on the real EHX Bass Big Muff Pi: olive-green body, 3 knobs in a top
 * row (Volume/Tone/Sustain -> here Gain/Tone/Filter), logo centred below. */
#include "BassFuzzParams.h"
#define PEDAL_TITLE  "BASS FUZZ"
#define PEDAL_NAMES  kBassFuzzNames
#define PEDAL_DEFS   kBassFuzzDef
#define PEDAL_ACR 84
#define PEDAL_ACG 126
#define PEDAL_ACB 56
#define PEDAL_W 340
#define PEDAL_H 400
#define PEDAL_KNOBS { {0.25f,0.20f,0.10f}, {0.50f,0.20f,0.10f}, {0.75f,0.20f,0.10f} }
#include "../_shared/pedal_ui.hpp"
