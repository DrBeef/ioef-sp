/*
===========================================================================
cl_cin_bink.c -- Bink video playback integration for EF1 singleplayer

Dynamically loads binkw32.dll (from the original EF game) and uses the
Bink 1 API to decode .bik video files.  Decoded frames are fed into the
engine's existing re.DrawStretchRaw() for display.

The Bink DLL is loaded on first use and stays loaded for the session.
If binkw32.dll is not found, .bik playback silently falls back to
showing a black screen (the game continues normally).
===========================================================================
*/

#ifdef ELITEFORCE

#include "client.h"
#include <windows.h>

/* ---- Bink types (subset of bink.h, avoids SDK dependency) ---- */

typedef unsigned int   BU32;
typedef signed int     BS32;

typedef struct BINK {
	BU32 Width;
	BU32 Height;
	BU32 Frames;
	BU32 FrameNum;
	BU32 LastFrameNum;
	BU32 FrameRate;
	BU32 FrameRateDiv;
	/* ... more fields follow but we don't need them */
} *HBINK;

#define BINKSURFACE32          3   /* ARGB 32-bit */
#define BINKSURFACE32R         4   /* RGBA 32-bit (what we want for GL) */
#define BINKSURFACE24R         2   /* RGB 24-bit reversed */
#define BINKSNDTRACK           0x00004000L
#define BINKNOFRAMEBUFFERS     0x00000200L
#define BINKNOSOUND            0x00000020L

/* ---- Function pointer types ---- */
typedef HBINK   (__stdcall *pfn_BinkOpen)(const char *name, BU32 flags);
typedef void    (__stdcall *pfn_BinkClose)(HBINK bnk);
typedef BS32    (__stdcall *pfn_BinkDoFrame)(HBINK bnk);
typedef void    (__stdcall *pfn_BinkNextFrame)(HBINK bnk);
typedef BS32    (__stdcall *pfn_BinkWait)(HBINK bnk);
typedef BS32    (__stdcall *pfn_BinkCopyToBuffer)(HBINK bnk, void *dest, BS32 destpitch,
                    BU32 destheight, BU32 destx, BU32 desty, BU32 flags);
typedef BS32    (__stdcall *pfn_BinkSetSoundOnOff)(HBINK bnk, BS32 onoff);
typedef void    (__stdcall *pfn_BinkSetVolume)(HBINK bnk, BU32 trackid, BS32 volume);
typedef BS32    (__stdcall *pfn_BinkSetSoundSystem)(void *open, BU32 param);

/* ---- State ---- */
static HMODULE      binkLib = NULL;
static qboolean     binkLoadAttempted = qfalse;
static pfn_BinkOpen             pBinkOpen;
static pfn_BinkClose            pBinkClose;
static pfn_BinkDoFrame          pBinkDoFrame;
static pfn_BinkNextFrame        pBinkNextFrame;
static pfn_BinkWait             pBinkWait;
static pfn_BinkCopyToBuffer     pBinkCopyToBuffer;
static pfn_BinkSetSoundOnOff    pBinkSetSoundOnOff;

/* Active playback state */
static HBINK        binkHandle = NULL;
static byte         *binkBuffer = NULL;     /* decoded frame (video dimensions) */
static byte         *binkPaddedBuf = NULL;  /* padded to power-of-2 for renderer */
static int          binkWidth, binkHeight;
static int          binkPadW, binkPadH;     /* power-of-2 padded dimensions */
static int          binkX, binkY, binkW, binkH;  /* display rect */
static qboolean     binkLooping;
static qboolean     binkPlaying;
static int          binkStartTime;

static int nextPow2( int v ) {
	int p = 1;
	while ( p < v ) p <<= 1;
	return p;
}

/* ---- DLL loading ---- */

