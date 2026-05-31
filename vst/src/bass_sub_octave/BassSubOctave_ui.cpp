/* BassSubOctave stompbox UI — shared pedal_ui template. Shape/colour from the
 * real Boss OC-2 (chocolate-brown Boss compact); knob layout from the Rocksmith
 * "Bass Sub Octave" art: two knobs in a top row (Mix left, Tone right). */
#include "BassSubOctaveParams.h"
#define PEDAL_TITLE  "SUB OCTAVE"
#define PEDAL_NAMES  kBassSubOctaveNames
#define PEDAL_DEFS   kBassSubOctaveDef
#define PEDAL_ACR 105
#define PEDAL_ACG 67
#define PEDAL_ACB 67
#define PEDAL_ARCR 235
#define PEDAL_ARCG 225
#define PEDAL_ARCB 210
#define PEDAL_W 360
#define PEDAL_H 440
// index order: Mix, Tone -> left / right (matches the RS art)
#define PEDAL_KNOBS { {0.30f,0.18f,0.10f}, {0.70f,0.18f,0.10f} }
#include "../_shared/pedal_ui.hpp"
