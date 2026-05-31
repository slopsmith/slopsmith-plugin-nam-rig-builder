/* FuzzWasHe stompbox UI — shared pedal_ui template. */
#include "FuzzWasHeParams.h"
#define PEDAL_TITLE  "FUZZY WAS HE"
#define PEDAL_NAMES  kFuzzWasHeNames
#define PEDAL_DEFS   kFuzzWasHeDef
#define PEDAL_ACR 150
#define PEDAL_ACG 80
#define PEDAL_ACB 170
#define PEDAL_KNOBS { {0.31f,0.20f,0.13f}, {0.69f,0.20f,0.13f} }
#include "../_shared/pedal_ui.hpp"
