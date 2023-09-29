//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2023 Adam Kaminski
// Copyright (C) 2023 Zandronum Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Zandronum Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: voicechat.cpp
//
//-----------------------------------------------------------------------------

#include "voicechat.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "d_netinf.h"
#include "network.h"
#include "v_text.h"
#include "stats.h"
#include "p_acs.h"

//*****************************************************************************
//	CONSOLE VARIABLES

// [AK] Enables noise suppression while transmitting audio.
CVAR( Bool, voice_suppressnoise, true, CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )

// [AK] Allows the client to load a custom RNNoise model file.
CVAR( String, voice_noisemodelfile, "", CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )

// [AK] Which input device to use when recording audio.
CUSTOM_CVAR( Int, voice_recorddriver, 0, CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )
{
	VOIPController &instance = VOIPController::GetInstance( );

	// [AK] If currently recording from a device, stop and start over.
	if ( instance.IsRecording( ))
	{
		instance.StopRecording( );
		instance.StartRecording( );
	}
}

// [AK] How sensitive voice activity detection is, in decibels.
CUSTOM_CVAR( Float, voice_recordsensitivity, -50.0f, CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )
{
	const float clampedValue = clamp<float>( self, -100.0f, 0.0f );

	if ( self != clampedValue )
		self = clampedValue;
}

// [AK] Controls the volume of the input device.
CUSTOM_CVAR( Float, voice_recordvolume, 1.0f, CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )
{
	const float clampedValue = clamp<float>( self, 0.0f, 2.0f );

	if ( self != clampedValue )
		self = clampedValue;
}

// [AK] Controls the volume of everyone's voices on the client's end.
CUSTOM_CVAR( Float, voice_outputvolume, 1.0f, CVAR_ARCHIVE | CVAR_NOSETBYACS | CVAR_GLOBALCONFIG )
{
	const float clampedValue = clamp<float>( self, 0.0f, 2.0f );

	if ( self != clampedValue )
	{
		self = clampedValue;
		return;
	}

	VOIPController::GetInstance( ).SetVolume( self );
}

// [AK] How the voice chat is used on the server (0 = never, 1 = always, 2 = teammates only).
CUSTOM_CVAR( Int, sv_allowvoicechat, VOICECHAT_EVERYONE, CVAR_NOSETBYACS | CVAR_SERVERINFO )
{
	const int clampedValue = clamp<int>( self, VOICECHAT_OFF, VOICECHAT_TEAMMATESONLY );

	if ( self != clampedValue )
	{
		self = clampedValue;
		return;
	}

	// [AK] Notify the clients about the change.
	SERVER_SettingChanged( self, false );
}

// [AK] Enables or disables proximity-based voice chat.
CUSTOM_CVAR( Bool, sv_proximityvoicechat, false, CVAR_NOSETBYACS | CVAR_SERVERINFO )
{
	VOIPController::GetInstance( ).UpdateProximityChat( );

	// [AK] Notify the clients about the change.
	SERVER_SettingChanged( self, false );
}

//*****************************************************************************
//	CONSOLE COMMANDS

// [AK] Everything past this point only compiles if compiling with sound.
#ifndef NO_SOUND

static void voicechat_SetChannelVolume( FCommandLine &argv, const bool isIndexCmd )
{
	int player = MAXPLAYERS;

	// [AK] Mods are not allowed to change a VoIP channel's volume.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [AK] Show a tip message if there's not enough arguments.
	if ( argv.argc( ) < 3 )
	{
		Printf( "Sets a player's channel volume.\nUsage: %s <%s> <volume, 0.0 to 2.0>\n", argv[0], isIndexCmd ? "index" : "name" );
		return;
	}

	if ( argv.GetPlayerFromArg( player, 1, isIndexCmd, true ))
	{
		if ( player == consoleplayer )
		{
			Printf( "You can't set the volume of your own channel.\n" );
			return;
		}

		float volume = clamp<float>( static_cast<float>( atof( argv[2] )), 0.0f, 2.0f );
		VOIPController::GetInstance( ).SetChannelVolume( player, volume );
	}
}

// [AK] Changes the volume of one VoIP channel, using the player's name or index.
CCMD( voice_chanvolume )
{
	voicechat_SetChannelVolume( argv, false );
}

CCMD( voice_chanvolume_idx )
{
	voicechat_SetChannelVolume( argv, true );
}

// [AK] Lists all recording devices that are currently connected.
CCMD( voice_listrecorddrivers )
{
	TArray<FString> recordDriverList;
	VOIPController::GetInstance( ).RetrieveRecordDrivers( recordDriverList );

	for ( unsigned int i = 0; i < recordDriverList.Size( ); i++ )
		Printf( "%d. %s\n", i, recordDriverList[i].GetChars( ));
}

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
// [AK] VOIPController::VOIPController
//
// Initializes all members of VOIPController to their default values, and resets
// the state of the "voicerecord" button.
//
//*****************************************************************************

VOIPController::VOIPController( void ) :
	VoIPChannels{ nullptr },
	system( nullptr ),
	recordSound( nullptr ),
	VoIPChannelGroup( nullptr ),
	encoder( nullptr ),
	denoiseModel( nullptr ),
	denoiseState( nullptr ),
	recordDriverID( 0 ),
	framesSent( 0 ),
	lastRecordPosition( 0 ),
	isInitialized( false ),
	isActive( false ),
	isRecordButtonPressed( false ),
	transmissionType( TRANSMISSIONTYPE_OFF )
{
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		channelVolumes[i] = 1.0f;

	proximityInfo.SysChannel = nullptr;
	proximityInfo.StartTime.AsOne = 0;
	proximityInfo.Rolloff.RolloffType = ROLLOFF_Doom;
	proximityInfo.Rolloff.MinDistance = 200.0f;
	proximityInfo.Rolloff.MaxDistance = 1200.0f;
	proximityInfo.DistanceScale = 1.0f;

	Button_VoiceRecord.Reset( );
}

//*****************************************************************************
//
// [AK] VOIPController::Init
//
// Initializes the VoIP controller.
//
//*****************************************************************************

