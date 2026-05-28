/************************************************************************************

Content		:	Handles common VR helper functionality and owns the global VR state.
Ported/adapted from RealRTCWXR (code/RealRTCWXR/RealRTCWXR/VrInputCommon.c)
for the ioEF Elite Force VR port (milestone M1).

This translation unit DEFINES the engine-side global `vr_client_info_t vr;`
(under -DEFXR_CLIENT, which the Makefile applies to client-binary objects only).
Math helpers are kept; the RealRTCW controller-button / touchscreen / movement
input helpers are dropped for M1 (the full input layer is a later milestone).

Created		:	September 2019
Authors		:	Simon Brown

*************************************************************************************/

#include "VrCommon.h"

#include "../qcommon/qcommon.h"
#include "../qcommon/q_shared.h"

long long global_time;
int ducked;

vr_client_info_t vr;

extern ovrApp gAppState;

/* ----------------------------------------------------------------------------
   Controller state globals (filled by OpenXrInput.c TBXR_UpdateControllers,
   consumed by EFXR_SurfaceView.c VR_HandleControllerInput).  Ported from
   RealRTCWXR VrInputCommon.c.
   ---------------------------------------------------------------------------- */
ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTrackedController        leftRemoteTracking_new;
ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTrackedController        rightRemoteTracking_new;

/* Per-frame movement/turn outputs that the engine (cl_input.c) reads via the
   VR_GetControllerMove / VR_GetTurnDelta getters in EFXR_SurfaceView.c. */
float remote_movementSideways;
float remote_movementForward;


void rotateAboutOrigin(float x, float y, float rotation, vec2_t out)
{
    out[0] = cosf(DEG2RAD(-rotation)) * x  +  sinf(DEG2RAD(-rotation)) * y;
    out[1] = cosf(DEG2RAD(-rotation)) * y  -  sinf(DEG2RAD(-rotation)) * x;
}

float length(float x, float y)
{
    return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

#define NLF_DEADZONE 0.1
#define NLF_POWER 2.2

float nonLinearFilter(float in)
{
    float val = 0.0f;
    if (in > NLF_DEADZONE)
    {
        val = in > 1.0f ? 1.0f : in;
        val -= NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = powf(val, NLF_POWER);
    }
    else if (in < -NLF_DEADZONE)
    {
        val = in < -1.0f ? -1.0f : in;
        val += NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = -powf(fabsf(val), NLF_POWER);
    }

    return val;
}

bool between(float min, float val, float max)
{
    return (min < val) && (val < max);
}
