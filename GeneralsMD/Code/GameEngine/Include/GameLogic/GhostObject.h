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

// FILE: GhostObject.h ////////////////////////////////////////////////////////////
// Placeholder for objects that have been deleted but need to be maintained because
// a player can see them fogged.
// Author: Mark Wilczynski, August 2002

#pragma once

#include "Lib/BaseType.h"
#include "Common/Snapshot.h"

// #define DEBUG_FOG_MEMORY	///< this define is used to force object snapshots for all players, not just local player.

class Object;
class PartitionData;
enum GeometryType CPP_11(: Int);
enum ObjectID CPP_11(: Int);

class GhostObject : public Snapshot
{
public:
	GhostObject();
	virtual ~GhostObject();
	virtual void snapShot(int playerIndex)=0;
	virtual void updateParentObject(Object *object, PartitionData *mod)=0;
	virtual void freeSnapShot(int playerIndex)=0;

	/// Splitscreen: which player's snapshot is currently standing in for the real object in the
	/// shared 3D scene, or -1 if none is. Every viewport draws that one scene, so a viewport whose
	/// render player does NOT own the snapshot must skip it - otherwise one seat's fogged memory
	/// of a building appears as a dark ghost in everyone else's viewport.
	virtual Int getSceneSnapshotPlayer() const { return -1; }
	/** Splitscreen: does this player actually have a remembered snapshot of the object?
		One grey snapshot stands in for the real object in the ONE shared scene, so it is drawn
		with whichever player recorded it; every viewport must still decide for itself whether its
		own player has ever seen the thing. Without this a seat that has never laid eyes on a
		building still saw another seat's memory of it. */
	virtual Bool hasSnapshotForPlayer(Int playerIndex) const { return TRUE; }
	PartitionData *friend_getPartitionData() const {return m_partitionData;}
	GeometryType getGeometryType() const {return m_parentGeometryType;}
	Bool getGeometrySmall() const {return m_parentGeometryIsSmall;}
	Real getGeometryMajorRadius() const {return m_parentGeometryMajorRadius;}
	Real getGeometryMinorRadius() const {return m_parentGeometryminorRadius;}
	Real getParentAngle() const {return m_parentAngle;}
	const Coord3D *getParentPosition() const {return &m_parentPosition;}

protected:
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	Object *m_parentObject;		///< object which we are ghosting
	GeometryType m_parentGeometryType;
	Bool m_parentGeometryIsSmall;
	Real m_parentGeometryMajorRadius;
	Real m_parentGeometryminorRadius;
	Real m_parentAngle;
	Coord3D m_parentPosition;
	PartitionData	*m_partitionData;	///< our PartitionData
};

class GhostObjectManager : public Snapshot
{
public:
	GhostObjectManager();
	virtual ~GhostObjectManager();

	virtual void reset();
	virtual GhostObject *addGhostObject(Object *object, PartitionData *pd);
	virtual void removeGhostObject(GhostObject *mod);
	virtual void setLocalPlayerIndex(int playerIndex) { m_localPlayer = playerIndex; }
	int getLocalPlayerIndex()	{ return m_localPlayer; }
	virtual void updateOrphanedObjects(int *playerIndexList, int playerIndexCount);
	virtual void releasePartitionData();	///<saves data needed to later rebuild partition manager data.
	virtual void restorePartitionData();	///<restores ghost objects into the partition manager.
	void lockGhostObjects(Bool enableLock) {m_lockGhostObjects=enableLock;}	///<temporary lock on creating new ghost objects. Only used by map border resizing!
	void saveLockGhostObjects(Bool enableLock) {m_saveLockGhostObjects=enableLock;}
	inline Bool trackAllPlayers() const; ///< returns whether the ghost object status is tracked for all players or for the local player only

protected:
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	Int m_localPlayer;
	Bool m_lockGhostObjects;
	Bool m_saveLockGhostObjects;	///< used to lock the ghost object system during a save/load
	Bool m_trackAllPlayers; ///< if enabled, tracks ghost object status for all players, otherwise for the local player only
};

/// Splitscreen: TRUE while more than one local seat is playing. Every seat is a "local" player
/// whose fog memory has to be maintained, so the ghost system must track all players - the vanilla
/// single-local-player path would only ever compute seat 0's shroud state. Defined in
/// GhostObject.cpp so this header does not have to pull in SeatManager.h.
extern Bool rts_isMultiSeatFogActive();

inline Bool GhostObjectManager::trackAllPlayers() const
{
#ifdef DEBUG_FOG_MEMORY
	return true;
#else
	return m_trackAllPlayers || rts_isMultiSeatFogActive();
#endif
}

// TheSuperHackers @feature bobtista 19/01/2026
// GhostObjectManager that does nothing for headless mode.
// Note: Does NOT override crc/xfer/loadPostProcess to maintain save compatibility.
class GhostObjectManagerDummy : public GhostObjectManager
{
public:
	virtual void reset() override {}
	virtual GhostObject *addGhostObject(Object *object, PartitionData *pd) override { return nullptr; }
	virtual void removeGhostObject(GhostObject *mod) override {}
	virtual void updateOrphanedObjects(int *playerIndexList, int playerIndexCount) override {}
	virtual void releasePartitionData() override {}
	virtual void restorePartitionData() override {}
};

// the singleton
extern GhostObjectManager *TheGhostObjectManager;