void VOIPController::Init( FMOD::System *mainSystem )
{
	int opusErrorCode = OPUS_OK;

	// [AK] The server never initializes the voice recorder.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	system = mainSystem;

	// [AK] Abort if the FMOD system is invalid. This should never happen.
	if ( system == nullptr )
	{
		Printf( TEXTCOLOR_ORANGE "Invalid FMOD::System pointer used to initialize VoIP controller.\n" );
		return;
	}

	FMOD_CREATESOUNDEXINFO exinfo = CreateSoundExInfo( RECORD_SAMPLE_RATE, RECORD_SOUND_LENGTH );

	// [AK] Abort if creating the sound to record into failed.
	if ( system->createSound( nullptr, FMOD_LOOP_NORMAL | FMOD_2D | FMOD_OPENUSER, &exinfo, &recordSound ) != FMOD_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to create sound for recording.\n" );
		return;
	}

	// [AK] Create the player VoIP channel group.
	if ( system->createChannelGroup( "VoIP", &VoIPChannelGroup ) != FMOD_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to create VoIP channel group for playback.\n" );
		return;
	}

	encoder = opus_encoder_create( PLAYBACK_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opusErrorCode );

	// [AK] Stop here if the Opus encoder wasn't created successfully.
	if ( opusErrorCode != OPUS_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to create Opus encoder: %s.\n", opus_strerror( opusErrorCode ));
		return;
	}

	opus_encoder_ctl( encoder, OPUS_SET_FORCE_CHANNELS( 1 ));
	opus_encoder_ctl( encoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_VOICE ));

	// [AK] Load a custom RNNoise model file if we can. Otherwise, use the built-in model.
	if ( strlen( voice_noisemodelfile ) > 0 )
	{
		const char *fileName = voice_noisemodelfile.GetGenericRep( CVAR_String ).String;
		FILE *modelFile = fopen( fileName, "r" );

		if ( modelFile != nullptr )
		{
			denoiseModel = rnnoise_model_from_file( modelFile );

			if ( denoiseModel == nullptr )
				Printf( TEXTCOLOR_ORANGE "Failed to load RNNoise model \"%s\". Using built-in model instead.\n", fileName );
		}
		else
		{
			Printf( TEXTCOLOR_YELLOW "Couldn't find RNNoise model \"%s\". Using built-in model instead.\n", fileName );
		}
	}

	// [AK] Initialize the denoise state, used for noise suppression.
	denoiseState = rnnoise_create( denoiseModel );

	isInitialized = true;
	Printf( "VoIP controller initialized successfully.\n" );

	// [AK] Set the output volume after initialization.
	SetVolume( voice_outputvolume );
}

//*****************************************************************************
//
// [AK] VOIPController::Shutdown
//
// Stops recording from the input device (if we were doing that), releases all
// memory used by the FMOD system, and shuts down the VoIP controller.
//
//*****************************************************************************

void VOIPController::Shutdown( void )
{
	Deactivate( );

	if ( encoder != nullptr )
	{
		opus_encoder_destroy( encoder );
		encoder = nullptr;
	}

	if ( recordSound != nullptr )
	{
		recordSound->release( );
		recordSound = nullptr;
	}

	if ( VoIPChannelGroup != nullptr )
	{
		VoIPChannelGroup->release( );
		VoIPChannelGroup = nullptr;
	}

	if ( denoiseModel != nullptr )
	{
		rnnoise_model_free( denoiseModel );
		denoiseModel = nullptr;
	}

	if ( denoiseState != nullptr )
	{
		rnnoise_destroy( denoiseState );
		denoiseState = nullptr;
	}

	isInitialized = false;
	isRecordButtonPressed = false;
	Printf( "VoIP controller shutting down.\n" );
}

//*****************************************************************************
//
// [AK] VOIPController::Activate
//
// Starts recording from the selected record driver.
//
//*****************************************************************************

