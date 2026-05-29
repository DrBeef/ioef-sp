/*
===========================================================================
snd_opensles.c -- OpenSL ES audio backend (SNDDMA_*) for the standalone
Android/Quest build, replacing code/sdl/sdl_snd.c.

Implements the ioquake3 software-mixer DMA interface (snd_local.h) over an
Android simple buffer queue: the engine mixer writes ahead into the ring buffer
dma.buffer, and a buffer-queue callback streams fixed-size chunks out of it,
advancing a play cursor that SNDDMA_GetDMAPos reports back.  Standard ioq3
Android pattern.
===========================================================================
*/

#include "../../client/snd_local.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <string.h>
#include <stdlib.h>

// 16-bit stereo @ 44100.  dma.samples counts interleaved (mono) samples and MUST
// be a power of two (the mixer masks with dma.samples-1).
#define SLS_SPEED        44100
#define SLS_CHANNELS     2
#define SLS_SAMPLEBITS   16
#define SLS_SAMPLES      (32 * 1024)              // interleaved samples in the ring
#define SLS_BYTES        (SLS_SAMPLES * (SLS_SAMPLEBITS / 8))
#define SLS_CHUNK_FRAMES 1024                     // frames enqueued per callback
#define SLS_CHUNK_SAMPLES (SLS_CHUNK_FRAMES * SLS_CHANNELS)
#define SLS_CHUNK_BYTES  (SLS_CHUNK_SAMPLES * (SLS_SAMPLEBITS / 8))

static SLObjectItf  engineObject = NULL;
static SLEngineItf  engineEngine = NULL;
static SLObjectItf  outputMixObject = NULL;
static SLObjectItf  playerObject = NULL;
static SLPlayItf    playerPlay = NULL;
static SLAndroidSimpleBufferQueueItf playerBufferQueue = NULL;

static volatile int s_playSample = 0;   // play cursor, in interleaved samples
static qboolean     s_running = qfalse;

// Called on an OpenSLES thread when a buffer finishes; enqueue the next chunk
// straight out of the engine's ring buffer and advance the play cursor.
static void SLS_BufferCallback( SLAndroidSimpleBufferQueueItf bq, void *context )
{
	int posBytes;

	if ( !s_running || dma.buffer == NULL )
	{
		return;
	}

	posBytes = ( s_playSample * (SLS_SAMPLEBITS / 8) ) & ( SLS_BYTES - 1 );

	// The ring is sized so a chunk never straddles the wrap point
	// (SLS_BYTES and SLS_CHUNK_BYTES are both powers of two, SLS_BYTES multiple).
	(*bq)->Enqueue( bq, dma.buffer + posBytes, SLS_CHUNK_BYTES );

	s_playSample = ( s_playSample + SLS_CHUNK_SAMPLES ) & ( SLS_SAMPLES - 1 );
}

qboolean SNDDMA_Init( void )
{
	SLresult r;

	if ( s_running )
	{
		return qtrue;
	}

	r = slCreateEngine( &engineObject, 0, NULL, 0, NULL, NULL );
	if ( r != SL_RESULT_SUCCESS ) { return qfalse; }
	(*engineObject)->Realize( engineObject, SL_BOOLEAN_FALSE );
	(*engineObject)->GetInterface( engineObject, SL_IID_ENGINE, &engineEngine );

	(*engineEngine)->CreateOutputMix( engineEngine, &outputMixObject, 0, NULL, NULL );
	(*outputMixObject)->Realize( outputMixObject, SL_BOOLEAN_FALSE );

	SLDataLocator_AndroidSimpleBufferQueue locBufq =
		{ SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
	SLDataFormat_PCM formatPcm = {
		SL_DATAFORMAT_PCM,
		SLS_CHANNELS,
		SL_SAMPLINGRATE_44_1,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};
	SLDataSource audioSrc = { &locBufq, &formatPcm };

	SLDataLocator_OutputMix locOutmix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObject };
	SLDataSink audioSnk = { &locOutmix, NULL };

	const SLInterfaceID ids[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
	const SLboolean     req[1] = { SL_BOOLEAN_TRUE };
	r = (*engineEngine)->CreateAudioPlayer( engineEngine, &playerObject,
			&audioSrc, &audioSnk, 1, ids, req );
	if ( r != SL_RESULT_SUCCESS ) { SNDDMA_Shutdown(); return qfalse; }
	(*playerObject)->Realize( playerObject, SL_BOOLEAN_FALSE );
	(*playerObject)->GetInterface( playerObject, SL_IID_PLAY, &playerPlay );
	(*playerObject)->GetInterface( playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &playerBufferQueue );
	(*playerBufferQueue)->RegisterCallback( playerBufferQueue, SLS_BufferCallback, NULL );

	// Fill the engine's DMA description and ring buffer.
	Com_Memset( &dma, 0, sizeof( dma ) );
	dma.channels         = SLS_CHANNELS;
	dma.samples          = SLS_SAMPLES;
	dma.submission_chunk = SLS_CHUNK_SAMPLES;
	dma.samplebits       = SLS_SAMPLEBITS;
	dma.speed            = SLS_SPEED;
	dma.buffer           = (byte *)calloc( 1, SLS_BYTES );
	if ( dma.buffer == NULL ) { SNDDMA_Shutdown(); return qfalse; }

	s_playSample = 0;
	s_running = qtrue;

	(*playerPlay)->SetPlayState( playerPlay, SL_PLAYSTATE_PLAYING );
	// Prime the queue (two buffers) to kick the callback chain.
	SLS_BufferCallback( playerBufferQueue, NULL );
	SLS_BufferCallback( playerBufferQueue, NULL );

	Com_Printf( "OpenSL ES audio: %d Hz, %d-bit, %d channels\n",
			dma.speed, dma.samplebits, dma.channels );
	return qtrue;
}

int SNDDMA_GetDMAPos( void )
{
	return s_playSample;
}

void SNDDMA_Shutdown( void )
{
	s_running = qfalse;

	if ( playerPlay )
	{
		(*playerPlay)->SetPlayState( playerPlay, SL_PLAYSTATE_STOPPED );
	}
	if ( playerObject )
	{
		(*playerObject)->Destroy( playerObject );
		playerObject = NULL; playerPlay = NULL; playerBufferQueue = NULL;
	}
	if ( outputMixObject )
	{
		(*outputMixObject)->Destroy( outputMixObject );
		outputMixObject = NULL;
	}
	if ( engineObject )
	{
		(*engineObject)->Destroy( engineObject );
		engineObject = NULL; engineEngine = NULL;
	}
	if ( dma.buffer )
	{
		free( dma.buffer );
		dma.buffer = NULL;
	}
}

// Ring-buffer model: the mixer writes directly into dma.buffer, so there is no
// separate lock/submit step.
void SNDDMA_BeginPainting( void ) {}
void SNDDMA_Submit( void ) {}

// snd_main.c's S_Init calls S_AL_Init unconditionally (s_useOpenAL defaults 1)
// and snd_openal.c/qal.c are not built on Android (no OpenAL).  Return qfalse so
// S_Init falls back to the DMA mixer driven by the OpenSL ES backend above.
qboolean S_AL_Init( soundInterface_t *si )
{
	(void)si;
	return qfalse;
}