static qboolean CIN_Bink_LoadDLL( void ) {
	if ( binkLoadAttempted ) {
		return binkLib != NULL;
	}
	binkLoadAttempted = qtrue;

	binkLib = LoadLibraryA( "binkw32.dll" );
	if ( !binkLib ) {
		Com_Printf( "Bink: binkw32.dll not found, .bik playback disabled\n" );
		return qfalse;
	}

	pBinkOpen          = (pfn_BinkOpen)GetProcAddress( binkLib, "_BinkOpen@8" );
	pBinkClose         = (pfn_BinkClose)GetProcAddress( binkLib, "_BinkClose@4" );
	pBinkDoFrame       = (pfn_BinkDoFrame)GetProcAddress( binkLib, "_BinkDoFrame@4" );
	pBinkNextFrame     = (pfn_BinkNextFrame)GetProcAddress( binkLib, "_BinkNextFrame@4" );
	pBinkWait          = (pfn_BinkWait)GetProcAddress( binkLib, "_BinkWait@4" );
	pBinkCopyToBuffer  = (pfn_BinkCopyToBuffer)GetProcAddress( binkLib, "_BinkCopyToBuffer@28" );
	pBinkSetSoundOnOff = (pfn_BinkSetSoundOnOff)GetProcAddress( binkLib, "_BinkSetSoundOnOff@8" );

	if ( !pBinkOpen || !pBinkClose || !pBinkDoFrame || !pBinkNextFrame ||
			!pBinkWait || !pBinkCopyToBuffer ) {
		Com_Printf( "Bink: stdcall names failed, trying cdecl...\n" );
		/* Try undecorated names (some Bink versions use cdecl) */
		pBinkOpen          = (pfn_BinkOpen)GetProcAddress( binkLib, "BinkOpen" );
		pBinkClose         = (pfn_BinkClose)GetProcAddress( binkLib, "BinkClose" );
		pBinkDoFrame       = (pfn_BinkDoFrame)GetProcAddress( binkLib, "BinkDoFrame" );
		pBinkNextFrame     = (pfn_BinkNextFrame)GetProcAddress( binkLib, "BinkNextFrame" );
		pBinkWait          = (pfn_BinkWait)GetProcAddress( binkLib, "BinkWait" );
		pBinkCopyToBuffer  = (pfn_BinkCopyToBuffer)GetProcAddress( binkLib, "BinkCopyToBuffer" );
		pBinkSetSoundOnOff = (pfn_BinkSetSoundOnOff)GetProcAddress( binkLib, "BinkSetSoundOnOff" );
	}

	if ( !pBinkOpen || !pBinkClose || !pBinkDoFrame || !pBinkNextFrame ||
			!pBinkWait || !pBinkCopyToBuffer ) {
		Com_Printf( "Bink: failed to resolve API functions from binkw32.dll\n" );
		FreeLibrary( binkLib );
		binkLib = NULL;
		return qfalse;
	}

	Com_Printf( "Bink: binkw32.dll loaded successfully\n" );
	return qtrue;
}

/* ---- Forward declarations ---- */
void CIN_Bink_Close( void );

/* ---- Public interface ---- */

qboolean CIN_Bink_IsBikFile( const char *filename ) {
	int len = strlen( filename );
	if ( len >= 4 && !Q_stricmp( filename + len - 4, ".bik" ) ) {
		return qtrue;
	}
	return qfalse;
}

