/*
===========================================================================
android_glimp.c -- GLimp backend for the standalone Android/Quest build.

Replaces code/sdl/sdl_glimp.c on Android.  There is NO window or GL-context
creation here: the EGL + OpenGL ES 3 context is created (and made current on the
VR render thread) by the OpenXR layer in TBXR_InitialiseOpenXR (called from
VR_PreRendererInit, before the renderer's R_Init).  GLimp_Init therefore just
ADOPTS the current context and fills glConfig.  The fixed-function GL1 calls the
renderer makes (qgl* -> gl*) are serviced by gl4es, which translates them to
GLES3 on this same context.

The per-eye stereo present is driven by the VR layer (TBXR_submitFrame); the
desktop window swap is suppressed here while a VR session is active (mirrors
sdl_glimp.c's GLimp_EndFrame), and there is no desktop mirror to present.
===========================================================================
*/

#include "tr_common.h"
// GL access goes through gl4es (via tr_common.h -> qgl.h -> <GL/gl.h>); do NOT
// also include <GLES3/gl3.h> here -- gl4es's <GL/glext.h> and the system GLES3
// headers define the PFNGL* typedefs incompatibly and collide.  <EGL/egl.h> is
// EGL-only (no GL typedefs) so it's safe, and gives us eglGetProcAddress.
#include <EGL/egl.h>

// The GL1 multitexture / compiled-vertex-array entry points the renderer reaches
// through these qgl* pointers are normally defined+assigned in sdl_glimp.c (which
// we don't build).  Define them here and bind them to gl4es via eglGetProcAddress.
// NOTE: gl4es is initialized once (initialize_gl4es) from the JNI onCreate in
// EFXR_AndroidGlue.c -- the RTCWQuest placement -- NOT here.

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);
void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

void GLimp_Init( void )
{
	int w, h;

	ri.Printf( PRINT_ALL, "GLimp_Init() [Android/EGL]\n" );

	// The render resolution is the per-eye size that VR_PreRendererInit forced
	// into r_customwidth/height (== the OpenXR swapchain size * vr_supersample).
	w = ri.Cvar_VariableIntegerValue( "r_customwidth" );
	h = ri.Cvar_VariableIntegerValue( "r_customheight" );
	if ( w <= 0 || h <= 0 )
	{
		w = 640;
		h = 480;
	}

	glConfig.vidWidth = w;
	glConfig.vidHeight = h;
	glConfig.windowAspect = (float)w / (float)h;
	glConfig.isFullscreen = qtrue;
	glConfig.displayFrequency = 0;

	glConfig.colorBits = 24;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;

	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
	glConfig.deviceSupportsGamma = qfalse;   // no hardware gamma in headset compositor

	// GL strings come from gl4es (which is bound to the current GLES3 context).
	Q_strncpyz( glConfig.vendor_string,   (char *)qglGetString( GL_VENDOR ),   sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *)qglGetString( GL_RENDERER ), sizeof( glConfig.renderer_string ) );
	if ( *glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n' )
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz( glConfig.version_string,    (char *)qglGetString( GL_VERSION ),    sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (char *)qglGetString( GL_EXTENSIONS ), sizeof( glConfig.extensions_string ) );

	// gl4es advertises the desktop-GL extensions the GL1 renderer probes; let the
	// renderer's own GLimp_InitExtensions (in the SDL build) equivalent be skipped
	// -- we conservatively disable texture compression here.
	glConfig.textureCompression = TC_NONE;
	glConfig.textureEnvAddAvailable = qtrue;

	// Bind the GL1 multitexture entry points to GL4ES's OWN functions, NOT via
	// the native eglGetProcAddress.  CRITICAL: gl4es tracks the active texture
	// unit internally.  The renderer selects a unit through these pointers and
	// then binds/uploads through gl4es (qgl* -> gl4es).  If the unit-select goes
	// to the NATIVE driver glActiveTexture (what eglGetProcAddress returns) while
	// the bind goes to gl4es, the two unit states desync -- the lightmap/diffuse
	// stages land on the wrong units and every surface samples a white/default
	// texture.  Taking the address of gl4es's glActiveTexture here (this TU links
	// gl4es via qgl.h) keeps unit-select and bind coherent.
	qglActiveTextureARB       = (void (APIENTRYP)(GLenum))                   glActiveTexture;
	qglClientActiveTextureARB = (void (APIENTRYP)(GLenum))                   glClientActiveTexture;
	qglMultiTexCoord2fARB     = (void (APIENTRYP)(GLenum, GLfloat, GLfloat)) glMultiTexCoord2f;
	// CVA (glLockArraysEXT) is an optional fast-path; the renderer guards NULL and
	// the log already reports "compiled vertex arrays: disabled".
	qglLockArraysEXT          = NULL;
	qglUnlockArraysEXT        = NULL;

	ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// No SDL window on Android; input comes from OpenXR (VR_GetController*).
	ri.IN_Init( NULL );
}

void GLimp_Shutdown( void )
{
	ri.IN_Shutdown();
	// The EGL context is owned by the VR layer (TBXR_LeaveVR), not here.
}

void GLimp_EndFrame( void )
{
#ifdef BUILD_VR
	// The XR compositor presents the eye swapchains; never swap here while a VR
	// session is active.  (On Android there is no desktop window to swap anyway.)
	if ( ri.VR_IsActive && ri.VR_IsActive() )
	{
		return;
	}
#endif
	// Flat fallback (vr_enable 0) has no on-screen surface on Android -- nothing
	// to present.
}

void GLimp_Minimize( void )
{
}

void GLimp_LogComment( char *comment )
{
}

void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	// No hardware gamma control on the headset compositor.
}

#ifdef BUILD_VR
// Wired to re.WIN_SwapWindow / re.WIN_GetDrawableSize in tr_init.c (GetRefAPI).
// On Android there is no desktop mirror, so the swap is a no-op and the drawable
// size is just the eye render resolution (only used by the no-op mirror resolve).
void GLimp_SwapWindow( void )
{
}

void GLimp_GetDrawableSize( int *w, int *h )
{
	*w = glConfig.vidWidth;
	*h = glConfig.vidHeight;
}
#endif
