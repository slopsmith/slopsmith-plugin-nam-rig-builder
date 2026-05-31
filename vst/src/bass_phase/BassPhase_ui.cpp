/* BassPhase stompbox UI — shared pedal_ui template. Shape/colour/knob layout
 * from the Rocksmith "Bass Phase" art: copper/bronze body, 4 knobs in a top row
 * (Rate / Depth / Mix / Filter, left→right), "PHASE" title. */
#include "BassPhaseParams.h"
#define PEDAL_TITLE  "PHASE"
#define PEDAL_NAMES  kBassPhaseNames
#define PEDAL_DEFS   kBassPhaseDef
#define PEDAL_ACR 163
#define PEDAL_ACG 128
#define PEDAL_ACB 105
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Rate, Depth, Mix, Filter -> evenly across the top row
#define PEDAL_KNOBS { {0.17f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.61f,0.17f,0.085f}, {0.83f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
