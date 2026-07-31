/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright(C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: GameSounds.cpp
//
// Created:   5/02/01
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Lib/BaseType.h"
#include "Common/GameSounds.h"

#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioRequest.h"
#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#if RTS_SDL3_ENABLE
#include "Common/SeatManager.h"
#endif

#include "GameLogic/PartitionManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

// Seat 0 plus, when splitscreen is compiled in, every other local seat. See canPlayNow.
#if RTS_SDL3_ENABLE
enum { MAX_AUDIO_LISTENERS = MAX_SEATS + 1 };
#else
enum { MAX_AUDIO_LISTENERS = 1 };
#endif

//-------------------------------------------------------------------------------------------------
SoundManager::SoundManager()
{
	// nada to do
}

//-------------------------------------------------------------------------------------------------
SoundManager::~SoundManager()
{
	// nada to do
}

//-------------------------------------------------------------------------------------------------
void SoundManager::init()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::postProcessLoad()
{
	// The AudioManager should actually be live now, so go ahead and get the info we need from it
	// here
}

//-------------------------------------------------------------------------------------------------
void SoundManager::update()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::reset()
{
	m_numPlaying2DSamples = 0;
	m_numPlaying3DSamples = 0;
}

//-------------------------------------------------------------------------------------------------
void SoundManager::loseFocus()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::regainFocus()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setListenerPosition( const Coord3D *position )
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setViewRadius( Real viewRadius )
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setCameraAudibleDistance( Real audibleDistance )
{

}

//-------------------------------------------------------------------------------------------------
Real SoundManager::getCameraAudibleDistance()
{
	return 1.0f;
}

//-------------------------------------------------------------------------------------------------
void SoundManager::addAudioEvent(AudioEventRTS *&eventToAdd)
{
	if (m_num2DSamples == 0 && m_num3DSamples == 0) {
		m_num2DSamples = TheAudio->getNum2DSamples();
		m_num3DSamples = TheAudio->getNum3DSamples();
	}

	if (canPlayNow(eventToAdd)) {
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG((" - appended to request list with handle '%d'.", (UnsignedInt) eventToAdd->getPlayingHandle()));
#endif
		AudioRequest *audioRequest = TheAudio->allocateAudioRequest( true );
		audioRequest->m_pendingEvent = eventToAdd;
		audioRequest->m_request = AR_Play;
		TheAudio->appendAudioRequest(audioRequest);
	} else {
		TheAudio->releaseAudioEventRTS(eventToAdd);
	}
}

//-------------------------------------------------------------------------------------------------
void SoundManager::notifyOf2DSampleStart()
{
	++m_numPlaying2DSamples;
}

//-------------------------------------------------------------------------------------------------
void SoundManager::notifyOf3DSampleStart()
{
	++m_numPlaying3DSamples;
}

//-------------------------------------------------------------------------------------------------
void SoundManager::notifyOf2DSampleCompletion()
{
	if (m_numPlaying2DSamples > 0) {
		--m_numPlaying2DSamples;
	}
}

//-------------------------------------------------------------------------------------------------
void SoundManager::notifyOf3DSampleCompletion()
{
	if (m_numPlaying3DSamples > 0) {
		--m_numPlaying3DSamples;
	}
}

//-------------------------------------------------------------------------------------------------
Int SoundManager::getAvailableSamples()
{
	return (m_num2DSamples - m_numPlaying2DSamples);
}

//-------------------------------------------------------------------------------------------------
Int SoundManager::getAvailable3DSamples()
{
	return (m_num3DSamples - m_numPlaying3DSamples);
}

//-------------------------------------------------------------------------------------------------
AsciiString SoundManager::getFilenameForPlayFromAudioEvent( const AudioEventRTS *eventToGetFrom )
{
	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::canPlayNow( AudioEventRTS *event )
{
	Bool retVal = false;
	// 1) Are we muted because we're beyond our maximum distance?
	// 2) Are we shrouded and this is a shroud sound?
	// 3) Are we violating our voice count or are we playing above the limit? (If so, stop now)
	// 4) is there an available channel open?
	// 5) if not, then determine if there is anything of lower priority that we can kill
	// 6) if not, are we an interrupt-sound type?
	// if so, are there any sounds of our type playing right now that we can interrupt?
	// potentially here: Are there any sounds that are playing that are now beyond their distance?
	// if so, kill them and start our sound
	// if not, we're done. Can't play dude.

	if( event->isPositionalAudio() && !BitIsSet( event->getAudioEventInfo()->m_type, ST_GLOBAL) && event->getAudioEventInfo()->m_priority != AP_CRITICAL )
	{
		Coord3D distance = *TheAudio->getListenerPosition();
		const Coord3D *pos = event->getCurrentPosition();
		if (pos)
		{
			distance.sub(pos);
			Real nearest = distance.length();
			Int listenerPlayers[MAX_AUDIO_LISTENERS];
			Int listenerCount = 0;
			listenerPlayers[listenerCount++] = rts::getObservedOrLocalPlayer()->getPlayerIndex();

#if RTS_SDL3_ENABLE
			// Splitscreen: both of these culls ask "is this near/visible to the player watching",
			// and there are up to eight of those, each looking somewhere else on the map. Asked
			// only of seat 0 they answered no for everything happening at any other seat's base -
			// which is most of what the other seats do - so those viewports played nearly silent.
			// The answer for the machine is the answer for the closest of the people at it.
			if (TheSeatManager != nullptr && TheSeatManager->isSplitscreenEnabled())
			{
				Int seatPlayers[MAX_SEATS];
				Coord3D seatLookAt[MAX_SEATS];
				const Int seatCount = TheSeatManager->getExtraLocalListeners(seatPlayers, seatLookAt, MAX_SEATS);
				for (Int i = 0; i < seatCount; ++i)
				{
					Coord3D d = seatLookAt[i];
					d.sub(pos);
					const Real len = d.length();
					if (len < nearest)
						nearest = len;
					listenerPlayers[listenerCount++] = seatPlayers[i];
				}
			}
#endif

			if (nearest >= event->getAudioEventInfo()->m_maxDistance)
			{
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG(("- culled due to distance (%.2f).", nearest));
#endif
				return false;
			}

			if( event->getAudioEventInfo()->m_type & ST_SHROUDED )
			{
				Bool anyClear = false;
				for (Int i = 0; i < listenerCount && !anyClear; ++i)
				{
					if (ThePartitionManager->getShroudStatusForPlayer(listenerPlayers[i], pos) == CELLSHROUD_CLEAR)
						anyClear = true;
				}

				if (!anyClear)
				{
#ifdef INTENSIVE_AUDIO_DEBUG
					DEBUG_LOG(("- culled due to shroud."));
#endif
					return false;
				}
			}
		}
	}

	if (violatesVoice(event))
	{
		retVal = isInterrupting(event);
		if (retVal)
		{
			return true;
		}
		else
		{
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- culled due to voice."));
#endif
			return false;
		}
	}

	if( TheAudio->doesViolateLimit( event ) )
	{
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- culled due to limit." ));
#endif
		return false;
	}
	else if( isInterrupting( event ) )
	{
		return true;
	}

	if (event->isPositionalAudio())
	{
		if (m_numPlaying3DSamples < m_num3DSamples)
		{
			return true;
		}
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- %d samples playing, %d samples available", m_numPlaying3DSamples, m_num3DSamples));
#endif
	}
	else
	{
		// its a UI sound (and thus, 2-D)
		if (m_numPlaying2DSamples < m_num2DSamples)
		{
			return true;
		}
	}

	if (TheAudio->isPlayingLowerPriority(event))
	{
		return true;
	}

	if (isInterrupting(event))
	{
		retVal = TheAudio->isPlayingAlready(event);
		if (retVal)
		{
			return true;
		}
		else
		{
#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG(("- culled due to no channels available and non-interrupting." ));
#endif
			return false;
		}
	}
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("culled due to unavailable channels"));
#endif
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::violatesVoice( AudioEventRTS *event )
{
	if (event->getAudioEventInfo()->m_type & ST_VOICE) {
		return (event->getObjectID() && TheAudio->isObjectPlayingVoice(event->getObjectID()));
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::isInterrupting( AudioEventRTS *event )
{
	return event->getAudioEventInfo()->m_control & AC_INTERRUPT;
}