void VOIPController::Activate( void )
{
	if (( isInitialized == false ) || ( isActive ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	StartRecording( );
	isActive = true;
}

//*****************************************************************************
//
// [AK] VOIPController::Deactivate
//
// Stops recording from the VoIP controller.
//
//*****************************************************************************

void VOIPController::Deactivate( void )
{
	if (( isInitialized == false ) || ( isActive == false ))
		return;

	// [AK] Clear all of the VoIP channels.
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		RemoveVoIPChannel( i );

	StopRecording( );

	framesSent = 0;
	isActive = false;
}

//*****************************************************************************
//
// [AK] VOIPController::Tick
//
// Executes any routines that the VoIP controller must do every tick.
//
//*****************************************************************************

template <typename T>
static void voicechat_ReadSoundBuffer( T *object, FMOD::Sound *sound, unsigned int &offset, const unsigned int length, void ( T::*callback )( unsigned char *, unsigned int ))
{
	void *ptr1, *ptr2;
	unsigned int len1, len2;

	if (( object == nullptr ) || ( sound == nullptr ) || ( callback == nullptr ) || ( length == 0 ))
		return;

	const unsigned int bufferSize = length * VOIPController::SAMPLE_SIZE;
	unsigned int soundLength = 0;

	// [AK] Lock the portion of the sound buffer that we want to read.
	if ( sound->lock( offset * VOIPController::SAMPLE_SIZE, bufferSize, &ptr1, &ptr2, &len1, &len2 ) == FMOD_OK )
	{
		if (( ptr1 != nullptr ) && ( len1 > 0 ))
		{
			// [AK] Combine the ptr1 and ptr2 buffers into a single buffer.
			if (( ptr2 != nullptr ) && ( len2 > 0 ))
			{
				unsigned char *combinedBuffer = new unsigned char[bufferSize];

				memcpy( combinedBuffer, ptr1, len1 );
				memcpy( combinedBuffer + len1, ptr2, len2 );

				( object->*callback )( combinedBuffer, bufferSize );

				memcpy( ptr1, combinedBuffer, len1 );
				memcpy( ptr2, combinedBuffer + len1, len2 );

				delete[] combinedBuffer;
			}
			else
			{
				( object->*callback )( reinterpret_cast<unsigned char *>( ptr1 ), len1 );
			}
		}

		// [AK] After everything's finished, unlock the sound buffer.
		sound->unlock( ptr1, ptr2, len1, len2 );
	}

	// [AK] Increment the offset.
	offset += length;

	if ( sound->getLength( &soundLength, FMOD_TIMEUNIT_PCM ) == FMOD_OK )
		offset = offset % soundLength;
}

//*****************************************************************************
//
void VOIPController::Tick( void )
{
	// [AK] Don't tick while the VoIP controller is uninitialized.
	if ( isInitialized == false )
		return;

	if ( IsVoiceChatAllowed( ))
	{
		if ( isActive == false )
			Activate( );
	}
	else if ( isActive )
	{
		Deactivate( );
	}

	// [AK] Check the status of the "voicerecord" button. If the button's been
	// pressed, start transmitting, or it's been released stop transmitting.
	if ( Button_VoiceRecord.bDown == false )
	{
		if ( isRecordButtonPressed )
		{
			isRecordButtonPressed = false;

			if ( transmissionType == TRANSMISSIONTYPE_BUTTON )
				StopTransmission( );
		}
	}
	else if ( isRecordButtonPressed == false )
	{
		isRecordButtonPressed = true;

		if ( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_PUSHTOTALK )
		{
			if ( IsVoiceChatAllowed( ))
				StartTransmission( TRANSMISSIONTYPE_BUTTON, true );
			// [AK] We can't transmit if we're watching a demo.
			else if ( CLIENTDEMO_IsPlaying( ))
				Printf( "Voice chat can't be used during demo playback.\n" );
			// ...or if we're in an offline game.
			else if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ))
				Printf( "Voice chat can't be used in a singleplayer game.\n" );
			// ...or if the server has disabled voice chatting.
			else if ( sv_allowvoicechat == VOICECHAT_OFF )
				Printf( "Voice chat has been disabled by the server.\n" );
		}
	}

	if ( isActive == false )
		return;

	// [AK] Are we're transmitting audio by pressing the "voicerecord" button right
	// now, or using voice activity detection? We'll check if we have enough new
	// samples recorded to fill an audio frame that can be encoded and sent out.
	if (( transmissionType != TRANSMISSIONTYPE_OFF ) || ( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_VOICEACTIVITY ))
	{
		unsigned int recordPosition = 0;

		if (( system->getRecordPosition( recordDriverID, &recordPosition ) == FMOD_OK ) && ( recordPosition != lastRecordPosition ))
		{
			unsigned int recordDelta = recordPosition >= lastRecordPosition ? recordPosition - lastRecordPosition : recordPosition + RECORD_SOUND_LENGTH - lastRecordPosition;

			// [AK] We may need to send out multiple audio frames in a single tic.
			for ( unsigned int frame = 0; frame < recordDelta / RECORD_SAMPLES_PER_FRAME; frame++ )
				voicechat_ReadSoundBuffer( this, recordSound, lastRecordPosition, RECORD_SAMPLES_PER_FRAME, &VOIPController::ReadRecordSamples );
		}
	}

	// [AK] Tick through all VoIP channels for each player.
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( VoIPChannels[i] == nullptr )
			continue;

		// [AK] Delete this channel if this player's no longer valid.
		if ( PLAYER_IsValidPlayer( i ) == false )
		{
			RemoveVoIPChannel( i );
			continue;
		}

		// [AK] If it's been long enough since we first received audio frames from
		// this player, start playing this channel. By now, the jitter buffer should
		// have enough samples for clean playback.
		if (( VoIPChannels[i]->sound != nullptr ) && ( VoIPChannels[i]->channel == nullptr ))
		{
			if (( VoIPChannels[i]->jitterBuffer.Size( ) == 0 ) || ( VoIPChannels[i]->playbackTick > gametic ))
				continue;

			VoIPChannels[i]->StartPlaying( );
		}

		// [AK] Keep updating the playback and reading more samples, such that there's
		// always enough gap between the number of samples read and played.
		if ( VoIPChannels[i]->channel != nullptr )
		{
			VoIPChannels[i]->UpdatePlayback( );
			const int sampleDiff = static_cast<int>( VoIPChannels[i]->samplesRead ) - static_cast<int>( VoIPChannels[i]->samplesPlayed );

			if ( sampleDiff < READ_BUFFER_SIZE )
			{
				const unsigned int samplesToRead = MIN( VoIPChannels[i]->GetUnreadSamples( ), READ_BUFFER_SIZE - sampleDiff );
				voicechat_ReadSoundBuffer( VoIPChannels[i], VoIPChannels[i]->sound, VoIPChannels[i]->lastReadPosition, samplesToRead, &VOIPChannel::ReadSamples );
			}
		}
	}
}

//*****************************************************************************
//
// [AK] VOIPController::ReadRecordSamples
//
// Reads samples from the recording sound's buffer into a single audio frame.
//
//*****************************************************************************

void VOIPController::ReadRecordSamples( unsigned char *soundBuffer, unsigned int length )
{
	float uncompressedBuffer[RECORD_SAMPLES_PER_FRAME];
	float downsizedBuffer[PLAYBACK_SAMPLES_PER_FRAME];
	float rms = 0.0f;

	for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
	{
		const unsigned int indexBase = i * SAMPLE_SIZE;
		union { DWORD l; float f; } dataUnion;

		dataUnion.l = 0;

		// [AK] Convert from a byte array to a float in little-endian.
		for ( unsigned int byte = 0; byte < SAMPLE_SIZE; byte++ )
			dataUnion.l |= soundBuffer[indexBase + byte] << 8 * byte;

		uncompressedBuffer[i] = clamp<float>( dataUnion.f * voice_recordvolume, -1.0f, 1.0f );
	}

	// [AK] Denoise the audio frame.
	if (( voice_suppressnoise ) && ( denoiseState != nullptr ))
	{
		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			uncompressedBuffer[i] *= SHRT_MAX;

		rnnoise_process_frame( denoiseState, uncompressedBuffer, uncompressedBuffer );

		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			uncompressedBuffer[i] /= SHRT_MAX;
	}

	// [AK] If using voice activity detection, calculate the RMS. This must be
	// done after denoising the audio frame.
	if ( transmissionType != TRANSMISSIONTYPE_BUTTON )
	{
		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			rms += powf( uncompressedBuffer[i], 2 );

		rms = sqrtf( rms / RECORD_SAMPLES_PER_FRAME );
	}

	// [AK] Check if the audio frame should actually be sent. This is always the
	// case while pressing the "voicerecord" button, or if the sound intensity
	// exceeds the minimum threshold.
	if (( transmissionType == TRANSMISSIONTYPE_BUTTON ) || ( 20 * log10( rms ) >= voice_recordsensitivity ))
	{
		// [AK] If we're using voice activity, and not transmitting audio already,
		// then start transmitting now.
		if ( transmissionType == TRANSMISSIONTYPE_OFF )
			StartTransmission( TRANSMISSIONTYPE_VOICEACTIVITY, false );

		// [AK] Downsize the input audio frame from 48 kHz to 24 kHz.
		for ( unsigned int i = 0; i < PLAYBACK_SAMPLES_PER_FRAME; i++ )
			downsizedBuffer[i] = ( uncompressedBuffer[2 * i] + uncompressedBuffer[2 * i + 1] ) / 2.0f;

		unsigned char compressedBuffer[MAX_PACKET_SIZE];
		int numBytesEncoded = EncodeOpusFrame( downsizedBuffer, PLAYBACK_SAMPLES_PER_FRAME, compressedBuffer, MAX_PACKET_SIZE );

		if ( numBytesEncoded > 0 )
			CLIENTCOMMANDS_VoIPAudioPacket( framesSent++, compressedBuffer, numBytesEncoded );
	}
	else
	{
		StopTransmission( );
	}
}

