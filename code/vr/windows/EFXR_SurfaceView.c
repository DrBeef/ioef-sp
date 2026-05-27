/*
===========================================================================
EFXR_SurfaceView.c -- game-specific OpenXR glue for the ioEF VR port.

Ported/adapted from RealRTCWXR (code/RealRTCWXR/RealRTCWXR/windows/
RTCWXR_SurfaceView.c) for the ioEF Elite Force VR port (milestone M1).

Holds the engine-touching glue that TBXR_Common.c deliberately avoids:
the virtual-screen-layer decision (reads clc/Key_GetCatcher), HMD pose
plumbing into the shared `vr` struct, the per-eye VR projection, per-frame
setup, the screen-resolution / swap helpers, and (stubbed for M1) the
controller-input and haptics entry points.

RTCW-specific gameplay (saber, vehicle, binoculars, emplaced gun, remote
turret, zoom-mode) and any vr_client_info_t fields RealRTCW had but the
ioEF VrClientInfo.h contract omits are dropped -- see report for the list.
None of this is wired into the engine yet (M1 is dead code).
===========================================================================
*/

#include "../VrCommon.h"
#include "../VrCvars.h"

#include "../../client/client.h"


/*
================================================================================

Virtual screen layer / cinematics decision

================================================================================
*/

bool VR_UseScreenLayer()
{
	static int frame = 0;
	vr.using_screen_layer =
			(frame++ < 100) || //use screen for first 100 frames - stops splash screen giving a headache
			(bool)((vr.cin_camera && !vr.immersive_cinematics) ||
			vr.misc_camera ||
			clc.demoplaying ||
			(clc.state == CA_DISCONNECTED) ||
			(clc.state == CA_CHALLENGING) ||
			(clc.state == CA_CONNECTING) ||
			(clc.state == CA_CINEMATIC) ||
			(clc.state == CA_LOADING) ||
			(clc.state == CA_PRIMED) ||
			( Key_GetCatcher( ) & KEYCATCH_UI ) ||
			( Key_GetCatcher( ) & KEYCATCH_CONSOLE ));

	return vr.using_screen_layer;
}

float VR_GetScreenLayerDistance()
{
	return (2.0f + vr_screen_dist->value);
}


/*
================================================================================

HMD pose plumbing

================================================================================
*/

void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	//Orientation
	VectorSet(vr.hmdorientation, pitch, yaw, roll);
	VectorSubtract(vr.hmdorientation_last, vr.hmdorientation, vr.hmdorientation_delta);

	//Keep this for our records
	VectorCopy(vr.hmdorientation, vr.hmdorientation_last);

	if (!vr.third_person)
	{
		VectorCopy(vr.hmdorientation, vr.hmdorientation_first);
	}

	VectorCopy(vr.weaponangles[ANGLES_ADJUSTED], vr.weaponangles_first[ANGLES_ADJUSTED]);

	// View yaw delta
	float clientview_yaw = vr.clientviewangles[YAW] - vr.hmdorientation[YAW];
	vr.clientview_yaw_delta = vr.clientview_yaw_last - clientview_yaw;
	vr.clientview_yaw_last = clientview_yaw;

	// Max-height is set only once on start, or after re-calibration
	// (ignore too low value which is sometimes provided on start)
	if (!vr.maxHeight || vr.maxHeight < 1.0) {
		vr.maxHeight = vr.hmdposition[1];
	}

	vr.curHeight = vr.hmdposition[1];
}

