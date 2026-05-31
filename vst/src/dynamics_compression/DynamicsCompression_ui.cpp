/* DynamicsCompression stompbox UI — shared pedal_ui template. */
#include "DynamicsCompressionParams.h"
#define PEDAL_TITLE  "COMPRESSION"
#define PEDAL_NAMES  kDynamicsCompressionNames
#define PEDAL_DEFS   kDynamicsCompressionDef
#define PEDAL_ACR 70
#define PEDAL_ACG 150
#define PEDAL_ACB 185
#define PEDAL_KNOBS { {0.25f,0.20f,0.11f}, {0.50f,0.20f,0.11f}, {0.75f,0.20f,0.11f} }
#include "../_shared/pedal_ui.hpp"
