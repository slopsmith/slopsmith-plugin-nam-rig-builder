/* LoFiFilter stompbox UI — shared pedal_ui template. */
#include "LoFiFilterParams.h"
#define PEDAL_TITLE  "LO-FI FILTER"
#define PEDAL_NAMES  kLoFiFilterNames
#define PEDAL_DEFS   kLoFiFilterDef
#define PEDAL_ACR 120
#define PEDAL_ACG 135
#define PEDAL_ACB 155
#define PEDAL_KNOBS { {0.31f,0.20f,0.13f}, {0.69f,0.20f,0.13f} }
#include "../_shared/pedal_ui.hpp"
