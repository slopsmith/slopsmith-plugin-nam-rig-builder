/* BassFilterEcho stompbox UI — shared pedal_ui template. Colour/knob layout
 * from the Rocksmith "Bass Filter Echo" art: dark maroon body, 4 knobs in a top
 * row (Time / Feedback / Mix / Filter), "ECHO" title. */
#include "BassFilterEchoParams.h"
#define PEDAL_TITLE  "ECHO"
#define PEDAL_NAMES  kBassFilterEchoNames
#define PEDAL_DEFS   kBassFilterEchoDef
#define PEDAL_ACR 110
#define PEDAL_ACG 69
#define PEDAL_ACB 47
#define PEDAL_ARCR 232
#define PEDAL_ARCG 196
#define PEDAL_ARCB 120
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Time, Feedback, Mix, Filter -> evenly across the top row
#define PEDAL_KNOBS { {0.17f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.61f,0.17f,0.085f}, {0.83f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
