/* StudioChorus rack UI — shared rack_ui template. RCE-10 Chorus Ensemble:
 * 7 knobs (Rate/Depth/Mix/LoFilter/HiFilter/Stereo/Delay) on a cool-blue
 * sub-panel in two rows, green LCD nameplate. */
#include "StudioChorusParams.h"
#define RACK_COUNT   kParamCount
#define RACK_TITLE   "STUDIO CHORUS"
#define RACK_NAMES   kStudioChorusNames
#define RACK_DEFS    kStudioChorusDef
#define RACK_ACR 120
#define RACK_ACG 165
#define RACK_ACB 205
// enum order: Rate, Depth, Mix, LoFilter | HiFilter, Stereo, Delay
#define RACK_KNOBS { \
    {0.155f,0.38f,0.024f}, {0.265f,0.38f,0.024f}, {0.375f,0.38f,0.024f}, {0.485f,0.38f,0.024f}, \
    {0.210f,0.72f,0.024f}, {0.320f,0.72f,0.024f}, {0.430f,0.72f,0.024f} }
#include "../_shared/rack_ui.hpp"
