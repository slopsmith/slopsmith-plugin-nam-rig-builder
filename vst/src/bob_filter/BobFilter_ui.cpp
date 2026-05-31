/* BobFilter stompbox UI — shared pedal_ui template. */
#include "BobFilterParams.h"
#define PEDAL_TITLE  "BOB FILTER"
#define PEDAL_NAMES  kBobFilterNames
#define PEDAL_DEFS   kBobFilterDef
#define PEDAL_ACR 210
#define PEDAL_ACG 130
#define PEDAL_ACB 60
#define PEDAL_KNOBS { {0.24f,0.16f,0.10f}, {0.50f,0.16f,0.10f}, {0.76f,0.16f,0.10f}, {0.36f,0.31f,0.09f}, {0.64f,0.31f,0.09f} }
#include "../_shared/pedal_ui.hpp"
