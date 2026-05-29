package com.teambeefvr.efxr;

import android.app.Activity;
import android.view.Surface;

// Thin wrapper over the native engine (libefxr.so).  Implemented in
// code/vr/android/EFXR_AndroidGlue.c.
public class GLES3JNILib
{
	// Activity lifecycle
	public static native long onCreate( Activity obj, String commandLineParams );
	public static native void onStart( long handle, Object obj );
	public static native void onResume( long handle );
	public static native void onPause( long handle );
	public static native void onStop( long handle );
	public static native void onDestroy( long handle );

	// Surface lifecycle
	public static native void onSurfaceCreated( long handle, Surface s );
	public static native void onSurfaceChanged( long handle, Surface s );
	public static native void onSurfaceDestroyed( long handle );
}
