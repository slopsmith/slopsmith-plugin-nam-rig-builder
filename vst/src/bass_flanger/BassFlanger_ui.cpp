/* BassFlanger stompbox UI — shared pedal_ui template. Shape/colour/knob layout
 * from the real Boss BF-3: purple body, 4 knobs in a top row
 * (Res-Manual/Depth/Rate/Mode -> Filter/Depth/Rate/Mix). */
#include "BassFlangerParams.h"
#define PEDAL_TITLE  "FLANGER"
#define PEDAL_NAMES  kBassFlangerNames
#define PEDAL_DEFS   kBassFlangerDef
#define PEDAL_ACR 175
#define PEDAL_ACG 90
#define PEDAL_ACB 160
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Rate, Depth, Filter, Mix -> BF-3 row positions (Res/Manual,
// Depth, Rate, Mode left→right) => Filter@0.17, Depth@0.39, Rate@0.61, Mix@0.83
#define PEDAL_KNOBS { {0.61f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.17f,0.17f,0.085f}, {0.83f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
