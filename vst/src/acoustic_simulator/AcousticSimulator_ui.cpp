/* AcousticSimulator stompbox UI — shared pedal_ui template. */
#include "AcousticSimulatorParams.h"
#define PEDAL_TITLE  "ACOUSTIC"
#define PEDAL_NAMES  kAcousticSimulatorNames
#define PEDAL_DEFS   kAcousticSimulatorDef
#define PEDAL_ACR 190
#define PEDAL_ACG 150
#define PEDAL_ACB 90
#define PEDAL_KNOBS { {0.31f,0.16f,0.10f}, {0.69f,0.16f,0.10f}, {0.31f,0.32f,0.10f}, {0.69f,0.32f,0.10f} }
#include "../_shared/pedal_ui.hpp"
