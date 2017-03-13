/*
================================================================================
This file is part of SaberMod - Star Wars Jedi Knight II: Jedi Outcast mod.

Copyright (C) 2015-2017 Witold Pilat <witold.pilat@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
================================================================================
*/

#include "g_local.h"

/*
=================
G_BlameForEntity

Set blameEntityNum and snapshotIgnore fields for all clients.
=================
*/
void G_BlameForEntity( int blame, gentity_t *ent )
{
	assert(blame >= 0 && blame < MAX_GENTITIES);

	if (blame >= MAX_CLIENTS && blame < ENTITYNUM_MAX_NORMAL) {
		blame = g_entities[blame].blameEntityNum;
		assert(blame < MAX_CLIENTS || blame == ENTITYNUM_NONE || blame == ENTITYNUM_WORLD);
	}

	ent->blameEntityNum = blame;
	// For now cgame can ignore only dueling players
	if (ent - g_entities < MAX_CLIENTS) {
		ent->dimension = DEFAULT_DIMENSION;
	} else {
		ent->dimension = ALL_DIMENSIONS;
	}

	if (mvapi) {
		uint8_t	*snapshotIgnore = mv_entities[ent->s.number].snapshotIgnore;
		int		i;

		if (blame == ENTITYNUM_WORLD) {
			for (i = 0; i < level.maxclients; i++) {
				snapshotIgnore[i] = 0;
			}
		} else {
			for (i = 0; i < level.maxclients; i++) {
				gclient_t	*client = level.clients + i;

				if (client->pers.connected == CON_CONNECTED &&
					client->pers.privateDuel && client->ps.duelInProgress &&
					blame != i && blame != client->ps.duelIndex) {
					snapshotIgnore[i] = 1;
				} else {
					snapshotIgnore[i] = 0;
				}
			}
		}
	}
}

unsigned G_GetFreeDuelDimension(void)
{
	unsigned dimension;
	qboolean free;
	int i;

	for (dimension = 1 << 15; dimension != 0; dimension <<= 1) {
		free = qtrue;

		for (i = 0; i < level.maxclients; i++) {
			if (!g_entities[i].inuse) {
				continue;
			}
			if ((g_entities[i].dimension & dimension) != 0) {
				free = qfalse;
				break;
			}
		}

		if (free) {
			return dimension;
		}
	}

	assert(0); // didn't find a free dimension
	return DEFAULT_DIMENSION;
}

unsigned G_EntitiesCollide(gentity_t *ent1, gentity_t *ent2)
{
	unsigned common = ent1->dimension & ent2->dimension;
#ifndef NDEBUG
	qboolean	collision;
	gclient_t	*client1 = ent1->client;
	gclient_t	*client2 = ent2->client;

	// cgame collision test has to follow the same logic
	if (client1 && client1->pers.connected == CON_CONNECTED &&
		client2 && client2->pers.connected == CON_CONNECTED)
	{
		if (client1 == client2) {
			// what would happen with dimensionless client?
			collision = qtrue;
		} else if (client1->ps.duelInProgress) {
			if (client1->ps.duelIndex == ent2->s.number) {
				collision = qtrue;
			} else {
				collision = qfalse;
			}
		} else {
			if (client2->ps.duelInProgress) {
				collision = qfalse;
			} else {
				collision = qtrue;
			}
		}

		assert(!!common == collision);
	}
#endif
	return common;
}

void G_StartPrivateDuel(gentity_t *ent)
{
	if (mvapi) {
		int	clientNum = ent->s.number;
		int opponentNum;
		int	i;

		if (!ent->client->pers.privateDuel) {
			return;
		}
		if (!ent->client->ps.duelInProgress) {
			return;
		}

		opponentNum = ent->client->ps.duelIndex;

		for (i = 0; i < level.num_entities; i++) {
			if (g_entities[i].inuse) {
				int blame = g_entities[i].blameEntityNum;

				if (blame == ENTITYNUM_WORLD || blame == clientNum || blame == opponentNum) {
					mv_entities[i].snapshotIgnore[clientNum] = 0;
				} else {
					mv_entities[i].snapshotIgnore[clientNum] = 1;
				}
			}
		}
	}
}

void G_StopPrivateDuel(gentity_t *ent)
{
	if (mvapi) {
		int clientNum = ent->s.number;
		int i;

		for (i = 0; i < level.num_entities; i++) {
			mv_entities[i].snapshotIgnore[clientNum] = 0;
		}
	}
}
