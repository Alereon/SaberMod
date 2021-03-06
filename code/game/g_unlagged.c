/*
================================================================================
This file is part of SaberMod - Star Wars Jedi Knight II: Jedi Outcast mod.

Copyright (C) 2017 Witold Pilat <witold.pilat@gmail.com>

This program is free software; you can redistribute it and/or modify it
under the terms and conditions of the GNU General Public License,
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
================================================================================
*/

// g_unlagged.c -- "unlagged" serverside backward reconciliation technique

#include "g_local.h"

#define MAX_STATES	32
#define STATE_MASK	31

typedef struct {
	trajectory_t	pos;
	trajectory_t	apos;
	int				groundEntityNum;
} savedEntityState_t;

typedef struct {
	entityShared_t		r;
	savedEntityState_t	s;
} savedGentity_t;

typedef struct {
	int				time;
	int				num_entities;
	savedGentity_t	entities[MAX_GENTITIES];
} savedState_t;

static savedState_t		states[MAX_STATES];
static int				stateNum;

static entityShared_t	savedEntities[MAX_GENTITIES];
static qboolean			saved[MAX_GENTITIES];
static int				savedNum;

/*
================
G_BackupWorld
================
*/
void G_BackupWorld( void ) {
	savedState_t	*state = &states[stateNum & STATE_MASK];
	int				num_entities;
	int				i;

	if (g_unlagged.integer == 1) {
		num_entities = level.maxclients;
	} else {
		num_entities = level.num_entities;
	}

	stateNum++;
	state->time = level.time;
	state->num_entities = num_entities;

	for ( i = 0; i < num_entities; i++ ) {
		gentity_t		*ent = &g_entities[i];
		savedGentity_t	*savedEnt = &state->entities[i];

		if (ent->r.linked) {
			savedEnt->r = ent->r;
			savedEnt->s.pos = ent->s.pos;
			savedEnt->s.apos = ent->s.apos;
			savedEnt->s.groundEntityNum = ent->s.groundEntityNum;
		} else {
			savedEnt->r.linked = qfalse;
		}
	}
}


/*
================
G_RollbackWorld

Any changes made to gentity_t::entityShared_t in-between
G_RollbackWord and G_RestoreWorld calls will be lost.

Resulting world may contain entities that are linked but not inuse so
always check the later.
================
*/
void G_RollbackWorld( int serverTime, int contents ) {
	const savedState_t	*state;
	const savedState_t	*nextState;
	int					i;
	int					stateTail = stateNum & STATE_MASK;

	if ( serverTime < level.time - g_unlaggedMaxPing.integer ) {
		serverTime = level.time - g_unlaggedMaxPing.integer;
	}

	//
	// find snapshots client was interpolating between
	//

	i = stateTail;

	do {
		i = (i - 1) & STATE_MASK;

		// so we can assume that freed entities haven't been reused
		if ( states[i].time + 1000 <= level.time ) {
			i = (i + 1) & STATE_MASK;
			break;
		}

		// good one
		if ( states[i].time <= serverTime ) {
			break;
		}
	} while ( i != stateTail );

	if ( i == stateTail ) {
		savedNum = 0;
		return;
	}

	state = &states[i];
	i = (i + 1) & STATE_MASK;
	nextState = &states[i];

	if ( nextState->time <= serverTime ) {
		// no interpolating state
		nextState = NULL;
	}

	//
	// rollback entities
	//

	// assume num_entities can only increase
	for ( i = 0; i < state->num_entities; i++ ) {
		gentity_t				*currEnt = &g_entities[i];
		const savedGentity_t	*ent = &state->entities[i];

		// assume entityShared_t::linked implies gentity_t::inuse

		// If both are linked, they must be the same entity due to
		// time difference being less than 1000.
		if ((ent->r.contents & contents) &&
			(currEnt->r.linked || ent->r.linked) &&
			!ent->r.mIsRoffing)
		{
			savedEntities[i] = currEnt->r;
			saved[i] = qtrue;

			// wasn't on player's screen one way or another
			if (!ent->r.linked) {
				trap_UnlinkEntity( currEnt );
				continue;
			}

			currEnt->r = ent->r;

			// interpolate position as in CG_CalcEntityLerpPositions
			if ( nextState && (ent->s.pos.trType == TR_INTERPOLATE || i < MAX_CLIENTS) )
			{
				const savedGentity_t	*nextEnt = &nextState->entities[i];
				vec3_t					current, next;
				float					f;

				BG_EvaluateTrajectory( &ent->s.pos, state->time, current );
				BG_EvaluateTrajectory( &nextEnt->s.pos, nextState->time, next );

				f = (float)(serverTime - state->time) / (nextState->time - state->time);

				currEnt->r.currentOrigin[0] = current[0] + f * ( next[0] - current[0] );
				currEnt->r.currentOrigin[1] = current[1] + f * ( next[1] - current[1] );
				currEnt->r.currentOrigin[2] = current[2] + f * ( next[2] - current[2] );

				// assume that ent->s.modelindex doesn't change
				if ( ent->r.bmodel ) {
					BG_EvaluateTrajectory( &ent->s.apos, state->time, current );
					BG_EvaluateTrajectory( &nextEnt->s.apos, nextState->time, next );

					currEnt->r.currentAngles[0] = LerpAngle( current[0], next[0], f );
					currEnt->r.currentAngles[1] = LerpAngle( current[1], next[1], f );
					currEnt->r.currentAngles[2] = LerpAngle( current[2], next[2], f );
				}
			}
			else
			{
				BG_EvaluateTrajectory( &ent->s.pos, serverTime, currEnt->r.currentOrigin );

				if ( ent->r.bmodel ) {
					BG_EvaluateTrajectory( &ent->s.apos, serverTime, currEnt->r.currentAngles );
				}
			}

			// adjust position for mover
			if ( 0 <= ent->s.groundEntityNum && ent->s.groundEntityNum < ENTITYNUM_MAX_NORMAL ) {
				const entityState_t	*mover = &g_entities[ent->s.groundEntityNum].s;

				if ( mover->eType == ET_MOVER ) {
					BG_AdjustPositionForMover( currEnt->r.currentOrigin, mover,
						state->time, serverTime, currEnt->r.currentOrigin );
				}
			}

			// TODO: Prepare other fields - roffing

			trap_LinkEntity( currEnt );
		} else {
			saved[i] = qfalse;
		}
	}

	// remaining saved items are invalid
	savedNum = level.num_entities;
}

/*
================
G_RestoreWorld
================
*/
void G_RestoreWorld( void ) {
	int		i;

	for ( i = 0; i < savedNum; i++ ) {
		gentity_t	*ent = &g_entities[i];

		if ( saved[i] ) {
			ent->r = savedEntities[i];

			if ( ent->r.linked ) {
				trap_LinkEntity( ent );
			} else {
				trap_UnlinkEntity( ent );
			}
		}
	}
}