//*****************************************************************************
//
// [AK] VOIPController::StartRecording
//
// Starts recording from the input device chosen by voice_recorddriver.
//
//*****************************************************************************

void VOIPController::StartRecording( void )
{
	if ( IsRecording( ))
		return;

	int numRecordDrivers = 0;

	// [AK] Try to start recording from the selected record driver.
	if ( system->getRecordNumDrivers( &numRecordDrivers ) == FMOD_OK )
	{
		if ( numRecordDrivers > 0 )
		{
			if ( voice_recorddriver >= numRecordDrivers )
			{
				Printf( "Record driver %d doesn't exist. Using 0 instead.\n", *voice_recorddriver );
				recordDriverID = 0;
			}
			else
			{
				recordDriverID = voice_recorddriver;
			}

			if ( system->recordStart( recordDriverID, recordSound, true ) != FMOD_OK )
				Printf( TEXTCOLOR_ORANGE "Failed to start VoIP recording.\n" );
		}
		else
		{
			Printf( TEXTCOLOR_ORANGE "Failed to find any connected record drivers.\n" );
		}
	}
	else
	{
		Printf( TEXTCOLOR_ORANGE "Failed to retrieve number of record drivers.\n" );
	}
}

//*****************************************************************************
//
// [AK] VOIPController::StopRecording
//
// Stops recording from the selected input device.
//
//*****************************************************************************

void VOIPController::StopRecording( void )
{
	if ( IsRecording( ) == false )
		return;

	// [AK] If we're in the middle of a transmission, stop that too.
	StopTransmission( );

	if ( system->recordStop( recordDriverID ) != FMOD_OK )
		Printf( TEXTCOLOR_ORANGE "Failed to stop voice recording.\n" );
}

//*****************************************************************************
//
// [AK] VOIPController::StartTransmission
//
// Prepares the VoIP controller to start transmitting audio to the server.
//
//*****************************************************************************

void VOIPController::StartTransmission( const TRANSMISSIONTYPE_e type, const bool getRecordPosition )
{
	if (( isInitialized == false ) || ( isActive == false ) || ( transmissionType != TRANSMISSIONTYPE_OFF ))
		return;

	if (( getRecordPosition ) && ( system->getRecordPosition( recordDriverID, &lastRecordPosition ) != FMOD_OK ))
	{
		Printf( TEXTCOLOR_ORANGE "Failed to get position of voice recording.\n" );
		return;
	}

	transmissionType = type;
}

//*****************************************************************************
//
// [AK] VOIPController::StopTransmission
//
// Stops transmitting audio to the server.
//
//*****************************************************************************

void VOIPController::StopTransmission( void )
{
	transmissionType = TRANSMISSIONTYPE_OFF;
}

//*****************************************************************************
//
// [AK] VOIPController::IsVoiceChatAllowed
//
// Checks if voice chat can be used right now.
//
//*****************************************************************************

bool VOIPController::IsVoiceChatAllowed( void ) const
{
	// [AK] Voice chat can only be used in online games.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return false;

	// [AK] Voice chat can only be used when it's enabled.
	if (( sv_allowvoicechat == VOICECHAT_OFF ) || ( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_OFF ))
		return false;

	// [AK] Voice chat can only be used while in the level or intermission screen.
	if (( gamestate != GS_LEVEL ) && ( gamestate != GS_INTERMISSION ))
		return false;

	return true;
}

//*****************************************************************************
//
// [AK] VOIPController::IsPlayerTalking
//
// Checks if the specified player is talking right now. If the player is the
// same as the local player, then they're talking while transmitting audio.
// Otherwise, they're talking if their channel is playing.
//
//*****************************************************************************

bool VOIPController::IsPlayerTalking( const unsigned int player ) const
{
	if (( player == static_cast<unsigned>( consoleplayer )) && ( transmissionType != TRANSMISSIONTYPE_OFF ))
		return true;

	if (( PLAYER_IsValidPlayer( player )) && ( VoIPChannels[player] != nullptr ) && ( VoIPChannels[player]->channel != nullptr ))
	{
		// [AK] If this channel's playing in 3D mode, check if they're audible.
		// In case getting the channel's audibility fails, just return true.
		if ( VoIPChannels[player]->ShouldPlayIn3DMode( ))
		{
			float audibility = 0.0f;

			if ( VoIPChannels[player]->channel->getAudibility( &audibility ) == FMOD_OK )
				return ( audibility > 0.0f );
		}

		return true;
	}

	return false;
}

//*****************************************************************************
//
// [AK] VOIPController::IsRecording
//
// Checks if the VoIP controller is recording from the selected input device.
//
//*****************************************************************************

bool VOIPController::IsRecording( void ) const
{
	bool isRecording = false;

	if (( system != nullptr ) && ( system->isRecording( recordDriverID, &isRecording ) == FMOD_OK ))
		return isRecording;

	return false;
}

