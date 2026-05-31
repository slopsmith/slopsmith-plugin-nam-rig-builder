/* BassChorus stompbox UI — shared pedal_ui template. Shape/colour/knob layout
 * from the real Boss CEB-3: light-blue body, 4 knobs in a top row
 * (E.Level/Low Filter/Rate/Depth -> Mix/LoFilter/Rate/Depth). */
#include "BassChorusParams.h"
#define PEDAL_TITLE  "BASS CHORUS"
#define PEDAL_NAMES  kBassChorusNames
#define PEDAL_DEFS   kBassChorusDef
#define PEDAL_ACR 106
#define PEDAL_ACG 159
#define PEDAL_ACB 203
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Rate, Depth, LoFilter, Mix -> CEB-3 row positions (E.Level,
// Low Filter, Rate, Depth left→right) => Mix@0.17, LoFilter@0.39, Rate@0.61, Depth@0.83
#define PEDAL_KNOBS { {0.61f,0.17f,0.085f}, {0.83f,0.17f,0.085f}, {0.39f,0.17f,0.085f}, {0.17f,0.17f,0.085f} }
#include "../_shared/pedal_ui.hpp"
