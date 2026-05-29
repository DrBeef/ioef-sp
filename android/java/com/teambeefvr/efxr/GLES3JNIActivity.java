package com.teambeefvr.efxr;

import static android.system.Os.setenv;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

// VR activity: owns the SurfaceView, loads the vendor OpenXR loader + the engine,
// and forwards the Android lifecycle to native code (which runs the engine on a
// dedicated render thread).  Modeled on JKXR's GLES3JNIActivity (haptics service
// integration omitted for now).
@SuppressLint("SdCardPath")
public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{
	private static final String TAG = "EFXR";
	private static final String COMMANDLINE = "/sdcard/EFXR/commandline.txt";

	// Loaded as early as possible so JNI_OnLoad runs first.
	static
	{
		// Single standard Khronos OpenXR loader (libopenxr_loader.so).  On
		// Android it resolves the active runtime via the system OpenXR broker,
		// so it works on Quest and Pico alike -- no per-vendor loader needed
		// (matches OpenJKDF2).  Loaded BEFORE the engine so libefxr.so's
		// openxr_loader dependency is already satisfied.
		System.loadLibrary("openxr_loader");
		System.loadLibrary("efxr");
	}

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;
	private String commandLineParams = "";

	@Override
	protected void onCreate(Bundle icicle)
	{
		Log.v(TAG, "GLES3JNIActivity::onCreate()");
		super.onCreate(icicle);

		mView = new SurfaceView(this);
		setContentView(mView);
		mView.getHolder().addCallback(this);

		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes(params);

		// Tell the engine where the SP game/UI .so live (nativeLibraryDir).
		try {
			setenv("EF_GAMELIBDIR", getApplicationInfo().nativeLibraryDir, true);
		} catch (Exception e) {
			Log.w(TAG, "setenv EF_GAMELIBDIR failed: " + e);
		}

		commandLineParams = readCommandLine();
		mNativeHandle = GLES3JNILib.onCreate(this, commandLineParams);
	}

	private String readCommandLine()
	{
		// Default to the EF1 first level if no commandline.txt is present.
		String params = "+set fs_basepath /sdcard/EFXR +set fs_game baseEF +set com_basegame baseEF";
		File f = new File(COMMANDLINE);
		if (f.exists()) {
			try (BufferedReader br = new BufferedReader(new FileReader(f))) {
				StringBuilder sb = new StringBuilder();
				String s;
				while ((s = br.readLine()) != null) sb.append(s).append(" ");
				params = sb.toString();
			} catch (Exception e) {
				Log.w(TAG, "Failed to read " + COMMANDLINE + ": " + e);
			}
		}
		return params;
	}

	@Override protected void onStart()
	{
		super.onStart();
		if (mNativeHandle != 0) GLES3JNILib.onStart(mNativeHandle, this);
	}

	@Override protected void onResume()
	{
		super.onResume();
		if (mNativeHandle != 0) GLES3JNILib.onResume(mNativeHandle);
	}

	@Override protected void onPause()
	{
		if (mNativeHandle != 0) GLES3JNILib.onPause(mNativeHandle);
		super.onPause();
	}

	@Override protected void onStop()
	{
		if (mNativeHandle != 0) GLES3JNILib.onStop(mNativeHandle);
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		if (mSurfaceHolder != null) GLES3JNILib.onSurfaceDestroyed(mNativeHandle);
		if (mNativeHandle != 0) GLES3JNILib.onDestroy(mNativeHandle);
		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated(SurfaceHolder holder)
	{
		if (mNativeHandle != 0) {
			GLES3JNILib.onSurfaceCreated(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		if (mNativeHandle != 0) {
			GLES3JNILib.onSurfaceChanged(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceDestroyed(SurfaceHolder holder)
	{
		if (mNativeHandle != 0) {
			GLES3JNILib.onSurfaceDestroyed(mNativeHandle);
			mSurfaceHolder = null;
		}
	}
}
