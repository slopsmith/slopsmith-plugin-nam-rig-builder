/* TapeEcho rack UI — shared rack_ui template. Five knobs on a green-grey panel. */
#include "TapeEchoParams.h"
#define RACK_COUNT   kParamCount
#define RACK_TITLE   "TAPE ECHO"
#define RACK_NAMES   kTapeEchoNames
#define RACK_DEFS    kTapeEchoDef
#define RACK_ACR 135
#define RACK_ACG 170
#define RACK_ACB 130
#define RACK_KNOBS { {0.14f,0.50f,0.026f}, {0.23f,0.50f,0.026f}, {0.32f,0.50f,0.026f}, {0.41f,0.50f,0.026f}, {0.50f,0.50f,0.026f} }
#include "../_shared/rack_ui.hpp"