//*****************************************************************************
//
// [AK] VOIPController::SetChannelVolume
//
// Adjusts the volume for one particular VoIP channel.
//
//*****************************************************************************

void VOIPController::SetChannelVolume( const unsigned int player, float volume )
{
	if (( isInitialized == false ) || ( player >= MAXPLAYERS ))
		return;

	channelVolumes[player] = volume;

	if (( VoIPChannels[player] == nullptr ) || ( VoIPChannels[player]->channel == nullptr ))
		return;

	if ( VoIPChannels[player]->channel->setVolume( volume ) != FMOD_OK )
		Printf( TEXTCOLOR_ORANGE "Couldn't change the volume of VoIP channel %u.\n", player );
}

//*****************************************************************************
//
// [AK] VOIPController::SetVolume
//
// Adjusts the volume of all VoIP channels.
//
//*****************************************************************************

void VOIPController::SetVolume( float volume )
{
	if ( isInitialized == false )
		return;

	if (( VoIPChannelGroup == nullptr ) || ( VoIPChannelGroup->setVolume( volume ) != FMOD_OK ))
		Printf( TEXTCOLOR_ORANGE "Couldn't change the volume of the VoIP channel group.\n" );
}

//*****************************************************************************
//
// [AK] VOIPController::SetPitch
//
// Adjusts the pitch of all VoIP channels.
//
//*****************************************************************************

void VOIPController::SetPitch( float pitch )
{
	if ( isInitialized == false )
		return;

	float oldPitch = 1.0f;

	if (( VoIPChannelGroup == nullptr ) || ( VoIPChannelGroup->getPitch( &oldPitch ) != FMOD_OK ))
	{
		Printf( TEXTCOLOR_ORANGE "Couldn't get the pitch of the VoIP channel group.\n" );
		return;
	}

	// [AK] Stop if the pitch is already the same.
	if ( pitch == oldPitch )
		return;

	if ( VoIPChannelGroup->setPitch( pitch ) != FMOD_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Couldn't change the pitch of the VoIP channel group.\n" );
		return;
	}

	// [AK] When the pitch is changed, every VoIP channel's end delay time must
	// be updated to account for the new pitch. The epoch must also be reset.
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( VoIPChannels[i] != nullptr ) && ( VoIPChannels[i]->channel != nullptr ))
			VoIPChannels[i]->UpdateEndDelay( true );
	}
}

//*****************************************************************************
//
// [AK] VOIPController::RetrieveRecordDrivers
//
// Prints a list of all record drivers that are connected in the same format
// as FMODSoundRenderer::PrintDriversList.
//
//*****************************************************************************

void VOIPController::RetrieveRecordDrivers( TArray<FString> &list ) const
{
	int numDrivers = 0;
	char name[256];

	list.Clear( );

	if (( system != nullptr ) && ( system->getRecordNumDrivers( &numDrivers ) == FMOD_OK ))
	{
		for ( int i = 0; i < numDrivers; i++ )
		{
			if ( system->getRecordDriverInfo( i, name, sizeof( name ), nullptr ) == FMOD_OK )
				list.Push( name );
		}
	}
}

//*****************************************************************************
//
// [AK] VOIPController::GrabStats
//
// Returns a string showing the VoIP controller's status, which VoIP channels
// are currently playing, and how many samples have been read and played.
//
//*****************************************************************************

FString VOIPController::GrabStats( void ) const
{
	FString out;

	out.Format( "VoIP controller status: %s\n", transmissionType != TRANSMISSIONTYPE_OFF ? "transmitting" : ( isActive ? "activated" : "deactivated" ));

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( VoIPChannels[i] == nullptr )
			continue;

		out.AppendFormat( "VoIP channel %u (%s): ", i, players[i].userinfo.GetName( ));

		if ( IsPlayerTalking( i ))
		{
			out.AppendFormat( "samples read/played = %u/%u", VoIPChannels[i]->samplesRead, VoIPChannels[i]->samplesPlayed );

			if ( VoIPChannels[i]->samplesRead >= VoIPChannels[i]->samplesPlayed )
				out.AppendFormat( " (diff = %u)", VoIPChannels[i]->samplesRead - VoIPChannels[i]->samplesPlayed );
		}
		else
		{
			out += "not talking";
		}

		out += '\n';
	}

	return out;
}

//*****************************************************************************
//
// [AK] VOIPController::ReceiveAudioPacket
//
// This is called when the client receives an audio packet from the server,
// previously sent by another client. The packet is decoded and saved into the
// jitter buffer belonging to that client's channel, where it will be played.
//
//*****************************************************************************

void VOIPController::ReceiveAudioPacket( const unsigned int player, const unsigned int frame, const unsigned char *data, const unsigned int length )
{
	if (( isActive == false ) || ( PLAYER_IsValidPlayer( player ) == false ) || ( data == nullptr ) || ( length == 0 ))
		return;

	// [AK] If this player's channel doesn't exist yet, create a new one.
	if ( VoIPChannels[player] == nullptr )
		VoIPChannels[player] = new VOIPChannel( player );

	// [AK] Don't accept any frames that arrived too late.
	if ( frame < VoIPChannels[player]->lastFrameRead )
		return;

	VOIPChannel::AudioFrame newAudioFrame;
	newAudioFrame.frame = frame;

	if ( VoIPChannels[player]->DecodeOpusFrame( data, length, newAudioFrame.samples, PLAYBACK_SAMPLES_PER_FRAME ) > 0 )
	{
		// [AK] Insert the new audio frame in the jitter buffer. The frames must
		// be ordered correctly so that the audio isn't distorted.
		for ( unsigned int i = 0; i < VoIPChannels[player]->jitterBuffer.Size( ); i++ )
		{
			if ( newAudioFrame.frame < VoIPChannels[player]->jitterBuffer[i].frame )
			{
				VoIPChannels[player]->jitterBuffer.Insert( i, newAudioFrame );
				return;
			}
		}

		// [AK] Wait five tics before playing this VoIP channel.
		if (( VoIPChannels[player]->jitterBuffer.Size( ) == 0 ) && ( VoIPChannels[player]->channel == nullptr ))
			VoIPChannels[player]->playbackTick = gametic + 5;

		VoIPChannels[player]->jitterBuffer.Push( newAudioFrame );
	}
}

