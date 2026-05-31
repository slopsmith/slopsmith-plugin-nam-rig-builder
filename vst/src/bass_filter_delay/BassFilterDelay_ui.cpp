/* BassFilterDelay stompbox UI — shared pedal_ui template. Colour/knob layout
 * from the Rocksmith "Bass Filter Delay" art: deep-blue body, 4 knobs in a top
 * row (Time / Feedback / Mix / Filter), "DELAY" title. */
#include "BassFilterDelayParams.h"
#define PEDAL_TITLE  "DELAY"
#define PEDAL_NAMES  kBassFilterDelayNames
#define PEDAL_DEFS   kBassFilterDelayDef
#define PEDAL_ACR 27
#define PEDAL_ACG 56
#define PEDAL_ACB 140
#define PEDAL_ARCR 150
#define PEDAL_ARCG 185
#define PEDAL_ARCB 240
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Time, Feedback, Mix, Filter -> evenly across the top row
#define PEDAL_KNOBS { {0.17f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.61f,0.17f,0.085f}, {0.83f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