void VR_SetHMDPosition(float x, float y, float z )
{
	static bool s_useScreen = qfalse;
	static int frame = 0;

	VectorSet(vr.hmdposition, x, y, z);

	//Can be set elsewhere
	vr.take_snap |= (s_useScreen != VR_UseScreenLayer());
	if (vr.take_snap || (frame++ < 100))
	{
		s_useScreen = VR_UseScreenLayer();

		//Record player position on transition
		VectorSet(vr.hmdposition_snap, x, y, z);
		VectorCopy(vr.hmdorientation, vr.hmdorientation_snap);
		if (vr.cin_camera)
		{
			//Reset snap turn too if in a cinematic
			vr.snapTurn = 0;
		}
		vr.take_snap = false;
	}

	VectorSubtract(vr.hmdposition, vr.hmdposition_snap, vr.hmdposition_offset);

	//Position
	VectorSubtract(vr.hmdposition_last, vr.hmdposition, vr.hmdposition_delta);

	//Keep this for our records
	VectorCopy(vr.hmdposition, vr.hmdposition_last);
}


/*
================================================================================

VR projection / per-frame setup

================================================================================
*/

qboolean VR_GetVRProjection(float zNear, float zFar, float gameFovX, float gameFovY, float* projection)
{
	//Don't use our projection if playing a cinematic and we are not immersive
	if (vr.cin_camera && !vr.immersive_cinematics)
	{
		return qfalse;
	}

	for (int eye = 0; eye < 2; ++eye)
	{
		XrFovf fov = gAppState.Views[eye].fov;

		//Just use X for zoom level for now.. something off with Y on Quest 3
		float zZoom = (vr.fov_x / gameFovX);

		fov.angleLeft = atanf((tanf(fov.angleLeft) / zZoom));
		fov.angleRight = atanf((tanf(fov.angleRight) / zZoom));
		fov.angleUp = atanf((tanf(fov.angleUp) / zZoom));
		fov.angleDown = atanf((tanf(fov.angleDown) / zZoom));

		XrMatrix4x4f_CreateProjectionFov(
			(XrMatrix4x4f*)(projection+(eye*16)), GRAPHICS_OPENGL,
			fov, zNear, zFar);
	}

	return qtrue;
}

int VR_SetRefreshRate(int refreshRate)
{
	return 0;
}

//All the stuff we want to do each frame specifically for this game
void VR_FrameSetup()
{
	static float refresh = 0;
	if (refresh != vr_refresh->value)
	{
		refresh = vr_refresh->value;
		VR_SetRefreshRate(vr_refresh->value);
	}

	//get any cvar values required here
	vr.immersive_cinematics = (vr_immersive_cinematics->value != 0.0f);
}


/*
================================================================================

Engine glue used by TBXR_Common.c (keeps that file engine-agnostic)

================================================================================
*/

void EFXR_GetScreenResolution(int *width, int *height)
{
	*width = cls.glconfig.vidWidth;
	*height = cls.glconfig.vidHeight;
}

void EFXR_SwapWindow()
{
	// Wired into the GL backend in a later milestone; no-op for M1.
}


/*
================================================================================

VR lifecycle

================================================================================
*/

void VR_Shutdown()
{
	TBXR_LeaveVR();
}

void VR_Init()
{
	GlInitExtensions();

	//First - all the OpenXR stuff and nonsense
	TBXR_InitialiseOpenXR();
	TBXR_EnterVR();
	TBXR_InitRenderer();
	TBXR_InitActions();
	TBXR_WaitForSessionActive();

	//Initialise all our variables
	vr.snapTurn = 0.0f;
	vr.immersive_cinematics = qtrue;
	vr.move_speed = 1; // Default to full speed now

	//init randomiser
	srand(time(NULL));

	//Create Cvars
	VR_InitCvars();

	vr.menu_right_handed = vr_control_scheme->integer == 0;
}


/*
================================================================================

Haptics / controller input -- stubbed for M1 (full input layer is a later
milestone).  Keeping the symbols here so TBXR_Common.c links.

================================================================================
*/

void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
}

void VR_HapticUpdateEvent(const char* event, int intensity, float angle )
{
}

void VR_HapticEndFrame()
{
}

void VR_HapticStopEvent(const char* event)
{
}

void VR_HapticEnable()
{
}

void VR_HapticDisable()
{
}

void VR_HandleControllerInput()
{
	// M1: controller input layer not yet ported.
}
