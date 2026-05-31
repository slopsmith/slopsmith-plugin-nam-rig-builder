/* BassEnbig stompbox UI — shared pedal_ui template. Colour/knob layout from the
 * Rocksmith "Bass Enbiggenator" art: dark blue-grey body, 4 knobs in a top row
 * (Rate / Depth / Mix / Filter), cyan value arcs for the sci-fi vibe. */
#include "BassEnbigParams.h"
#define PEDAL_TITLE  "ENBIGGEN"
#define PEDAL_NAMES  kBassEnbigNames
#define PEDAL_DEFS   kBassEnbigDef
#define PEDAL_ACR 81
#define PEDAL_ACG 87
#define PEDAL_ACB 95
#define PEDAL_ARCR 110
#define PEDAL_ARCG 215
#define PEDAL_ARCB 230
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Rate, Depth, Mix, Filter -> evenly across the top row
#define PEDAL_KNOBS { {0.17f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.61f,0.17f,0.085f}, {0.83f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
