/*
===========================================================================
VrCommon.h -- shared VR helper declarations for the ioEF OpenXR VR port.

Ported/adapted from RealRTCWXR (code/RealRTCWXR/RealRTCWXR/VrCommon.h) for
the ioEF Elite Force VR port (milestone M1).  Include paths fixed for this
tree (../qcommon/...).  Controller/touchscreen input helpers from RealRTCWXR
are dropped for M1 (the full input layer is a later milestone); only the math
helpers actually defined in VrInputCommon.c are declared here.
===========================================================================
*/
#if !defined(vrcommon_h)
#define vrcommon_h

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include "VrClientInfo.h"

#ifdef _WIN32
#include "windows/TBXR_Common.h"
#endif


extern long long global_time;
extern int ducked;

float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

#endif //vrcommon_h
