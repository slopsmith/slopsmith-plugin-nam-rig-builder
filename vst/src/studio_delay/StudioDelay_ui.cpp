/* StudioDelay rack UI — shared rack_ui template. Five knobs on a blue panel. */
#include "StudioDelayParams.h"
#define RACK_COUNT   kParamCount
#define RACK_TITLE   "STUDIO DELAY"
#define RACK_NAMES   kStudioDelayNames
#define RACK_DEFS    kStudioDelayDef
#define RACK_ACR 105
#define RACK_ACG 135
#define RACK_ACB 205
#define RACK_KNOBS { {0.14f,0.50f,0.026f}, {0.23f,0.50f,0.026f}, {0.32f,0.50f,0.026f}, {0.41f,0.50f,0.026f}, {0.50f,0.50f,0.026f} }
#include "../_shared/rack_ui.hpp"