int CIN_Bink_Open( const char *filename, int x, int y, int w, int h, int systemBits ) {
	char fullpath[MAX_OSPATH];
	char *basepath;

	if ( !CIN_Bink_LoadDLL() ) {
		return -1;
	}

	/* Close any existing playback */
	CIN_Bink_Close();

	/* Build full filesystem path - Bink needs a real file path, not a pak path.
	   Extract the file from the pak to a temp location if needed. */
	basepath = Cvar_VariableString( "fs_basepath" );
	Com_sprintf( fullpath, sizeof(fullpath), "%s/baseEF/%s", basepath, filename );

	/* Try to open directly first (might be loose file) */
	binkHandle = pBinkOpen( fullpath, BINKNOSOUND );
	if ( !binkHandle ) {
		/* Try with video/ prefix */
		Com_sprintf( fullpath, sizeof(fullpath), "%s/baseEF/video/%s", basepath, filename );
		binkHandle = pBinkOpen( fullpath, BINKNOSOUND );
	}
	if ( !binkHandle ) {
		Com_Printf( "Bink: failed to open '%s'\n", filename );
		return -1;
	}

	binkWidth = binkHandle->Width;
	binkHeight = binkHandle->Height;
	binkPadW = nextPow2( binkWidth );
	binkPadH = nextPow2( binkHeight );
	binkX = x;
	binkY = y;
	binkW = w ? w : cls.glconfig.vidWidth;
	binkH = h ? h : cls.glconfig.vidHeight;
	binkLooping = ( systemBits & CIN_loop ) ? qtrue : qfalse;
	binkPlaying = qtrue;
	binkStartTime = cls.realtime;

	/* Allocate RGBA buffers - padded to power-of-2 for the renderer */
	binkBuffer = (byte *)Z_Malloc( binkPadW * binkPadH * 4 );
	Com_Memset( binkBuffer, 0, binkPadW * binkPadH * 4 );

	Com_Printf( "Bink: playing '%s' (%dx%d, %d frames)\n",
		filename, binkWidth, binkHeight, binkHandle->Frames );

	/* Set client state to cinematic if system bit set */
	if ( systemBits & CIN_system ) {
		clc.state = CA_CINEMATIC;
	}

	return 0;
}

e_status CIN_Bink_RunFrame( void ) {
	if ( !binkHandle || !binkPlaying ) {
		return FMV_EOF;
	}

	/* Check if at end */
	if ( binkHandle->FrameNum >= binkHandle->Frames ) {
		if ( binkLooping ) {
			/* TODO: seek to start */
		} else {
			binkPlaying = qfalse;
			return FMV_EOF;
		}
	}

	/* Wait returns 0 when it's time to decompress a frame */
	while ( pBinkWait( binkHandle ) ) {
		/* Not time yet - just return and show current frame */
		return FMV_PLAY;
	}

	/* Decompress the current frame */
	pBinkDoFrame( binkHandle );

	/* Copy decoded frame to our RGBA buffer (padded to power-of-2).
	   Use padded pitch so rows align to the power-of-2 texture width. */
	pBinkCopyToBuffer( binkHandle, binkBuffer,
		binkPadW * 4, binkPadH, 0, 0, BINKSURFACE32 );

	/* Advance to next frame */
	pBinkNextFrame( binkHandle );

	return FMV_PLAY;
}

void CIN_Bink_Draw( void ) {
	if ( !binkHandle || !binkBuffer ) {
		return;
	}

	/* Swap BGRA→RGBA in the buffer since RE_StretchRaw uses GL_RGBA.
	   Bink's BINKSURFACE32 outputs BGRA on little-endian x86. */
	{
		int i, total = binkPadW * binkPadH;
		byte *p = binkBuffer;
		for ( i = 0; i < total; i++, p += 4 ) {
			byte tmp = p[0];
			p[0] = p[2];
			p[2] = tmp;
		}
	}

	/* Upload decoded frame to renderer as power-of-2 texture.
	   Scale the draw rect taller to compensate for the padding at the
	   bottom, so only the actual video content is visible in the
	   original binkW x binkH area. */
	{
		int scaledH = binkH * binkPadH / binkHeight;
		re.DrawStretchRaw( binkX, binkY, binkW, scaledH,
			binkPadW, binkPadH, binkBuffer, 0, qtrue );
	}
}

void CIN_Bink_Close( void ) {
	if ( binkHandle ) {
		pBinkClose( binkHandle );
		binkHandle = NULL;
	}
	if ( binkBuffer ) {
		Z_Free( binkBuffer );
		binkBuffer = NULL;
	}
	binkPaddedBuf = NULL;
	binkPlaying = qfalse;
}

qboolean CIN_Bink_IsPlaying( void ) {
	return binkPlaying;
}

#endif /* ELITEFORCE */
