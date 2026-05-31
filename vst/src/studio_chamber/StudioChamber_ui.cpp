/* StudioChamber rack UI — shared rack_ui template. Four knobs (Time/Tone/Depth/Mix) on
 * a tinted sub-panel sampled from the Rocksmith art. */
#include "../_shared/reverb_params.h"
#define RACK_COUNT   kParamCount
#define RACK_TITLE   "STUDIO CHAMBER"
#define RACK_NAMES   kReverbNames
#define RACK_DEFS    kReverbDef
#define RACK_ACR 140
#define RACK_ACG 175
#define RACK_ACB 200
#define RACK_KNOBS { {0.16f,0.50f,0.030f}, {0.27f,0.50f,0.030f}, {0.38f,0.50f,0.030f}, {0.49f,0.50f,0.030f} }
#include "../_shared/rack_ui.hpp"