//*****************************************************************************
//
// [AK] VOIPController::UpdateProximityChat
//
// Updates the VoIP controller's proximity chat for every player's channel. If
// proximity chat is enabled, and the player isn't spectating or being spied
// on, then 3D mode is enabled and their 3D attributes (position and velocity)
// are updated.
//
// Otherwise, 3D mode is disabled and 2D mode is re-enabled.
//
//*****************************************************************************

void VOIPController::UpdateProximityChat( void )
{
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( playeringame[i] == false ) || ( VoIPChannels[i] == nullptr ) || ( VoIPChannels[i]->channel == nullptr ))
			continue;

		VoIPChannels[i]->Update3DAttributes( );
	}
}

//*****************************************************************************
//
// [AK] VOIPController::RemoveVoIPChannel
//
// Deletes a channel from the VOIP controller.
//
//*****************************************************************************

void VOIPController::RemoveVoIPChannel( const unsigned int player )
{
	if (( player < MAXPLAYERS ) && ( VoIPChannels[player] != nullptr ))
	{
		delete VoIPChannels[player];
		VoIPChannels[player] = nullptr;
	}
}

//*****************************************************************************
//
// [AK] VOIPController::EncodeOpusFrame
//
// Encodes a single audio frame using the Opus audio codec, and returns the
// number of bytes encoded. If encoding fails, an error message is printed.
//
//*****************************************************************************

int VOIPController::EncodeOpusFrame( const float *inBuffer, const unsigned int inLength, unsigned char *outBuffer, const unsigned int outLength )
{
	if (( inBuffer == nullptr ) || ( outBuffer == nullptr ))
		return 0;

	int numBytesEncoded = opus_encode_float( encoder, inBuffer, inLength, outBuffer, outLength );

	// [AK] Print the error message if encoding failed.
	if ( numBytesEncoded <= 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to encode Opus audio frame: %s.\n", opus_strerror( numBytesEncoded ));
		return 0;
	}

	return numBytesEncoded;
}

//*****************************************************************************
//
// [AK] VOIPController::CreateSoundExInfo
//
// Returns an FMOD_CREATESOUNDEXINFO struct with the settings needed to create
// new FMOD sounds used by the VoIP controller. The sample rate and file length
// (in PCM samples) can be adjusted as required.
//
//*****************************************************************************

FMOD_CREATESOUNDEXINFO VOIPController::CreateSoundExInfo( const unsigned int sampleRate, const unsigned int fileLength )
{
	FMOD_CREATESOUNDEXINFO exinfo;

	memset( &exinfo, 0, sizeof( FMOD_CREATESOUNDEXINFO ));
	exinfo.cbsize = sizeof( FMOD_CREATESOUNDEXINFO );
	exinfo.numchannels = 1;
	exinfo.format = FMOD_SOUND_FORMAT_PCMFLOAT;
	exinfo.defaultfrequency = sampleRate;
	exinfo.length = fileLength * SAMPLE_SIZE;

	return exinfo;
}

//*****************************************************************************
//
// [AK] VOIPController::ChannelCallback
//
// Static callback function that executes when a VoIP channel stops playing.
//
//*****************************************************************************

