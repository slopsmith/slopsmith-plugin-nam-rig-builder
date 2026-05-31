/* BassMultiComp stompbox UI — shared pedal_ui template. */
#include "BassMultiCompParams.h"
#define PEDAL_TITLE  "MB COMP"
#define PEDAL_NAMES  kBassMultiCompNames
#define PEDAL_DEFS   kBassMultiCompDef
#define PEDAL_ACR 150
#define PEDAL_ACG 160
#define PEDAL_ACB 172
#define PEDAL_KNOBS { {0.28f,0.17f,0.13f}, {0.50f,0.31f,0.10f}, {0.72f,0.17f,0.13f} }
#include "../_shared/pedal_ui.hpp"
