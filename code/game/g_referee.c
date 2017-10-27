/*
================================================================================
This file is part of SaberMod - Star Wars Jedi Knight II: Jedi Outcast mod.

Copyright (C) 2015-2017 Witold Pilat <witold.pilat@gmail.com>

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

#include "g_local.h"

// g_referee.c -- exclusive referee commands

refCmdContext_t	ref;

void Ref_Referee_f(void) {
	const char	*errorMsg;
	gclient_t	*client;
	int			targetNum;
	char		arg[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2) {
		ref.Printf("Usage: referee <player>\n");
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	targetNum = G_ClientNumberFromString(arg, &errorMsg);
	if (targetNum == -1) {
		ref.Printf("%s", errorMsg);
		return;
	}

	client = &level.clients[targetNum];

	if (!client->sess.referee) {
		client->sess.referee = qtrue;
		ClientUpdateConfigString(targetNum);
		G_SendServerCommand(-1, "print \"%s" S_COLOR_WHITE " became a referee\n\"",
			client->info.netname);
		ref.LogPrintf(LOG_REFEREE, "Referee: %d: %s became a referee\n",
			targetNum, client->info.netname);
	}
}

void Ref_UnReferee_f(void) {
	const char	*errorMsg;
	gclient_t	*client;
	int			targetNum;
	char		arg[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2) {
		ref.Printf("Usage: unreferee <player>\n");
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	targetNum = G_ClientNumberFromString(arg, &errorMsg);
	if (targetNum == -1) {
		ref.Printf("%s", errorMsg);
		return;
	}

	client = &level.clients[targetNum];

	if (client->sess.referee) {
		client->sess.referee = qfalse;
		ClientUpdateConfigString(targetNum);
		G_SendServerCommand(-1, "print \"%s" S_COLOR_WHITE " is not a referee anymore\n\"",
			client->info.netname);
		ref.LogPrintf(LOG_REFEREE, "UnReferee: %d: %s is not a referee anymore\n",
			targetNum, client->info.netname);
	}
}

void Ref_LockTeam_f(void) {
	char		str[MAX_TOKEN_CHARS];
	team_t		team;
	int			argc = trap_Argc();
	int			i;

	trap_Argv(0, str, sizeof(str));

	if (argc < 2) {
		ref.Printf("Usage: lockteam <teams>\n");
		return;
	}

	for (i = 1; i < argc; i++) {
		trap_Argv( i, str, sizeof( str ) );

		team = BG_TeamFromString( str );
		if ( team == TEAM_NUM_TEAMS ) {
			return;
		}

		if (level.teamLock[team] != qtrue) {
			level.teamLock[team] = qtrue;
			G_SendServerCommand(-1, "print \"%s%s" S_COLOR_WHITE " team was locked.\n\"",
				BG_TeamColor(team), BG_TeamName(team, CASE_NORMAL));
			ref.LogPrintf(LOG_REFEREE, "LockTeam: %s\n", BG_TeamName(team, CASE_UPPER));
		}
	}
}

void Ref_UnLockTeam_f(void) {
	char		str[MAX_TOKEN_CHARS];
	team_t		team;
	int			argc = trap_Argc();
	int			i;

	trap_Argv(0, str, sizeof(str));

	if (argc < 2) {
		ref.Printf("Usage: unlockteam <teams>\n");
		return;
	}

	for (i = 1; i < argc; i++) {
		trap_Argv( i, str, sizeof( str ) );

		team = BG_TeamFromString( str );
		if ( team == TEAM_NUM_TEAMS ) {
			return;
		}

		if (level.teamLock[team] != qfalse) {
			level.teamLock[team] = qfalse;
			G_SendServerCommand(-1, "print \"%s%s" S_COLOR_WHITE " team was unlocked.\n\"",
				BG_TeamColor(team), BG_TeamName(team, CASE_NORMAL));
			ref.LogPrintf(LOG_REFEREE, "UnLockTeam: %s\n", BG_TeamName(team, CASE_UPPER));
		}
	}
}

void Ref_ForceTeam_f(void) {
	char		str[MAX_TOKEN_CHARS];
	const char	*errorMsg;
	int			clientNum;
	int			lastClient;
	team_t		team;

	if (trap_Argc() < 3) {
		ref.Printf(
			"Usage: forceteam <player> <team>\n"
			"       forceteam all <team>\n");
		return;
	}
	// find the player
	trap_Argv(1, str, sizeof(str));
	if (!strcmp(str, "all")) {
		clientNum = 0;
		lastClient = level.maxclients - 1;
	} else {
		clientNum = G_ClientNumberFromString(str, &errorMsg);
		if (clientNum == -1) {
			ref.Printf("%s", errorMsg);
			return;
		}
		lastClient = clientNum;
	}

	// set the team
	trap_Argv(2, str, sizeof(str));
	team = BG_TeamFromString(str);
	if (team == TEAM_NUM_TEAMS) {
		return;
	}

	if (clientNum == lastClient) {
		ref.LogPrintf(LOG_REFEREE, "ForceTeam: %d %s\n",
			clientNum, BG_TeamName(team, CASE_UPPER));
	} else {
		ref.LogPrintf(LOG_REFEREE, "ForceTeam: ALL %s\n",
			BG_TeamName(team, CASE_UPPER));
	}

	for (; clientNum <= lastClient; clientNum++) {
		gentity_t *targEnt = &g_entities[clientNum];

		if (targEnt->inuse) {
			SetTeam( targEnt, team );
			targEnt->client->prof.switchTeamTime = level.time + 5000;
		}
	}
}

void Ref_Announce_f(void) {
	char	*str = ConcatArgs(1);

	if (!str[0]) {
		ref.Printf("Usage: announce <message|motd>\n");
		return;
	}

	if (!Q_stricmp(str, "motd")) {
		trap_SendServerCommand(-1, "motd");
	} else {
		G_CenterPrintPersistant(Q_SanitizeStr(str));
	}

	ref.LogPrintf(LOG_REFEREE, "Announce: %s\n", str);
}

void Ref_Help_f(void);

typedef struct {
	const char	*name;				// must be lower-case for comparing
	void		(*function)(void);
} refereeCommand_t;

static const refereeCommand_t refCommands[] = {
	{ "referee", Ref_Referee_f },
	{ "unreferee", Ref_UnReferee_f },
	{ "lockteam", Ref_LockTeam_f },
	{ "unlockteam", Ref_UnLockTeam_f },
	{ "forceteam", Ref_ForceTeam_f },
	{ "announce", Ref_Announce_f },
	{ "help", Ref_Help_f },
};

void Ref_Help_f(void) {
	const int	columns = 3;
	const char	*fmt = "%-26s";
	char		line[DEFAULT_CONSOLE_WIDTH + 1];
	int			i;

	ref.Printf("Referee commands:\n");

	line[0] = '\0';
	for (i = CV_FIRST; i < (int)ARRAY_LEN(refCommands); i++) {
		Q_strcat(line, sizeof(line), va(fmt, refCommands[i].name));

		if (i % columns == 0) {
			ref.Printf("%s\n", line);
			line[0] = '\0';
		}
	}

	if (line[0]) {
		ref.Printf("%s\n", line);
	}
}

qboolean RefereeCommand(const char *cmd) {
	int	i;

	for (i = 0; i < (int)ARRAY_LEN(refCommands); i++) {
		if (!strcmp(cmd, refCommands[i].name)) {
			refCommands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}