FMOD_RESULT F_CALLBACK VOIPController::ChannelCallback( FMOD_CHANNEL *channel, FMOD_CHANNEL_CALLBACKTYPE type, void *commanddata1, void *commanddata2 )
{
	if ( type == FMOD_CHANNEL_CALLBACKTYPE_END )
	{
		FMOD::Channel *castedChannel = reinterpret_cast<FMOD::Channel *>( channel );

		// [AK] Find which VoIP channel this object belongs to.
		for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		{
			VOIPChannel *VoIPChannel = GetInstance( ).VoIPChannels[i];

			if (( VoIPChannel != nullptr ) && ( castedChannel == VoIPChannel->channel ))
			{
				// [AK] Reset the read and playback positions.
				VoIPChannel->channel = nullptr;
				VoIPChannel->lastReadPosition = 0;
				VoIPChannel->lastPlaybackPosition = 0;

				// [AK] Check if this VoIP channel still has any samples that
				// haven't been read into the sound's buffer yet. If there are,
				// read them and play the channel again.
				if ( VoIPChannel->GetUnreadSamples( ) > 0 )
				{
					VoIPChannel->samplesPlayed = VoIPChannel->samplesRead;
					VoIPChannel->StartPlaying( );
				}
				else
				{
					VoIPChannel->lastFrameRead = 0;
					VoIPChannel->samplesRead = 0;
					VoIPChannel->samplesPlayed = 0;
				}

				break;
			}
		}
	}

	return FMOD_OK;
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::VOIPChannel
//
// Creates the channel's decoder and FMOD sound, and sets all members to their
// default values.
//
//*****************************************************************************

VOIPController::VOIPChannel::VOIPChannel( const unsigned int player ) :
	player( player ),
	sound( nullptr ),
	channel( nullptr ),
	decoder( nullptr ),
	playbackTick( 0 ),
	lastReadPosition( 0 ),
	lastPlaybackPosition( 0 ),
	lastFrameRead( 0 ),
	samplesRead( 0 ),
	samplesPlayed( 0 ),
	dspEpochHi( 0 ),
	dspEpochLo( 0 ),
	endDelaySamples( 0 )
{
	int opusErrorCode = OPUS_OK;
	decoder = opus_decoder_create( PLAYBACK_SAMPLE_RATE, 1, &opusErrorCode );

	// [AK] Print an error message if the Opus decoder wasn't created successfully.
	if ( opusErrorCode != OPUS_OK )
		Printf( TEXTCOLOR_ORANGE "Failed to create Opus decoder for VoIP channel %u: %s.\n", player, opus_strerror( opusErrorCode ));

	FMOD_CREATESOUNDEXINFO exinfo = CreateSoundExInfo( PLAYBACK_SAMPLE_RATE, PLAYBACK_SOUND_LENGTH );
	FMOD_MODE mode = FMOD_3D | FMOD_OPENUSER | FMOD_LOOP_NORMAL | FMOD_SOFTWARE;

	if (( VOIPController::GetInstance( ).system == nullptr ) || ( VOIPController::GetInstance( ).system->createSound( nullptr, mode, &exinfo, &sound ) != FMOD_OK ))
		Printf( TEXTCOLOR_ORANGE "Failed to create sound for VoIP channel %u.\n", player );
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::~VOIPChannel
//
// Destroys the decoder and FMOD sound/channel.
//
//*****************************************************************************

VOIPController::VOIPChannel::~VOIPChannel( void )
{
	if ( channel != nullptr )
	{
		channel->stop( );
		channel = nullptr;
	}

	if ( sound != nullptr )
	{
		sound->release( );
		sound = nullptr;
	}

	if ( decoder != nullptr )
	{
		opus_decoder_destroy( decoder );
		decoder = nullptr;
	}

	// [AK] Reset this channel's volume back to default.
	VOIPController::GetInstance( ).channelVolumes[player] = 1.0f;
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::ShouldPlayIn3DMode
//
// Checks if the VoIP channel should be played in 3D mode. To do so, proximity
// chat must be enabled while in a level, and the player can't be spectating or
// be spied on by the local player.
//
//*****************************************************************************

bool VOIPController::VOIPChannel::ShouldPlayIn3DMode( void ) const
{
	if (( sv_proximityvoicechat == false ) || ( gamestate != GS_LEVEL ) || ( PLAYER_IsValidPlayer( player ) == false ))
		return false;

	return (( players[player].bSpectating == false ) && ( players[player].mo != nullptr ) && ( players[player].mo != players[consoleplayer].camera ));
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::GetUnreadSamples
//
// Returns the number of samples that haven't been read into the VoIP channel's
// sound buffer yet. This includes the total samples still in the jitter buffer
// and "extra" samples from previous VOIPChannel::ReadSamples calls.
//
//*****************************************************************************

int VOIPController::VOIPChannel::GetUnreadSamples( void ) const
{
	return jitterBuffer.Size( ) * PLAYBACK_SAMPLES_PER_FRAME + extraSamples.Size( );
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::DecodeOpusFrame
//
// Decodes a single audio frame using the Opus audio codec, and returns the
// number of bytes decoded. If decoding fails, an error message is printed.
//
//*****************************************************************************

int VOIPController::VOIPChannel::DecodeOpusFrame( const unsigned char *inBuffer, const unsigned int inLength, float *outBuffer, const unsigned int outLength )
{
	if (( decoder == nullptr ) || ( inBuffer == nullptr ) || ( outBuffer == nullptr ))
		return 0;

	int numBytesDecoded = opus_decode_float( decoder, inBuffer, inLength, outBuffer, outLength, 0 );

	// [AK] Print the error message if decoding failed.
	if ( numBytesDecoded <= 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to decode Opus audio frame: %s.\n", opus_strerror( numBytesDecoded ));
		return 0;
	}

	return numBytesDecoded;
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::StartPlaying
//
// Starts playing the VoIP channel.
//
//*****************************************************************************

void VOIPController::VOIPChannel::StartPlaying( void )
{
	if ( channel != nullptr )
		return;

	if ( VOIPController::GetInstance( ).system->playSound( FMOD_CHANNEL_FREE, sound, true, &channel ) != FMOD_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to start playing VoIP channel %u.\n", player );
		return;
	}

	channel->setUserData( &VOIPController::GetInstance( ).proximityInfo );
	channel->setCallback( VOIPController::ChannelCallback );

	// [AK] Give the VoIP channels more priority than other sounds.
	channel->setPriority( 0 );

	// [AK] Reset the channel's end delay epoch before playing.
	UpdateEndDelay( true );

	// [AK] Update this channel's 3D attributes.
	Update3DAttributes( );

	voicechat_ReadSoundBuffer( this, sound, lastReadPosition, MIN( GetUnreadSamples( ), READ_BUFFER_SIZE ), &VOIPChannel::ReadSamples );

	channel->setChannelGroup( VOIPController::GetInstance( ).VoIPChannelGroup );
	channel->setVolume( VOIPController::GetInstance( ).channelVolumes[player] );
	channel->setPaused( false );
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::ReadSamples
//
// Stops playing the voice recording, clears the jitter buffer, and releases
// any memory the sound and/or channel was using.
//
//*****************************************************************************

static void voicechat_FloatToByteArray( const float value, unsigned char *bytes )
{
	if ( bytes == nullptr )
		return;

	union { DWORD l; float f; } dataUnion;
	dataUnion.f = value;

	for ( unsigned int i = 0; i < 4; i++ )
		bytes[i] = ( dataUnion.l >> 8 * i ) & 0xFF;
}

//*****************************************************************************
//
void VOIPController::VOIPChannel::ReadSamples( unsigned char *soundBuffer, const unsigned int length )
{
	const unsigned int samplesInBuffer = length / SAMPLE_SIZE;
	unsigned int samplesReadIntoBuffer = 0;

	// [AK] Read the extra samples into the sound buffer first. Make sure to
	// only read as many samples as what can fit in the sound buffer.
	if ( extraSamples.Size( ) > 0 )
	{
		const unsigned int maxExtraSamples = MIN<unsigned int>( extraSamples.Size( ), samplesInBuffer );

		for ( unsigned int i = 0; i < maxExtraSamples; i++ )
		{
			voicechat_FloatToByteArray( extraSamples[0], soundBuffer + i * SAMPLE_SIZE );
			extraSamples.Delete( 0 );
		}

		samplesReadIntoBuffer += maxExtraSamples;
	}

	// [AK] If there's still room left to read more samples, then start reading
	// frames from the jitter buffer. First, find how many frames are needed in
	// the sound buffer with respect to how many samples have already been read,
	// then determine how many frames can actually be read. It's possible that
	// there's less frames in the jitter buffer than what's required.
	if ( samplesReadIntoBuffer < samplesInBuffer )
	{
		const unsigned int framesRequired = static_cast<unsigned int>( ceil( static_cast<float>( samplesInBuffer - samplesReadIntoBuffer ) / PLAYBACK_SAMPLES_PER_FRAME ));
		const unsigned int framesToRead = MIN<unsigned int>( framesRequired, jitterBuffer.Size( ));

		for ( unsigned int frame = 0; frame < framesToRead; frame++ )
		{
			for ( unsigned int i = 0; i < PLAYBACK_SAMPLES_PER_FRAME; i++ )
			{
				if ( samplesReadIntoBuffer < samplesInBuffer )
				{
					voicechat_FloatToByteArray( jitterBuffer[0].samples[i], soundBuffer + samplesReadIntoBuffer * SAMPLE_SIZE );
					samplesReadIntoBuffer++;
				}
				else
				{
					extraSamples.Push( jitterBuffer[0].samples[i] );
				}
			}

			lastFrameRead = jitterBuffer[0].frame;
			jitterBuffer.Delete( 0 );
		}
	}

	samplesRead += samplesReadIntoBuffer;
	UpdateEndDelay( false );
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::Update3DAttributes
//
// Updates a channel's 3D attributes.
//
//*****************************************************************************

void VOIPController::VOIPChannel::Update3DAttributes( void )
{
	FMOD_VECTOR pos = { 0.0f, 0.0f, 0.0f };
	FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };

	// [AK] If this channel shouldn't play in "3D" mode, then set its position
	// and velocity to the listener's. This effectively makes them sound "2D".
	if ( ShouldPlayIn3DMode( ) == false )
	{
		if ( VOIPController::GetInstance( ).system == nullptr )
		{
			Printf( TEXTCOLOR_ORANGE "Can't get 3D attributes of the listener without a valid FMOD system.\n" );
			return;
		}

		if ( VOIPController::GetInstance( ).system->get3DListenerAttributes( 0, &pos, &vel, nullptr, nullptr ) != FMOD_OK )
		{
			Printf( TEXTCOLOR_ORANGE "Failed to get 3D attributes of the listener.\n" );
			return;
		}
	}
	else if ( players[player].mo != nullptr )
	{
		pos.x = FIXED2FLOAT( players[player].mo->x );
		pos.y = FIXED2FLOAT( players[player].mo->z );
		pos.z = FIXED2FLOAT( players[player].mo->y );

		vel.x = FIXED2FLOAT( players[player].mo->velx );
		vel.y = FIXED2FLOAT( players[player].mo->velz );
		vel.z = FIXED2FLOAT( players[player].mo->vely );
	}

	if ( channel->set3DAttributes( &pos, &vel ) != FMOD_OK )
		Printf( TEXTCOLOR_ORANGE "Failed to set 3D attributes for VoIP channel %u.\n", player );
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::UpdatePlayback
//
// Updates the playback position and the number of samples played.
//
//*****************************************************************************

void VOIPController::VOIPChannel::UpdatePlayback( void )
{
	unsigned int playbackPosition = 0;

	// [AK] Check how many new samples have been played since the last call.
	if ( channel->getPosition( &playbackPosition, FMOD_TIMEUNIT_PCM ) == FMOD_OK )
	{
		unsigned int playbackDelta = 0;

		if ( playbackPosition >= lastPlaybackPosition )
			playbackDelta = playbackPosition - lastPlaybackPosition;
		else
			playbackDelta = playbackPosition + PLAYBACK_SOUND_LENGTH - lastPlaybackPosition;

		samplesPlayed += playbackDelta;
		lastPlaybackPosition = playbackPosition;
	}
}

//*****************************************************************************
//
// [AK] VOIPController::VOIPChannel::UpdateEndDelay
//
// Determines precisely when a VoIP channel needs to stop, with respect to the
// FMOD system's DSP clock and sample rate. This is a sample-accurate way of
// knowing how long a channel should play without any "spilling" (i.e. playing
// more samples than read).
//
//*****************************************************************************

void VOIPController::VOIPChannel::UpdateEndDelay( const bool resetEpoch )
{
	if (( channel == nullptr ) || ( VOIPController::GetInstance( ).system == nullptr ))
		return;

	// [AK] Resetting the epoch means that we get the current DSP clock time of
	// the system and the current number of samples played. The latter becomes
	// the new base which we subtract the number of read samples by.
	if ( resetEpoch )
	{
		VOIPController::GetInstance( ).system->getDSPClock( &dspEpochHi, &dspEpochLo );
		UpdatePlayback( );

		endDelaySamples = samplesPlayed;
	}

	// [AK] The channel should stop immediately if the number of samples read is
	// less than or equal to the "end delay" samples.
	if ( samplesRead <= endDelaySamples )
	{
		channel->setDelay( FMOD_DELAYTYPE_DSPCLOCK_END, dspEpochHi, dspEpochLo );
		return;
	}

	int sysSampleRate = 0;
	unsigned int newDSPHi = dspEpochHi;
	unsigned int newDSPLo = dspEpochLo;
	FMOD::ChannelGroup *channelGroup = nullptr;

	// [AK] It's important to consider that the system and channel might not
	// be playing at the same sample rates. Therefore, we must convert the
	// number of samples with respect to the system's sample rate.
	VOIPController::GetInstance( ).system->getSoftwareFormat( &sysSampleRate, nullptr, nullptr, nullptr, nullptr, nullptr );
	float scalar = static_cast<float>( sysSampleRate ) / PLAYBACK_SAMPLE_RATE;

	// [AK] The channel's pitch might've changed (e.g. listening underwater).
	// This also affects the end delay time; lower pitches extend the time and
	// higher pitches shorten it.
	if (( channel->getChannelGroup( &channelGroup ) == FMOD_OK ) && ( channelGroup != nullptr ))
	{
		float channelGroupPitch = 1.0f;

		channelGroup->getPitch( &channelGroupPitch );
		scalar /= channelGroupPitch;
	}

	FMOD_64BIT_ADD( newDSPHi, newDSPLo, 0, static_cast<unsigned int>(( samplesRead - endDelaySamples ) * scalar ));
	channel->setDelay( FMOD_DELAYTYPE_DSPCLOCK_END, newDSPHi, newDSPLo );
}

#endif // NO_SOUND

//*****************************************************************************
//	STATISTICS

#ifndef NO_SOUND

ADD_STAT( voice )
{
	return VOIPController::GetInstance( ).GrabStats( );
}

#endif // NO_SOUND
