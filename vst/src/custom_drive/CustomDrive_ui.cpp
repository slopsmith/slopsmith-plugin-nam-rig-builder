/* CustomDrive stompbox UI — shared pedal_ui template. */
#include "CustomDriveParams.h"
#define PEDAL_TITLE  "CUSTOM DRIVE"
#define PEDAL_NAMES  kCustomDriveNames
#define PEDAL_DEFS   kCustomDriveDef
#define PEDAL_ACR 200
#define PEDAL_ACG 180
#define PEDAL_ACB 120
#define PEDAL_KNOBS { {0.31f,0.17f,0.12f}, {0.69f,0.17f,0.12f}, {0.50f,0.32f,0.10f} }
#include "../_shared/pedal_ui.hpp"
