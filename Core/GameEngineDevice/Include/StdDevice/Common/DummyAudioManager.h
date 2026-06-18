/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @build bobtista 13/06/2026 No-op AudioManager for platforms
// without an audio backend (Android: OpenAL would drag in FFmpeg). Keeps TheAudio
// non-null so the engine boots; all audio operations are silent no-ops.
#pragma once

#include "Common/GameAudio.h"

class DummyAudioManager : public AudioManager
{
public:
	DummyAudioManager() {}
	virtual ~DummyAudioManager() {}

	virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *userData, FILE *fp = nullptr ) {}
	virtual void stopAudio( AudioAffect which ) {}
	virtual void pauseAudio( AudioAffect which ) {}
	virtual void resumeAudio( AudioAffect which ) {}
	virtual void pauseAmbient( Bool shouldPause ) {}
	virtual void killAudioEventImmediately( AudioHandle audioEvent ) {}
	virtual void nextMusicTrack() {}
	virtual void prevMusicTrack() {}
	virtual Bool isMusicPlaying() const { return FALSE; }
	virtual Bool hasMusicTrackCompleted( const AsciiString& trackName, Int numberOfTimes ) const { return FALSE; }
	virtual AsciiString getMusicTrackName() const { return AsciiString::TheEmptyString; }
	virtual void openDevice() {}
	virtual void closeDevice() {}
	virtual void *getDevice() {}
	virtual void notifyOfAudioCompletion( UnsignedInt audioCompleted, UnsignedInt flags ) {}
	virtual UnsignedInt getProviderCount() const { return 0; }
	virtual AsciiString getProviderName( UnsignedInt providerNum ) const { return AsciiString::TheEmptyString; }
	virtual UnsignedInt getProviderIndex( AsciiString providerName ) const { return 0; }
	virtual void selectProvider( UnsignedInt providerNdx ) {}
	virtual void unselectProvider() {}
	virtual UnsignedInt getSelectedProvider() const { return 0; }
	virtual void setSpeakerType( UnsignedInt speakerType ) {}
	virtual UnsignedInt getSpeakerType() { return 0; }
	virtual UnsignedInt getNum2DSamples() const { return 0; }
	virtual UnsignedInt getNum3DSamples() const { return 0; }
	virtual UnsignedInt getNumStreams() const { return 0; }
	virtual Bool doesViolateLimit( AudioEventRTS *event ) const { return FALSE; }
	virtual Bool isPlayingLowerPriority( AudioEventRTS *event ) const { return FALSE; }
	virtual Bool isPlayingAlready( AudioEventRTS *event ) const { return FALSE; }
	virtual Bool isObjectPlayingVoice( UnsignedInt objID ) const { return FALSE; }
	virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume) {}
	virtual void removePlayingAudio( AsciiString eventName ) {}
	virtual void removeAllDisabledAudio() {}
	virtual Bool has3DSensitiveStreamsPlaying() const { return FALSE; }
	virtual void *getHandleForBink() {}
	virtual void releaseHandleForBink() {}
	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay) {}
	virtual void setPreferredProvider(AsciiString providerNdx) {}
	virtual void setPreferredSpeaker(AsciiString speakerType) {}
	virtual Real getFileLengthMS( AsciiString strToLoad ) const { return 0.0f; }
	virtual void closeAnySamplesUsingFile( const void *fileToClose ) {}
	virtual void setDeviceListenerPosition() {}
};
