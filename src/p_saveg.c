// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2022 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_saveg.c
/// \brief Archiving: SaveGame I/O

#include "doomdef.h"
#include "byteptr.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_random.h"
#include "m_misc.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "r_data.h"
#include "r_textures.h"
#include "r_things.h"
#include "r_skins.h"
#include "r_state.h"
#include "w_wad.h"
#include "y_inter.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_sky.h"
#include "p_polyobj.h"
#include "lua_script.h"
#include "p_setup.h"
#include "p_slopes.h"
#include "hashtable.h" //I don't want to use hashtables there, but there will be anyways in the future

savedata_t savedata;
UINT8 *save_p;

// Block UINT32s to attempt to ensure that the correct data is
// being sent and received
#define ARCHIVEBLOCK_MISC     0x7FEEDEED
#define ARCHIVEBLOCK_PLAYERS  0x7F448008
#define ARCHIVEBLOCK_WORLD    0x7F8C08C0
#define ARCHIVEBLOCK_POBJS    0x7F928546
#define ARCHIVEBLOCK_THINKERS 0x7F37037C
#define ARCHIVEBLOCK_SPECIALS 0x7F228378

// boolean memleak = false;

// Note: This cannot be bigger
// than an UINT16
typedef enum
{
//	RFLAGPOINT = 0x01,
//	BFLAGPOINT = 0x02,
	CAPSULE    = 0x04,
	AWAYVIEW   = 0x08,
	FIRSTAXIS  = 0x10,
	SECONDAXIS = 0x20,
	FOLLOW     = 0x40,
	DRONE      = 0x80,
} player_saveflags;

static inline void P_ArchivePlayer(void)
{
	const player_t *player = &players[consoleplayer];
	SINT8 pllives = player->lives;
	if (pllives < startinglivesbalance[numgameovers]) // Bump up to 3 lives if the player
		pllives = startinglivesbalance[numgameovers]; // has less than that.

#ifdef NEWSKINSAVES
	// Write a specific value into the old skininfo location.
	// If we read something other than this, it's an older save file that used skin numbers.
	WRITEUINT16(save_p, NEWSKINSAVES);
#endif

	// Write skin names, so that loading skins in different orders
	// doesn't change who the save file is for!
	WRITESTRINGN(save_p, skins[player->skin].name, SKINNAMESIZE);

	if (botskin != 0)
	{
		WRITESTRINGN(save_p, skins[botskin-1].name, SKINNAMESIZE);
	}
	else
	{
		WRITESTRINGN(save_p, "\0", SKINNAMESIZE);
	}

	WRITEUINT8(save_p, numgameovers);
	WRITESINT8(save_p, pllives);
	WRITEUINT32(save_p, player->score);
	WRITEINT32(save_p, player->continues);
}

static inline void P_UnArchivePlayer(void)
{
#ifdef NEWSKINSAVES
	INT16 backwardsCompat = READUINT16(save_p);

	if (backwardsCompat != NEWSKINSAVES)
	{
		// This is an older save file, which used direct skin numbers.
		savedata.skin = backwardsCompat & ((1<<5) - 1);
		savedata.botskin = backwardsCompat >> 5;
	}
	else
#endif
	{
		char ourSkinName[SKINNAMESIZE+1];
		char botSkinName[SKINNAMESIZE+1];

		READSTRINGN(save_p, ourSkinName, SKINNAMESIZE);
		savedata.skin = R_SkinAvailable(ourSkinName);

		READSTRINGN(save_p, botSkinName, SKINNAMESIZE);
		savedata.botskin = R_SkinAvailable(botSkinName) + 1;
	}

	savedata.numgameovers = READUINT8(save_p);
	savedata.lives = READSINT8(save_p);
	savedata.score = READUINT32(save_p);
	savedata.continues = READINT32(save_p);
}

static void P_NetArchivePlayers(void)
{
	INT32 i, j;
	UINT16 flags;
//	size_t q;

	WRITEUINT32(save_p, ARCHIVEBLOCK_PLAYERS);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITESINT8(save_p, (SINT8)adminplayers[i]);

		if (!playeringame[i])
			continue;

		flags = 0;

		// no longer send ticcmds

		WRITESTRINGN(save_p, player_names[i], MAXPLAYERNAME);
		WRITEINT16(save_p, players[i].angleturn);
		WRITEINT16(save_p, players[i].oldrelangleturn);
		WRITEANGLE(save_p, players[i].aiming);
		WRITEANGLE(save_p, players[i].drawangle);
		WRITEANGLE(save_p, players[i].viewrollangle);
		WRITEANGLE(save_p, players[i].awayviewaiming);
		WRITEINT32(save_p, players[i].awayviewtics);
		WRITEINT16(save_p, players[i].rings);
		WRITEINT16(save_p, players[i].spheres);

		WRITESINT8(save_p, players[i].pity);
		WRITEINT32(save_p, players[i].currentweapon);
		WRITEINT32(save_p, players[i].ringweapons);

		WRITEUINT16(save_p, players[i].ammoremoval);
		WRITEUINT32(save_p, players[i].ammoremovaltimer);
		WRITEINT32(save_p, players[i].ammoremovaltimer);

		for (j = 0; j < NUMPOWERS; j++)
			WRITEUINT16(save_p, players[i].powers[j]);

		WRITEUINT8(save_p, players[i].playerstate);
		WRITEUINT32(save_p, players[i].pflags);
		WRITEUINT8(save_p, players[i].panim);
		WRITEUINT8(save_p, players[i].spectator);

		WRITEUINT16(save_p, players[i].flashpal);
		WRITEUINT16(save_p, players[i].flashcount);

		WRITEUINT8(save_p, players[i].skincolor);
		WRITEINT32(save_p, players[i].skin);
		WRITEUINT32(save_p, players[i].availabilities);
		WRITEUINT32(save_p, players[i].score);
		WRITEFIXED(save_p, players[i].dashspeed);
		WRITESINT8(save_p, players[i].lives);
		WRITESINT8(save_p, players[i].continues);
		WRITESINT8(save_p, players[i].xtralife);
		WRITEUINT8(save_p, players[i].gotcontinue);
		WRITEFIXED(save_p, players[i].speed);
		WRITEUINT8(save_p, players[i].secondjump);
		WRITEUINT8(save_p, players[i].fly1);
		WRITEUINT8(save_p, players[i].scoreadd);
		WRITEUINT32(save_p, players[i].glidetime);
		WRITEUINT8(save_p, players[i].climbing);
		WRITEINT32(save_p, players[i].deadtimer);
		WRITEUINT32(save_p, players[i].exiting);
		WRITEUINT8(save_p, players[i].homing);
		WRITEUINT32(save_p, players[i].dashmode);
		WRITEUINT32(save_p, players[i].skidtime);

		//////////
		// Bots //
		//////////
		WRITEUINT8(save_p, players[i].bot);
		WRITEUINT8(save_p, players[i].botmem.lastForward);
		WRITEUINT8(save_p, players[i].botmem.lastBlocked);
		WRITEUINT8(save_p, players[i].botmem.catchup_tics);
		WRITEUINT8(save_p, players[i].botmem.thinkstate);
		WRITEUINT8(save_p, players[i].removing);
		
		WRITEUINT8(save_p, players[i].blocked);
		WRITEUINT16(save_p, players[i].lastbuttons);

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		WRITEFIXED(save_p, players[i].cmomx); // Conveyor momx
		WRITEFIXED(save_p, players[i].cmomy); // Conveyor momy
		WRITEFIXED(save_p, players[i].rmomx); // "Real" momx (momx - cmomx)
		WRITEFIXED(save_p, players[i].rmomy); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		WRITEINT16(save_p, players[i].numboxes);
		WRITEINT16(save_p, players[i].totalring);
		WRITEUINT32(save_p, players[i].realtime);
		WRITEUINT8(save_p, players[i].laps);

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		WRITEINT32(save_p, players[i].ctfteam);
		WRITEUINT16(save_p, players[i].gotflag);

		WRITEINT32(save_p, players[i].weapondelay);
		WRITEINT32(save_p, players[i].tossdelay);

		WRITEUINT32(save_p, players[i].starposttime);
		WRITEINT16(save_p, players[i].starpostx);
		WRITEINT16(save_p, players[i].starposty);
		WRITEINT16(save_p, players[i].starpostz);
		WRITEINT32(save_p, players[i].starpostnum);
		WRITEANGLE(save_p, players[i].starpostangle);
		WRITEFIXED(save_p, players[i].starpostscale);

		WRITEANGLE(save_p, players[i].angle_pos);
		WRITEANGLE(save_p, players[i].old_angle_pos);

		WRITEINT32(save_p, players[i].flyangle);
		WRITEUINT32(save_p, players[i].drilltimer);
		WRITEINT32(save_p, players[i].linkcount);
		WRITEUINT32(save_p, players[i].linktimer);
		WRITEINT32(save_p, players[i].anotherflyangle);
		WRITEUINT32(save_p, players[i].nightstime);
		WRITEUINT32(save_p, players[i].bumpertime);
		WRITEINT32(save_p, players[i].drillmeter);
		WRITEUINT8(save_p, players[i].drilldelay);
		WRITEUINT8(save_p, players[i].bonustime);
		WRITEFIXED(save_p, players[i].oldscale);
		WRITEUINT8(save_p, players[i].mare);
		WRITEUINT8(save_p, players[i].marelap);
		WRITEUINT8(save_p, players[i].marebonuslap);
		WRITEUINT32(save_p, players[i].marebegunat);
		WRITEUINT32(save_p, players[i].startedtime);
		WRITEUINT32(save_p, players[i].finishedtime);
		WRITEUINT32(save_p, players[i].lapbegunat);
		WRITEUINT32(save_p, players[i].lapstartedtime);
		WRITEINT16(save_p, players[i].finishedspheres);
		WRITEINT16(save_p, players[i].finishedrings);
		WRITEUINT32(save_p, players[i].marescore);
		WRITEUINT32(save_p, players[i].lastmarescore);
		WRITEUINT32(save_p, players[i].totalmarescore);
		WRITEUINT8(save_p, players[i].lastmare);
		WRITEUINT8(save_p, players[i].lastmarelap);
		WRITEUINT8(save_p, players[i].lastmarebonuslap);
		WRITEUINT8(save_p, players[i].totalmarelap);
		WRITEUINT8(save_p, players[i].totalmarebonuslap);
		WRITEINT32(save_p, players[i].maxlink);
		WRITEUINT8(save_p, players[i].texttimer);
		WRITEUINT8(save_p, players[i].textvar);

		if (players[i].capsule)
			flags |= CAPSULE;

		if (players[i].awayviewmobj)
			flags |= AWAYVIEW;

		if (players[i].axis1)
			flags |= FIRSTAXIS;

		if (players[i].axis2)
			flags |= SECONDAXIS;

		if (players[i].followmobj)
			flags |= FOLLOW;

		if (players[i].drone)
			flags |= DRONE;

		WRITEINT16(save_p, players[i].lastsidehit);
		WRITEINT16(save_p, players[i].lastlinehit);

		WRITEUINT32(save_p, players[i].losstime);

		WRITEUINT8(save_p, players[i].timeshit);

		WRITEINT32(save_p, players[i].onconveyor);

		WRITEUINT32(save_p, players[i].jointime);
		WRITEUINT32(save_p, players[i].quittime);

		WRITEUINT16(save_p, flags);

		if (flags & CAPSULE)
			WRITEUINT32(save_p, players[i].capsule->mobjnum);

		if (flags & FIRSTAXIS)
			WRITEUINT32(save_p, players[i].axis1->mobjnum);

		if (flags & SECONDAXIS)
			WRITEUINT32(save_p, players[i].axis2->mobjnum);

		if (flags & AWAYVIEW)
			WRITEUINT32(save_p, players[i].awayviewmobj->mobjnum);

		if (flags & FOLLOW)
			WRITEUINT32(save_p, players[i].followmobj->mobjnum);

		if (flags & DRONE)
			WRITEUINT32(save_p, players[i].drone->mobjnum);

		WRITEFIXED(save_p, players[i].camerascale);
		WRITEFIXED(save_p, players[i].shieldscale);

		WRITEUINT8(save_p, players[i].charability);
		WRITEUINT8(save_p, players[i].charability2);
		WRITEUINT32(save_p, players[i].charflags);
		WRITEUINT32(save_p, (UINT32)players[i].thokitem);
		WRITEUINT32(save_p, (UINT32)players[i].spinitem);
		WRITEUINT32(save_p, (UINT32)players[i].revitem);
		WRITEUINT32(save_p, (UINT32)players[i].followitem);
		WRITEFIXED(save_p, players[i].actionspd);
		WRITEFIXED(save_p, players[i].mindash);
		WRITEFIXED(save_p, players[i].maxdash);
		WRITEFIXED(save_p, players[i].normalspeed);
		WRITEFIXED(save_p, players[i].runspeed);
		WRITEUINT8(save_p, players[i].thrustfactor);
		WRITEUINT8(save_p, players[i].accelstart);
		WRITEUINT8(save_p, players[i].acceleration);
		WRITEFIXED(save_p, players[i].jumpfactor);
		WRITEFIXED(save_p, players[i].height);
		WRITEFIXED(save_p, players[i].spinheight);
	}
}

static void P_NetUnArchivePlayers(void)
{
	INT32 i, j;
	UINT16 flags;

	if (READUINT32(save_p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad $$$.sav at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		adminplayers[i] = (INT32)READSINT8(save_p);

		// Do NOT memset player struct to 0
		// other areas may initialize data elsewhere
		//memset(&players[i], 0, sizeof (player_t));
		if (!playeringame[i])
			continue;

		// NOTE: sending tics should (hopefully) no longer be necessary

		READSTRINGN(save_p, player_names[i], MAXPLAYERNAME);
		players[i].angleturn = READINT16(save_p);
		players[i].oldrelangleturn = READINT16(save_p);
		players[i].aiming = READANGLE(save_p);
		players[i].drawangle = READANGLE(save_p);
		players[i].viewrollangle = READANGLE(save_p);
		players[i].awayviewaiming = READANGLE(save_p);
		players[i].awayviewtics = READINT32(save_p);
		players[i].rings = READINT16(save_p);
		players[i].spheres = READINT16(save_p);

		players[i].pity = READSINT8(save_p);
		players[i].currentweapon = READINT32(save_p);
		players[i].ringweapons = READINT32(save_p);

		players[i].ammoremoval = READUINT16(save_p);
		players[i].ammoremovaltimer = READUINT32(save_p);
		players[i].ammoremovalweapon = READINT32(save_p);

		for (j = 0; j < NUMPOWERS; j++)
			players[i].powers[j] = READUINT16(save_p);

		players[i].playerstate = READUINT8(save_p);
		players[i].pflags = READUINT32(save_p);
		players[i].panim = READUINT8(save_p);
		players[i].spectator = READUINT8(save_p);

		players[i].flashpal = READUINT16(save_p);
		players[i].flashcount = READUINT16(save_p);

		players[i].skincolor = READUINT8(save_p);
		players[i].skin = READINT32(save_p);
		players[i].availabilities = READUINT32(save_p);
		players[i].score = READUINT32(save_p);
		players[i].dashspeed = READFIXED(save_p); // dashing speed
		players[i].lives = READSINT8(save_p);
		players[i].continues = READSINT8(save_p); // continues that player has acquired
		players[i].xtralife = READSINT8(save_p); // Ring Extra Life counter
		players[i].gotcontinue = READUINT8(save_p); // got continue from stage
		players[i].speed = READFIXED(save_p); // Player's speed (distance formula of MOMX and MOMY values)
		players[i].secondjump = READUINT8(save_p);
		players[i].fly1 = READUINT8(save_p); // Tails flying
		players[i].scoreadd = READUINT8(save_p); // Used for multiple enemy attack bonus
		players[i].glidetime = READUINT32(save_p); // Glide counter for thrust
		players[i].climbing = READUINT8(save_p); // Climbing on the wall
		players[i].deadtimer = READINT32(save_p); // End game if game over lasts too long
		players[i].exiting = READUINT32(save_p); // Exitlevel timer
		players[i].homing = READUINT8(save_p); // Are you homing?
		players[i].dashmode = READUINT32(save_p); // counter for dashmode ability
		players[i].skidtime = READUINT32(save_p); // Skid timer

		//////////
		// Bots //
		//////////
		players[i].bot = READUINT8(save_p);
		
		players[i].botmem.lastForward = READUINT8(save_p);
		players[i].botmem.lastBlocked = READUINT8(save_p);
		players[i].botmem.catchup_tics = READUINT8(save_p);
		players[i].botmem.thinkstate = READUINT8(save_p);
		players[i].removing = READUINT8(save_p);

		players[i].blocked = READUINT8(save_p);
		players[i].lastbuttons = READUINT16(save_p);
		
		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		players[i].cmomx = READFIXED(save_p); // Conveyor momx
		players[i].cmomy = READFIXED(save_p); // Conveyor momy
		players[i].rmomx = READFIXED(save_p); // "Real" momx (momx - cmomx)
		players[i].rmomy = READFIXED(save_p); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		players[i].numboxes = READINT16(save_p); // Number of item boxes obtained for Race Mode
		players[i].totalring = READINT16(save_p); // Total number of rings obtained for Race Mode
		players[i].realtime = READUINT32(save_p); // integer replacement for leveltime
		players[i].laps = READUINT8(save_p); // Number of laps (optional)

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		players[i].ctfteam = READINT32(save_p); // 1 == Red, 2 == Blue
		players[i].gotflag = READUINT16(save_p); // 1 == Red, 2 == Blue Do you have the flag?

		players[i].weapondelay = READINT32(save_p);
		players[i].tossdelay = READINT32(save_p);

		players[i].starposttime = READUINT32(save_p);
		players[i].starpostx = READINT16(save_p);
		players[i].starposty = READINT16(save_p);
		players[i].starpostz = READINT16(save_p);
		players[i].starpostnum = READINT32(save_p);
		players[i].starpostangle = READANGLE(save_p);
		players[i].starpostscale = READFIXED(save_p);

		players[i].angle_pos = READANGLE(save_p);
		players[i].old_angle_pos = READANGLE(save_p);

		players[i].flyangle = READINT32(save_p);
		players[i].drilltimer = READUINT32(save_p);
		players[i].linkcount = READINT32(save_p);
		players[i].linktimer = READUINT32(save_p);
		players[i].anotherflyangle = READINT32(save_p);
		players[i].nightstime = READUINT32(save_p);
		players[i].bumpertime = READUINT32(save_p);
		players[i].drillmeter = READINT32(save_p);
		players[i].drilldelay = READUINT8(save_p);
		players[i].bonustime = (boolean)READUINT8(save_p);
		players[i].oldscale = READFIXED(save_p);
		players[i].mare = READUINT8(save_p);
		players[i].marelap = READUINT8(save_p);
		players[i].marebonuslap = READUINT8(save_p);
		players[i].marebegunat = READUINT32(save_p);
		players[i].startedtime = READUINT32(save_p);
		players[i].finishedtime = READUINT32(save_p);
		players[i].lapbegunat = READUINT32(save_p);
		players[i].lapstartedtime = READUINT32(save_p);
		players[i].finishedspheres = READINT16(save_p);
		players[i].finishedrings = READINT16(save_p);
		players[i].marescore = READUINT32(save_p);
		players[i].lastmarescore = READUINT32(save_p);
		players[i].totalmarescore = READUINT32(save_p);
		players[i].lastmare = READUINT8(save_p);
		players[i].lastmarelap = READUINT8(save_p);
		players[i].lastmarebonuslap = READUINT8(save_p);
		players[i].totalmarelap = READUINT8(save_p);
		players[i].totalmarebonuslap = READUINT8(save_p);
		players[i].maxlink = READINT32(save_p);
		players[i].texttimer = READUINT8(save_p);
		players[i].textvar = READUINT8(save_p);

		players[i].lastsidehit = READINT16(save_p);
		players[i].lastlinehit = READINT16(save_p);

		players[i].losstime = READUINT32(save_p);

		players[i].timeshit = READUINT8(save_p);

		players[i].onconveyor = READINT32(save_p);

		players[i].jointime = READUINT32(save_p);
		players[i].quittime = READUINT32(save_p);

		flags = READUINT16(save_p);

		if (flags & CAPSULE)
			players[i].capsule = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & FIRSTAXIS)
			players[i].axis1 = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & SECONDAXIS)
			players[i].axis2 = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & AWAYVIEW)
			players[i].awayviewmobj = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & FOLLOW)
			players[i].followmobj = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & DRONE)
			players[i].drone = (mobj_t *)(size_t)READUINT32(save_p);

		players[i].camerascale = READFIXED(save_p);
		players[i].shieldscale = READFIXED(save_p);

		//SetPlayerSkinByNum(i, players[i].skin);
		players[i].charability = READUINT8(save_p);
		players[i].charability2 = READUINT8(save_p);
		players[i].charflags = READUINT32(save_p);
		players[i].thokitem = (mobjtype_t)READUINT32(save_p);
		players[i].spinitem = (mobjtype_t)READUINT32(save_p);
		players[i].revitem = (mobjtype_t)READUINT32(save_p);
		players[i].followitem = (mobjtype_t)READUINT32(save_p);
		players[i].actionspd = READFIXED(save_p);
		players[i].mindash = READFIXED(save_p);
		players[i].maxdash = READFIXED(save_p);
		players[i].normalspeed = READFIXED(save_p);
		players[i].runspeed = READFIXED(save_p);
		players[i].thrustfactor = READUINT8(save_p);
		players[i].accelstart = READUINT8(save_p);
		players[i].acceleration = READUINT8(save_p);
		players[i].jumpfactor = READFIXED(save_p);
		players[i].height = READFIXED(save_p);
		players[i].spinheight = READFIXED(save_p);

		players[i].viewheight = 41*players[i].height/48; // scale cannot be factored in at this point
	}
}

static void P_LocalArchivePlayers(void)
{
	int i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_PLAYERS);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		WRITEMEM(save_p, &players[i], sizeof(player_t));

		player_t* player = &((player_t*)save_p)[-1];

#define RELINK(var) if (var) var = (mobj_t*)var->mobjnum
		RELINK(player->capsule);
		RELINK(player->axis1);
		RELINK(player->axis2);
		RELINK(player->awayviewmobj);
		RELINK(player->followmobj);
		RELINK(player->drone);
#undef RELINK
	}
}

static void P_LocalUnArchivePlayers(void)
{
	int i;

	if (READUINT32(save_p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad savestate at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		READMEM(save_p, &players[i], sizeof(player_t));
	}
}

///
/// Colormaps
///

static extracolormap_t *net_colormaps = NULL;
static UINT32 num_net_colormaps = 0;
static UINT32 num_ffloors = 0; // for loading

// Copypasta from r_data.c AddColormapToList
// But also check for equality and return the matching index
static UINT32 CheckAddNetColormapToList(extracolormap_t *extra_colormap)
{
	extracolormap_t *exc, *exc_prev = NULL;
	UINT32 i = 0;

	if (!net_colormaps)
	{
		net_colormaps = R_CopyColormap(extra_colormap, false);
		net_colormaps->next = 0;
		net_colormaps->prev = 0;
		num_net_colormaps = i+1;
		return i;
	}

	for (exc = net_colormaps; exc; exc_prev = exc, exc = exc->next)
	{
		if (R_CheckEqualColormaps(exc, extra_colormap, true, true, true))
			return i;
		i++;
	}

	exc_prev->next = R_CopyColormap(extra_colormap, false);
	extra_colormap->prev = exc_prev;
	extra_colormap->next = 0;

	num_net_colormaps = i+1;
	return i;
}

static extracolormap_t *GetNetColormapFromList(UINT32 index)
{
	// For loading, we have to be tricky:
	// We load the sectors BEFORE knowing the colormap values
	// So if an index doesn't exist, fill our list with dummy colormaps
	// until we get the index we want
	// Then when we load the color data, we set up the dummy colormaps

	extracolormap_t *exc, *last_exc = NULL;
	UINT32 i = 0;

	if (!net_colormaps) // initialize our list
		net_colormaps = R_CreateDefaultColormap(false);

	for (exc = net_colormaps; exc; last_exc = exc, exc = exc->next)
	{
		if (i++ == index)
			return exc;
	}


	// LET'S HOPE that index is a sane value, because we create up to [index]
	// entries in net_colormaps. At this point, we don't know
	// what the total colormap count is
	if (index >= numsectors*3 + num_ffloors)
		// if every sector had a unique colormap change AND a fade color thinker which has two colormap entries
		// AND every ffloor had a fade FOF thinker with one colormap entry
		I_Error("Colormap %d from server is too high for sectors %d", index, (UINT32)numsectors);

	// our index doesn't exist, so just make the entry
	for (; i <= index; i++)
	{
		exc = R_CreateDefaultColormap(false);
		if (last_exc)
			last_exc->next = exc;
		exc->prev = last_exc;
		exc->next = NULL;
		last_exc = exc;
	}
	return exc;
}

static void ClearNetColormaps(void)
{
	// We're actually Z_Freeing each entry here,
	// so don't call this in P_NetUnArchiveColormaps (where entries will be used in-game)
	extracolormap_t *exc, *exc_next;

	for (exc = net_colormaps; exc; exc = exc_next)
	{
		exc_next = exc->next;
		Z_Free(exc);
	}
	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetArchiveColormaps(void)
{
	// We save and then we clean up our colormap mess
	extracolormap_t *exc, *exc_next;
	UINT32 i = 0;
	WRITEUINT32(save_p, num_net_colormaps); // save for safety

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		// We must save num_net_colormaps worth of data
		// So fill non-existent entries with default.
		if (!exc)
			exc = R_CreateDefaultColormap(false);

		WRITEUINT8(save_p, exc->fadestart);
		WRITEUINT8(save_p, exc->fadeend);
		WRITEUINT8(save_p, exc->flags);

		WRITEINT32(save_p, exc->rgba);
		WRITEINT32(save_p, exc->fadergba);

#ifdef EXTRACOLORMAPLUMPS
		WRITESTRINGN(save_p, exc->lumpname, 9);
#endif

		exc_next = exc->next;
		Z_Free(exc); // don't need anymore
	}

	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetUnArchiveColormaps(void)
{
	// When we reach this point, we already populated our list with
	// dummy colormaps. Now that we are loading the color data,
	// set up the dummies.
	extracolormap_t *exc, *existing_exc, *exc_next = NULL;
	UINT32 i = 0;

	num_net_colormaps = READUINT32(save_p);

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		UINT8 fadestart, fadeend, flags;
		INT32 rgba, fadergba;
#ifdef EXTRACOLORMAPLUMPS
		char lumpname[9];
#endif

		fadestart = READUINT8(save_p);
		fadeend = READUINT8(save_p);
		flags = READUINT8(save_p);

		rgba = READINT32(save_p);
		fadergba = READINT32(save_p);

#ifdef EXTRACOLORMAPLUMPS
		READSTRINGN(save_p, lumpname, 9);

		if (lumpname[0])
		{
			if (!exc)
				// no point making a new entry since nothing points to it,
				// but we needed to read the data so now continue
				continue;

			exc_next = exc->next; // this gets overwritten during our operations here, so get it now
			existing_exc = R_ColormapForName(lumpname);
			*exc = *existing_exc;
			R_AddColormapToList(exc); // see HACK note below on why we're adding duplicates
			continue;
		}
#endif

		if (!exc)
			// no point making a new entry since nothing points to it,
			// but we needed to read the data so now continue
			continue;

		exc_next = exc->next; // this gets overwritten during our operations here, so get it now

		exc->fadestart = fadestart;
		exc->fadeend = fadeend;
		exc->flags = flags;

		exc->rgba = rgba;
		exc->fadergba = fadergba;

#ifdef EXTRACOLORMAPLUMPS
		exc->lump = LUMPERROR;
		exc->lumpname[0] = 0;
#endif

		existing_exc = R_GetColormapFromListByValues(rgba, fadergba, fadestart, fadeend, flags);

		if (existing_exc)
			exc->colormap = existing_exc->colormap;
		else
			// CONS_Debug(DBG_RENDER, "Creating Colormap: rgba(%d,%d,%d,%d) fadergba(%d,%d,%d,%d)\n",
			// 	R_GetRgbaR(rgba), R_GetRgbaG(rgba), R_GetRgbaB(rgba), R_GetRgbaA(rgba),
			//	R_GetRgbaR(fadergba), R_GetRgbaG(fadergba), R_GetRgbaB(fadergba), R_GetRgbaA(fadergba));
			exc->colormap = R_CreateLightTable(exc);

		// HACK: If this dummy is a duplicate, we're going to add it
		// to the extra_colormaps list anyway. I think this is faster
		// than going through every loaded sector and correcting their
		// colormap address to the pre-existing one, PER net_colormap entry
		R_AddColormapToList(exc);

		if (i < num_net_colormaps-1 && !exc_next)
			exc_next = R_CreateDefaultColormap(false);
	}

	// if we still have a valid net_colormap after iterating up to num_net_colormaps,
	// some sector had a colormap index higher than num_net_colormaps. We done goofed or $$$ was corrupted.
	// In any case, add them to the colormap list too so that at least the sectors' colormap
	// addresses are valid and accounted properly
	if (exc_next)
	{
		existing_exc = R_GetDefaultColormap();
		for (exc = exc_next; exc; exc = exc->next)
		{
			exc->colormap = existing_exc->colormap; // all our dummies are default values
			R_AddColormapToList(exc);
		}
	}

	// Don't need these anymore
	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetArchiveWaypoints(void)
{
	INT32 i, j;

	for (i = 0; i < NUMWAYPOINTSEQUENCES; i++)
	{
		WRITEUINT16(save_p, numwaypoints[i]);
		for (j = 0; j < numwaypoints[i]; j++)
			WRITEUINT32(save_p, waypoints[i][j] ? waypoints[i][j]->mobjnum : 0);
	}
}

static void P_NetUnArchiveWaypoints(void)
{
	INT32 i, j;
	UINT32 mobjnum;

	for (i = 0; i < NUMWAYPOINTSEQUENCES; i++)
	{
		numwaypoints[i] = READUINT16(save_p);
		for (j = 0; j < numwaypoints[i]; j++)
		{
			mobjnum = READUINT32(save_p);
			waypoints[i][j] = (mobjnum == 0) ? NULL : P_FindNewPosition(mobjnum);
		}
	}
}

///
/// World Archiving
///

#define SD_FLOORHT  0x01
#define SD_CEILHT   0x02
#define SD_FLOORPIC 0x04
#define SD_CEILPIC  0x08
#define SD_LIGHT    0x10
#define SD_SPECIAL  0x20
#define SD_DIFF2    0x40
#define SD_FFLOORS  0x80

// diff2 flags
#define SD_FXOFFS    0x01
#define SD_FYOFFS    0x02
#define SD_CXOFFS    0x04
#define SD_CYOFFS    0x08
#define SD_FLOORANG  0x10
#define SD_CEILANG   0x20
#define SD_TAG       0x40
#define SD_DIFF3     0x80

// diff3 flags
#define SD_TAGLIST   0x01
#define SD_COLORMAP  0x02
#define SD_CRUMBLESTATE 0x04

#define LD_FLAG     0x01
#define LD_SPECIAL  0x02
#define LD_CLLCOUNT 0x04
#define LD_S1TEXOFF 0x08
#define LD_S1TOPTEX 0x10
#define LD_S1BOTTEX 0x20
#define LD_S1MIDTEX 0x40
#define LD_DIFF2    0x80

// diff2 flags
#define LD_S2TEXOFF      0x01
#define LD_S2TOPTEX      0x02
#define LD_S2BOTTEX      0x04
#define LD_S2MIDTEX      0x08
#define LD_ARGS          0x10
#define LD_STRINGARGS    0x20
#define LD_EXECUTORDELAY 0x40

static boolean P_AreArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUMLINEARGS; i++)
		if (li->args[i] != spawnli->args[i])
			return false;

	return true;
}

static boolean P_AreStringArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUMLINESTRINGARGS; i++)
	{
		if (!li->stringargs[i])
			return !spawnli->stringargs[i];

		if (strcmp(li->stringargs[i], spawnli->stringargs[i]))
			return false;
	}

	return true;
}

//
// ResetSectors
// Resets the sectors in the level to their original state
//
static void ResetSectors(void)
{
	UINT8* wadData = NULL;
	const mapsector_t *ms;
	const mapsidedef_t *msd;
	const maplinedef_t *mld;
	sector_t *ss, *spawnss;
	line_t* li;
	side_t* si;

	if (W_IsLumpWad(lastloadedmaplumpnum)) // welp it's a map wad in a pk3
	{ // HACK: Open wad file rather quickly so we can get the data from the relevant lumps
		wadData = W_CacheLumpNum(lastloadedmaplumpnum, PU_STATIC);
		filelump_t *fileinfo = (filelump_t *)(wadData + ((wadinfo_t *)wadData)->infotableofs);
#define retrieve_mapdata(d, f)\
		d = (void*)(wadData + (f)->filepos);

		retrieve_mapdata(ms, fileinfo + ML_SECTORS);
		retrieve_mapdata(mld, fileinfo + ML_LINEDEFS);
		retrieve_mapdata(msd, fileinfo + ML_SIDEDEFS);
#undef retrieve_mapdata
	}
	else // phew it's just a WAD
	{
		ms = W_CacheLumpNum(lastloadedmaplumpnum + ML_SECTORS, PU_CACHE);
		mld = W_CacheLumpNum(lastloadedmaplumpnum + ML_LINEDEFS, PU_CACHE);
		msd = W_CacheLumpNum(lastloadedmaplumpnum + ML_SIDEDEFS, PU_CACHE);
	}

	// reset every sector
	// commented out lines are eh lines
	// we really should do them properly, but they take effort and only have a visual effect
	for (ss = sectors, spawnss = spawnsectors; ss < &sectors[numsectors]; ss++, ms++, spawnss++) {
		ss->floorheight = ms->floorheight<<FRACBITS;
		ss->ceilingheight = ms->ceilingheight<<FRACBITS;

		//ss->floorpic = P_AddLevelFlatRuntime((char *)get);
		//ss->ceilingpic = P_AddLevelFlatRuntime((char *)get);
		ss->lightlevel = ms->lightlevel;
		ss->special = ms->special;

		ss->floor_xoffs = spawnss->floor_xoffs;
		ss->floor_yoffs = spawnss->floor_yoffs;
		ss->ceiling_xoffs = spawnss->ceiling_xoffs;
		ss->ceiling_yoffs = spawnss->ceiling_yoffs;
		ss->floorpic_angle = spawnss->floorpic_angle;
		ss->ceilingpic_angle = spawnss->ceilingpic_angle;
		// ss->tags = ms->tag; // DON'T use Tag_FSet
		Tag_FSet(&ss->tags, SHORT(ms->tag));

		// ss->firsttag = spawnss->firsttag;
		// ss->nexttag = spawnss->nexttag;
		//ss->extra_colormap = GetNetColormapFromList(READUINT32(get));

		ffloor_t* floor = ss->ffloors;

		while (floor != NULL)
		{
			floor->flags = floor->spawnflags;
			floor->alpha = floor->spawnalpha;

			floor = floor->next;
		}
	}

	for (li = lines; li < &lines[numlines]; li++, mld++)
	{
		li->special = mld->special;
		if (SHORT(li->special) == 321 || SHORT(li->special) == 322) {
			li->callcount = 0;
		}

		si = &sides[li->sidenum[0]];
		//si->textureoffset = si->;
		//si->toptexture = READINT32(get);
		//si->bottomtexture = READINT32(get);
		//si->midtexture = READINT32(get);

		si = &sides[li->sidenum[1]];
		//si->textureoffset = READFIXED(get);
		//si->toptexture = READINT32(get);
		//si->bottomtexture = READINT32(get);
		//si->midtexture = READINT32(get);
	}

	if (wadData)
	{
		Z_Free(wadData);
	}
}

#define FD_FLAGS 0x01
#define FD_ALPHA 0x02

// Check if any of the sector's FOFs differ from how they spawned
static boolean CheckFFloorDiff(const sector_t *ss)
{
	ffloor_t *rover;

	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		if (rover->flags != rover->spawnflags
		|| rover->alpha != rover->spawnalpha)
			{
				return true; // we found an FOF that changed!
				// don't bother checking for more, we do that later
			}
	}
	return false;
}

// Special case: save the stats of all modified ffloors along with their ffloor "number"s
// we don't bother with ffloors that haven't changed, that would just add to savegame even more than is really needed
static void ArchiveFFloors(const sector_t *ss)
{
	size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
	ffloor_t *rover;
	UINT8 fflr_diff;
	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		fflr_diff = 0; // reset diff flags
		if (rover->flags != rover->spawnflags)
			fflr_diff |= FD_FLAGS;
		if (rover->alpha != rover->spawnalpha)
			fflr_diff |= FD_ALPHA;

		if (fflr_diff)
		{
			WRITEUINT16(save_p, j); // save ffloor "number"
			WRITEUINT8(save_p, fflr_diff);
			if (fflr_diff & FD_FLAGS)
				WRITEUINT32(save_p, rover->flags);
			if (fflr_diff & FD_ALPHA)
				WRITEINT16(save_p, rover->alpha);
		}
		j++;
	}
	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveFFloors(const sector_t *ss)
{
	UINT16 j = 0; // number of current ffloor in loop
	UINT16 fflr_i; // saved ffloor "number" of next modified ffloor
	UINT16 fflr_diff; // saved ffloor diff
	ffloor_t *rover;

	rover = ss->ffloors;
	if (!rover) // it is assumed sectors[i].ffloors actually exists, but just in case...
		I_Error("Sector does not have any ffloors!");

	fflr_i = READUINT16(save_p); // get first modified ffloor's number ready
	for (;;) // for some reason the usual for (rover = x; ...) thing doesn't work here?
	{
		if (fflr_i == 0xffff) // end of modified ffloors list, let's stop already
			break;
		// should NEVER need to be checked
		//if (rover == NULL)
			//break;
		if (j != fflr_i) // this ffloor was not modified
		{
			j++;
			rover = rover->next;
			continue;
		}

		fflr_diff = READUINT8(save_p);

		if (fflr_diff & FD_FLAGS)
			rover->flags = READUINT32(save_p);
		if (fflr_diff & FD_ALPHA)
			rover->alpha = READINT16(save_p);

		fflr_i = READUINT16(save_p); // get next ffloor "number" ready

		j++;
		rover = rover->next;
	}
}

static void ArchiveSectors(void)
{
	size_t i, j;
	const sector_t *ss = sectors;
	const sector_t *spawnss = spawnsectors;
	UINT8 diff, diff2, diff3;

	for (i = 0; i < numsectors; i++, ss++, spawnss++)
	{
		diff = diff2 = diff3 = 0;
		if (ss->floorheight != spawnss->floorheight)
			diff |= SD_FLOORHT;
		if (ss->ceilingheight != spawnss->ceilingheight)
			diff |= SD_CEILHT;
		//
		// flats
		//
		if (ss->floorpic != spawnss->floorpic)
			diff |= SD_FLOORPIC;
		if (ss->ceilingpic != spawnss->ceilingpic)
			diff |= SD_CEILPIC;

		if (ss->lightlevel != spawnss->lightlevel)
			diff |= SD_LIGHT;
		if (ss->special != spawnss->special)
			diff |= SD_SPECIAL;

		if (ss->floor_xoffs != spawnss->floor_xoffs)
			diff2 |= SD_FXOFFS;
		if (ss->floor_yoffs != spawnss->floor_yoffs)
			diff2 |= SD_FYOFFS;
		if (ss->ceiling_xoffs != spawnss->ceiling_xoffs)
			diff2 |= SD_CXOFFS;
		if (ss->ceiling_yoffs != spawnss->ceiling_yoffs)
			diff2 |= SD_CYOFFS;
		if (ss->floorpic_angle != spawnss->floorpic_angle)
			diff2 |= SD_FLOORANG;
		if (ss->ceilingpic_angle != spawnss->ceilingpic_angle)
			diff2 |= SD_CEILANG;

		if (!Tag_Compare(&ss->tags, &spawnss->tags))
			diff2 |= SD_TAG;

		if (ss->extra_colormap != spawnss->extra_colormap)
			diff3 |= SD_COLORMAP;
		if (ss->crumblestate)
			diff3 |= SD_CRUMBLESTATE;

		if (ss->ffloors && CheckFFloorDiff(ss))
			diff |= SD_FFLOORS;

		if (diff3)
			diff2 |= SD_DIFF3;

		if (diff2)
			diff |= SD_DIFF2;

		if (diff)
		{
			WRITEUINT16(save_p, i);
			WRITEUINT8(save_p, diff);
			if (diff & SD_DIFF2)
				WRITEUINT8(save_p, diff2);
			if (diff2 & SD_DIFF3)
				WRITEUINT8(save_p, diff3);
			if (diff & SD_FLOORHT)
				WRITEFIXED(save_p, ss->floorheight);
			if (diff & SD_CEILHT)
				WRITEFIXED(save_p, ss->ceilingheight);
			if (diff & SD_FLOORPIC)
				WRITEMEM(save_p, levelflats[ss->floorpic].name, 8);
			if (diff & SD_CEILPIC)
				WRITEMEM(save_p, levelflats[ss->ceilingpic].name, 8);
			if (diff & SD_LIGHT)
				WRITEINT16(save_p, ss->lightlevel);
			if (diff & SD_SPECIAL)
				WRITEINT16(save_p, ss->special);
			if (diff2 & SD_FXOFFS)
				WRITEFIXED(save_p, ss->floor_xoffs);
			if (diff2 & SD_FYOFFS)
				WRITEFIXED(save_p, ss->floor_yoffs);
			if (diff2 & SD_CXOFFS)
				WRITEFIXED(save_p, ss->ceiling_xoffs);
			if (diff2 & SD_CYOFFS)
				WRITEFIXED(save_p, ss->ceiling_yoffs);
			if (diff2 & SD_FLOORANG)
				WRITEANGLE(save_p, ss->floorpic_angle);
			if (diff2 & SD_CEILANG)
				WRITEANGLE(save_p, ss->ceilingpic_angle);
			if (diff2 & SD_TAG)
			{
				WRITEUINT32(save_p, ss->tags.count);
				for (j = 0; j < ss->tags.count; j++)
					WRITEINT16(save_p, ss->tags.tags[j]);
			}

			if (diff3 & SD_COLORMAP)
				WRITEUINT32(save_p, CheckAddNetColormapToList(ss->extra_colormap));
					// returns existing index if already added, or appends to net_colormaps and returns new index
			if (diff3 & SD_CRUMBLESTATE)
				WRITEINT32(save_p, ss->crumblestate);
			if (diff & SD_FFLOORS)
				ArchiveFFloors(ss);
		}
	}

	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveSectors(void)
{
	UINT16 i, j;
	UINT8 diff, diff2, diff3;
	for (;;)
	{
		i = READUINT16(save_p);

		if (i == 0xffff)
			break;

		if (i > numsectors)
			I_Error("Invalid sector number %u from server (expected end at %s)", i, sizeu1(numsectors));

		diff = READUINT8(save_p);
		if (diff & SD_DIFF2)
			diff2 = READUINT8(save_p);
		else
			diff2 = 0;
		if (diff2 & SD_DIFF3)
			diff3 = READUINT8(save_p);
		else
			diff3 = 0;

		if (diff & SD_FLOORHT)
			sectors[i].floorheight = READFIXED(save_p);
		if (diff & SD_CEILHT)
			sectors[i].ceilingheight = READFIXED(save_p);
		if (diff & SD_FLOORPIC)
		{
			sectors[i].floorpic = P_AddLevelFlatRuntime((char *)save_p);
			save_p += 8;
		}
		if (diff & SD_CEILPIC)
		{
			sectors[i].ceilingpic = P_AddLevelFlatRuntime((char *)save_p);
			save_p += 8;
		}
		if (diff & SD_LIGHT)
			sectors[i].lightlevel = READINT16(save_p);
		if (diff & SD_SPECIAL)
			sectors[i].special = READINT16(save_p);

		if (diff2 & SD_FXOFFS)
			sectors[i].floor_xoffs = READFIXED(save_p);
		if (diff2 & SD_FYOFFS)
			sectors[i].floor_yoffs = READFIXED(save_p);
		if (diff2 & SD_CXOFFS)
			sectors[i].ceiling_xoffs = READFIXED(save_p);
		if (diff2 & SD_CYOFFS)
			sectors[i].ceiling_yoffs = READFIXED(save_p);
		if (diff2 & SD_FLOORANG)
			sectors[i].floorpic_angle  = READANGLE(save_p);
		if (diff2 & SD_CEILANG)
			sectors[i].ceilingpic_angle = READANGLE(save_p);
		if (diff2 & SD_TAG)
		{
			size_t ncount = READUINT32(save_p);

			// Remove entries from global lists.
			for (j = 0; j < sectors[i].tags.count; j++)
				Taggroup_Remove(tags_sectors, sectors[i].tags.tags[j], i);

			// Reallocate if size differs.
			if (ncount != sectors[i].tags.count)
			{
				sectors[i].tags.count = ncount;
				sectors[i].tags.tags = Z_Realloc(sectors[i].tags.tags, ncount*sizeof(mtag_t), PU_LEVEL, NULL);
			}

			for (j = 0; j < ncount; j++)
				sectors[i].tags.tags[j] = READINT16(save_p);

			// Add new entries.
			for (j = 0; j < sectors[i].tags.count; j++)
				Taggroup_Remove(tags_sectors, sectors[i].tags.tags[j], i);
		}


		if (diff3 & SD_COLORMAP)
			sectors[i].extra_colormap = GetNetColormapFromList(READUINT32(save_p));
		if (diff3 & SD_CRUMBLESTATE)
			sectors[i].crumblestate = READINT32(save_p);

		if (diff & SD_FFLOORS)
			UnArchiveFFloors(&sectors[i]);
	}
}

static void ArchiveLines(void)
{
	size_t i;
	const line_t *li = lines;
	const line_t *spawnli = spawnlines;
	const side_t *si;
	const side_t *spawnsi;
	UINT8 diff, diff2; // no diff3

	for (i = 0; i < numlines; i++, spawnli++, li++)
	{
		diff = diff2 = 0;

		if (li->special != spawnli->special)
			diff |= LD_SPECIAL;

		if (spawnli->special == 321 || spawnli->special == 322) // only reason li->callcount would be non-zero is if either of these are involved
			diff |= LD_CLLCOUNT;

		if (!P_AreArgsEqual(li, spawnli))
			diff2 |= LD_ARGS;

		if (!P_AreStringArgsEqual(li, spawnli))
			diff2 |= LD_STRINGARGS;

		if (li->executordelay != spawnli->executordelay)
			diff2 |= LD_EXECUTORDELAY;

		if (li->sidenum[0] != 0xffff)
		{
			si = &sides[li->sidenum[0]];
			spawnsi = &spawnsides[li->sidenum[0]];
			if (si->textureoffset != spawnsi->textureoffset)
				diff |= LD_S1TEXOFF;
			//SoM: 4/1/2000: Some textures are colormaps. Don't worry about invalid textures.
			if (si->toptexture != spawnsi->toptexture)
				diff |= LD_S1TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff |= LD_S1BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff |= LD_S1MIDTEX;
		}
		if (li->sidenum[1] != 0xffff)
		{
			si = &sides[li->sidenum[1]];
			spawnsi = &spawnsides[li->sidenum[1]];
			if (si->textureoffset != spawnsi->textureoffset)
				diff2 |= LD_S2TEXOFF;
			if (si->toptexture != spawnsi->toptexture)
				diff2 |= LD_S2TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff2 |= LD_S2BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff2 |= LD_S2MIDTEX;
		}

		if (diff2)
			diff |= LD_DIFF2;

		if (diff)
		{
			WRITEINT16(save_p, i);
			WRITEUINT8(save_p, diff);
			if (diff & LD_DIFF2)
				WRITEUINT8(save_p, diff2);
			if (diff & LD_FLAG)
				WRITEINT16(save_p, li->flags);
			if (diff & LD_SPECIAL)
				WRITEINT16(save_p, li->special);
			if (diff & LD_CLLCOUNT)
				WRITEINT16(save_p, li->callcount);

			si = &sides[li->sidenum[0]];
			if (diff & LD_S1TEXOFF)
				WRITEFIXED(save_p, si->textureoffset);
			if (diff & LD_S1TOPTEX)
				WRITEINT32(save_p, si->toptexture);
			if (diff & LD_S1BOTTEX)
				WRITEINT32(save_p, si->bottomtexture);
			if (diff & LD_S1MIDTEX)
				WRITEINT32(save_p, si->midtexture);

			si = &sides[li->sidenum[1]];
			if (diff2 & LD_S2TEXOFF)
				WRITEFIXED(save_p, si->textureoffset);
			if (diff2 & LD_S2TOPTEX)
				WRITEINT32(save_p, si->toptexture);
			if (diff2 & LD_S2BOTTEX)
				WRITEINT32(save_p, si->bottomtexture);
			if (diff2 & LD_S2MIDTEX)
				WRITEINT32(save_p, si->midtexture);
			if (diff2 & LD_ARGS)
			{
				UINT8 j;
				for (j = 0; j < NUMLINEARGS; j++)
					WRITEINT32(save_p, li->args[j]);
			}
			if (diff2 & LD_STRINGARGS)
			{
				UINT8 j;
				for (j = 0; j < NUMLINESTRINGARGS; j++)
				{
					size_t len, k;

					if (!li->stringargs[j])
					{
						WRITEINT32(save_p, 0);
						continue;
					}

					len = strlen(li->stringargs[j]);
					WRITEINT32(save_p, len);
					for (k = 0; k < len; k++)
						WRITECHAR(save_p, li->stringargs[j][k]);
				}
			}
			if (diff2 & LD_EXECUTORDELAY)
				WRITEINT32(save_p, li->executordelay);
		}
	}
	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveLines(void)
{
	UINT16 i;
	line_t *li;
	side_t *si;
	UINT8 diff, diff2; // no diff3

	for (;;)
	{
		i = READUINT16(save_p);

		if (i == 0xffff)
			break;
		if (i > numlines)
			I_Error("Invalid line number %u from server", i);

		diff = READUINT8(save_p);
		li = &lines[i];

		if (diff & LD_DIFF2)
			diff2 = READUINT8(save_p);
		else
			diff2 = 0;

		if (diff & LD_FLAG)
			li->flags = READINT16(save_p);
		if (diff & LD_SPECIAL)
			li->special = READINT16(save_p);
		if (diff & LD_CLLCOUNT)
			li->callcount = READINT16(save_p);

		si = &sides[li->sidenum[0]];
		if (diff & LD_S1TEXOFF)
			si->textureoffset = READFIXED(save_p);
		if (diff & LD_S1TOPTEX)
			si->toptexture = READINT32(save_p);
		if (diff & LD_S1BOTTEX)
			si->bottomtexture = READINT32(save_p);
		if (diff & LD_S1MIDTEX)
			si->midtexture = READINT32(save_p);

		si = &sides[li->sidenum[1]];
		if (diff2 & LD_S2TEXOFF)
			si->textureoffset = READFIXED(save_p);
		if (diff2 & LD_S2TOPTEX)
			si->toptexture = READINT32(save_p);
		if (diff2 & LD_S2BOTTEX)
			si->bottomtexture = READINT32(save_p);
		if (diff2 & LD_S2MIDTEX)
			si->midtexture = READINT32(save_p);
		if (diff2 & LD_ARGS)
		{
			UINT8 j;
			for (j = 0; j < NUMLINEARGS; j++)
				li->args[j] = READINT32(save_p);
		}
		if (diff2 & LD_STRINGARGS)
		{
			UINT8 j;
			for (j = 0; j < NUMLINESTRINGARGS; j++)
			{
				size_t len = READINT32(save_p);
				size_t k;

				if (!len)
				{
					Z_Free(li->stringargs[j]);
					li->stringargs[j] = NULL;
					continue;
				}

				li->stringargs[j] = Z_Realloc(li->stringargs[j], len + 1, PU_LEVEL, NULL);
				for (k = 0; k < len; k++)
					li->stringargs[j][k] = READCHAR(save_p);
				li->stringargs[j][len] = '\0';
			}
		}
		if (diff2 & LD_EXECUTORDELAY)
			li->executordelay = READINT32(save_p);

	}
}

static void P_NetArchiveWorld(void)
{
	// initialize colormap vars because paranoia
	ClearNetColormaps();

	WRITEUINT32(save_p, ARCHIVEBLOCK_WORLD);

	ArchiveSectors();
	ArchiveLines();
	R_ClearTextureNumCache(false);
}

static void P_NetUnArchiveWorld(void)
{
	UINT16 i;

	if (READUINT32(save_p) != ARCHIVEBLOCK_WORLD)
		I_Error("Bad $$$.sav at archive block World");

	// initialize colormap vars because paranoia
	ClearNetColormaps();

	// count the level's ffloors so that colormap loading can have an upper limit
	for (i = 0; i < numsectors; i++)
	{
		ffloor_t *rover;
		for (rover = sectors[i].ffloors; rover; rover = rover->next)
			num_ffloors++;
	}

	UnArchiveSectors();
	UnArchiveLines();
}

//
// Thinkers
//

typedef enum
{
	MD_SPAWNPOINT  = 1,
	MD_POS         = 1<<1,
	MD_TYPE        = 1<<2,
	MD_MOM         = 1<<3,
	MD_RADIUS      = 1<<4,
	MD_HEIGHT      = 1<<5,
	MD_FLAGS       = 1<<6,
	MD_HEALTH      = 1<<7,
	MD_RTIME       = 1<<8,
	MD_STATE       = 1<<9,
	MD_TICS        = 1<<10,
	MD_SPRITE      = 1<<11,
	MD_FRAME       = 1<<12,
	MD_EFLAGS      = 1<<13,
	MD_PLAYER      = 1<<14,
	MD_MOVEDIR     = 1<<15,
	MD_MOVECOUNT   = 1<<16,
	MD_THRESHOLD   = 1<<17,
	MD_LASTLOOK    = 1<<18,
	MD_TARGET      = 1<<19,
	MD_TRACER      = 1<<20,
	MD_FRICTION    = 1<<21,
	MD_MOVEFACTOR  = 1<<22,
	MD_FLAGS2      = 1<<23,
	MD_FUSE        = 1<<24,
	MD_WATERTOP    = 1<<25,
	MD_WATERBOTTOM = 1<<26,
	MD_SCALE       = 1<<27,
	MD_DSCALE      = 1<<28,
	MD_BLUEFLAG    = 1<<29,
	MD_REDFLAG     = 1<<30,
	MD_MORE        = 1<<31
} mobj_diff_t;

typedef enum
{
	MD2_CUSVAL      = 1,
	MD2_CVMEM       = 1<<1,
	MD2_SKIN        = 1<<2,
	MD2_COLOR       = 1<<3,
	MD2_SCALESPEED  = 1<<4,
	MD2_EXTVAL1     = 1<<5,
	MD2_EXTVAL2     = 1<<6,
	MD2_HNEXT       = 1<<7,
	MD2_HPREV       = 1<<8,
	MD2_FLOORROVER  = 1<<9,
	MD2_CEILINGROVER = 1<<10,
	MD2_SLOPE        = 1<<11,
	MD2_COLORIZED    = 1<<12,
	MD2_MIRRORED     = 1<<13,
	MD2_ROLLANGLE    = 1<<14,
	MD2_SHADOWSCALE  = 1<<15,
	MD2_RENDERFLAGS  = 1<<16,
	MD2_BLENDMODE    = 1<<17,
	MD2_SPRITEXSCALE = 1<<18,
	MD2_SPRITEYSCALE = 1<<19,
	MD2_SPRITEXOFFSET = 1<<20,
	MD2_SPRITEYOFFSET = 1<<21,
	MD2_FLOORSPRITESLOPE = 1<<22,
} mobj_diff2_t;

typedef enum
{
	tc_mobj,
	tc_ceiling,
	tc_floor,
	tc_flash,
	tc_strobe,
	tc_glow,
	tc_fireflicker,
	tc_thwomp,
	tc_camerascanner,
	tc_elevator,
	tc_continuousfalling,
	tc_bouncecheese,
	tc_startcrumble,
	tc_marioblock,
	tc_marioblockchecker,
	tc_floatsector,
	tc_crushceiling,
	tc_scroll,
	tc_friction,
	tc_pusher,
	tc_laserflash,
	tc_lightfade,
	tc_executor,
	tc_raisesector,
	tc_noenemies,
	tc_eachtime,
	tc_disappear,
	tc_fade,
	tc_fadecolormap,
	tc_planedisplace,
	tc_dynslopeline,
	tc_dynslopevert,
	tc_polyrotate, // haleyjd 03/26/06: polyobjects
	tc_polymove,
	tc_polywaypoint,
	tc_polyslidedoor,
	tc_polyswingdoor,
	tc_polyflag,
	tc_polydisplace,
	tc_polyrotdisplace,
	tc_polyfade,
	tc_end
} specials_e;

typedef enum
{
	ST_FLOORDATA = 1,
	ST_CEILDATA = 2,
	ST_LIGHTDATA = 4,
	ST_FADEMAPDATA = 8
} sectortarget_e;

typedef struct
{
	actionf_p1 action;
	UINT16 size;
	UINT32 sectorPointer;
	UINT8 sectorTargets;
} specialdef_t;

// special function/data size associations
#define NOSECTOR(func, type) (actionf_p1)func, sizeof(type), 0, 0
#define WITHSECTOR(func, type, targets) (actionf_p1)func, sizeof(type), (UINT32)&((type*)NULL)->sector, targets
static const specialdef_t specialDefs[] =
{
	NOSECTOR(P_MobjThinker, mobj_t),                   // tc_mobj
	WITHSECTOR(T_MoveCeiling, ceiling_t, ST_CEILDATA), // tc_ceiling
	WITHSECTOR(T_MoveFloor, floormove_t, ST_FLOORDATA), // tc_floor
	WITHSECTOR(T_LightningFlash, lightflash_t, ST_LIGHTDATA), // tc_flash
	WITHSECTOR(T_StrobeFlash, strobe_t, ST_LIGHTDATA),                 // tc_strobe
	WITHSECTOR(T_Glow, glow_t, ST_LIGHTDATA),                          // tc_glow
	WITHSECTOR(T_FireFlicker, fireflicker_t, ST_LIGHTDATA),            // tc_fireflicker
	WITHSECTOR(T_ThwompSector, thwomp_t, ST_FLOORDATA | ST_CEILDATA),         // tc_thwomp
	NOSECTOR(T_CameraScanner, elevator_t),              // tc_camerascanner
	WITHSECTOR(T_MoveElevator, elevator_t, ST_CEILDATA | ST_FLOORDATA),              // tc_elevator
	WITHSECTOR(T_ContinuousFalling, continuousfall_t, ST_CEILDATA | ST_FLOORDATA),   // tc_contunousfalling
	WITHSECTOR(T_BounceCheese, bouncecheese_t, ST_CEILDATA),         // tc_bouncecheese
	WITHSECTOR(T_StartCrumble, elevator_t, ST_FLOORDATA),               // tc_startcrumble
	WITHSECTOR(T_MarioBlock, mariothink_t, ST_FLOORDATA | ST_CEILDATA),           // tc_marioblock
	NOSECTOR(T_MarioBlockChecker, mariocheck_t),    // tc_marioblockchecker
	NOSECTOR(T_FloatSector, floatthink_t),          // tc_floatsector
	WITHSECTOR(T_CrushCeiling, ceiling_t, ST_CEILDATA),                // tc_crushceiling
	NOSECTOR(T_Scroll, scroll_t),                       // tc_scroll
	NOSECTOR(T_Friction, friction_t),                   // tc_friction
	NOSECTOR(T_Pusher, pusher_t),                       // tc_pusher
	NOSECTOR(T_LaserFlash, laserthink_t),               // tc_laserflash
	WITHSECTOR(T_LightFade, lightlevel_t, ST_LIGHTDATA),                // tc_lightfade
	NOSECTOR(T_ExecutorDelay, executor_t),              // tc_executor
	NOSECTOR(T_RaiseSector, raise_t),          // tc_raisesector
	NOSECTOR(T_NoEnemiesSector, noenemies_t),      // tc_noenemies
	NOSECTOR(T_EachTimeThinker, eachtime_t),      // tc_eachtime
	NOSECTOR(T_Disappear, disappear_t),                // tc_disappear
	NOSECTOR(T_Fade, fade_t),                          // tc_fade
	WITHSECTOR(T_FadeColormap, fadecolormap_t, ST_FADEMAPDATA),          // tc_fade
	NOSECTOR(T_PlaneDisplace, planedisplace_t),        // tc_planedisplace
	NOSECTOR(T_DynamicSlopeLine, dynplanethink_t),      // tc_dynslopeline
	NOSECTOR(T_DynamicSlopeVert, dynplanethink_t),      // tc_dynslopevert
	NOSECTOR(T_PolyObjRotate, polyrotate_t),            // tc_polyrotate
	NOSECTOR(T_PolyObjMove, polymove_t),                // tc_polymove
	NOSECTOR(T_PolyObjWaypoint, polywaypoint_t),        // tc_polywaypoint
	NOSECTOR(T_PolyDoorSlide, polyslidedoor_t),         // tc_polyslidedoor
	NOSECTOR(T_PolyDoorSwing, polyswingdoor_t),         // tc_polyswingdoor
	NOSECTOR(T_PolyObjFlag, polymove_t),                // tc_polyflag
	NOSECTOR(T_PolyObjDisplace, polydisplace_t),        // tc_polydisplace
	NOSECTOR(T_PolyObjRotDisplace, polyrotdisplace_t),  // tc_polyrotdisplace
	NOSECTOR(T_PolyObjFade, polyfade_t),                // tc_polyfade
};
#undef A

static inline UINT32 SaveMobjnum(const mobj_t *mobj)
{
	if (mobj) return mobj->mobjnum;
	return 0;
}

static UINT32 SaveSector(const sector_t *sector)
{
	if (sector) return (UINT32)(sector - sectors);
	return 0xFFFFFFFF;
}

static UINT32 SaveLine(const line_t *line)
{
	if (line) return (UINT32)(line - lines);
	return 0xFFFFFFFF;
}

static inline UINT32 SavePlayer(const player_t *player)
{
	if (player) return (UINT32)(player - players);
	return 0xFFFFFFFF;
}

static UINT32 SaveSlope(const pslope_t *slope)
{
	if (slope) return (UINT32)(slope->id);
	return 0xFFFFFFFF;
}

static void SaveMobjThinker(const thinker_t *th, const UINT8 type)
{
	const mobj_t *mobj = (const mobj_t *)th;
	UINT32 diff;
	UINT32 diff2;

	// Ignore stationary hoops - these will be respawned from mapthings.
	if (mobj->type == MT_HOOP)
		return;

	// These are NEVER saved.
	if (mobj->type == MT_HOOPCOLLIDE)
		return;

	// This hoop has already been collected.
	if (mobj->type == MT_HOOPCENTER && mobj->threshold == 4242)
		return;

	if (mobj->spawnpoint && mobj->info->doomednum != -1)
	{
		// spawnpoint is not modified but we must save it since it is an identifier
		diff = MD_SPAWNPOINT;

		if ((mobj->x != mobj->spawnpoint->x << FRACBITS) ||
			(mobj->y != mobj->spawnpoint->y << FRACBITS) ||
			(mobj->angle != FixedAngle(mobj->spawnpoint->angle*FRACUNIT)) ||
			(mobj->pitch != FixedAngle(mobj->spawnpoint->pitch*FRACUNIT)) ||
			(mobj->roll != FixedAngle(mobj->spawnpoint->roll*FRACUNIT)) )
			diff |= MD_POS;

		if (mobj->info->doomednum != mobj->spawnpoint->type)
			diff |= MD_TYPE;
	}
	else
		diff = MD_POS | MD_TYPE; // not a map spawned thing so make it from scratch

	diff2 = 0;

	// not the default but the most probable
	if (mobj->momx != 0 || mobj->momy != 0 || mobj->momz != 0 || mobj->pmomz !=0)
		diff |= MD_MOM;
	if (mobj->radius != mobj->info->radius)
		diff |= MD_RADIUS;
	if (mobj->height != mobj->info->height)
		diff |= MD_HEIGHT;
	if (mobj->flags != mobj->info->flags)
		diff |= MD_FLAGS;
	if (mobj->flags2)
		diff |= MD_FLAGS2;
	if (mobj->health != mobj->info->spawnhealth)
		diff |= MD_HEALTH;
	if (mobj->reactiontime != mobj->info->reactiontime)
		diff |= MD_RTIME;
	if ((statenum_t)(mobj->state-states) != mobj->info->spawnstate)
		diff |= MD_STATE;
	if (mobj->tics != mobj->state->tics)
		diff |= MD_TICS;
	if (mobj->sprite != mobj->state->sprite)
		diff |= MD_SPRITE;
	if (mobj->sprite == SPR_PLAY && mobj->sprite2 != (mobj->state->frame&FF_FRAMEMASK))
		diff |= MD_SPRITE;
	if (mobj->frame != mobj->state->frame)
		diff |= MD_FRAME;
	if (mobj->anim_duration != (UINT16)mobj->state->var2)
		diff |= MD_FRAME;
	if (mobj->eflags)
		diff |= MD_EFLAGS;
	if (mobj->player)
		diff |= MD_PLAYER;

	if (mobj->movedir)
		diff |= MD_MOVEDIR;
	if (mobj->movecount)
		diff |= MD_MOVECOUNT;
	if (mobj->threshold)
		diff |= MD_THRESHOLD;
	if (mobj->lastlook != -1)
		diff |= MD_LASTLOOK;
	if (mobj->target)
		diff |= MD_TARGET;
	if (mobj->tracer)
		diff |= MD_TRACER;
	if (mobj->friction != ORIG_FRICTION)
		diff |= MD_FRICTION;
	if (mobj->movefactor != FRACUNIT)
		diff |= MD_MOVEFACTOR;
	if (mobj->fuse)
		diff |= MD_FUSE;
	if (mobj->watertop)
		diff |= MD_WATERTOP;
	if (mobj->waterbottom)
		diff |= MD_WATERBOTTOM;
	if (mobj->scale != FRACUNIT)
		diff |= MD_SCALE;
	if (mobj->destscale != mobj->scale)
		diff |= MD_DSCALE;
	if (mobj->scalespeed != FRACUNIT/12)
		diff2 |= MD2_SCALESPEED;

	if (mobj == redflag)
		diff |= MD_REDFLAG;
	if (mobj == blueflag)
		diff |= MD_BLUEFLAG;

	if (mobj->cusval)
		diff2 |= MD2_CUSVAL;
	if (mobj->cvmem)
		diff2 |= MD2_CVMEM;
	if (mobj->color)
		diff2 |= MD2_COLOR;
	if (mobj->skin)
		diff2 |= MD2_SKIN;
	if (mobj->extravalue1)
		diff2 |= MD2_EXTVAL1;
	if (mobj->extravalue2)
		diff2 |= MD2_EXTVAL2;
	if (mobj->hnext)
		diff2 |= MD2_HNEXT;
	if (mobj->hprev)
		diff2 |= MD2_HPREV;
	if (mobj->floorrover)
		diff2 |= MD2_FLOORROVER;
	if (mobj->ceilingrover)
		diff2 |= MD2_CEILINGROVER;
	if (mobj->standingslope)
		diff2 |= MD2_SLOPE;
	if (mobj->colorized)
		diff2 |= MD2_COLORIZED;
	if (mobj->mirrored)
		diff2 |= MD2_MIRRORED;
	if (mobj->rollangle)
		diff2 |= MD2_ROLLANGLE;
	if (mobj->shadowscale)
		diff2 |= MD2_SHADOWSCALE;
	if (mobj->renderflags)
		diff2 |= MD2_RENDERFLAGS;
	if (mobj->blendmode != AST_TRANSLUCENT)
		diff2 |= MD2_BLENDMODE;
	if (mobj->spritexscale != FRACUNIT)
		diff2 |= MD2_SPRITEXSCALE;
	if (mobj->spriteyscale != FRACUNIT)
		diff2 |= MD2_SPRITEYSCALE;
	if (mobj->spritexoffset)
		diff2 |= MD2_SPRITEXOFFSET;
	if (mobj->spriteyoffset)
		diff2 |= MD2_SPRITEYOFFSET;
	if (mobj->floorspriteslope)
	{
		pslope_t *slope = mobj->floorspriteslope;
		if (slope->zangle || slope->zdelta || slope->xydirection
		|| slope->o.x || slope->o.y || slope->o.z
		|| slope->d.x || slope->d.y
		|| slope->normal.x || slope->normal.y
		|| (slope->normal.z != FRACUNIT))
			diff2 |= MD2_FLOORSPRITESLOPE;
	}

	if (diff2 != 0)
		diff |= MD_MORE;

	// Scrap all of that. If we're a hoop center, this is ALL we're saving.
	if (mobj->type == MT_HOOPCENTER)
		diff = MD_SPAWNPOINT;

	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, diff);
	if (diff & MD_MORE)
		WRITEUINT32(save_p, diff2);

	// save pointer, at load time we will search this pointer to reinitilize pointers
	WRITEUINT32(save_p, (size_t)mobj);

	WRITEFIXED(save_p, mobj->z); // Force this so 3dfloor problems don't arise.
	WRITEFIXED(save_p, mobj->floorz);
	WRITEFIXED(save_p, mobj->ceilingz);

	if (diff2 & MD2_FLOORROVER)
	{
		WRITEUINT32(save_p, SaveSector(mobj->floorrover->target));
		WRITEUINT16(save_p, P_GetFFloorID(mobj->floorrover));
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		WRITEUINT32(save_p, SaveSector(mobj->ceilingrover->target));
		WRITEUINT16(save_p, P_GetFFloorID(mobj->ceilingrover));
	}

	if (diff & MD_SPAWNPOINT)
	{
		size_t z;

		for (z = 0; z < nummapthings; z++)
			if (&mapthings[z] == mobj->spawnpoint)
				WRITEUINT16(save_p, z);
		if (mobj->type == MT_HOOPCENTER)
			return;
	}

	if (diff & MD_TYPE)
		WRITEUINT32(save_p, mobj->type);
	if (diff & MD_POS)
	{
		WRITEFIXED(save_p, mobj->x);
		WRITEFIXED(save_p, mobj->y);
		WRITEANGLE(save_p, mobj->angle);
		WRITEANGLE(save_p, mobj->pitch);
		WRITEANGLE(save_p, mobj->roll);
	}
	if (diff & MD_MOM)
	{
		WRITEFIXED(save_p, mobj->momx);
		WRITEFIXED(save_p, mobj->momy);
		WRITEFIXED(save_p, mobj->momz);
		WRITEFIXED(save_p, mobj->pmomz);
	}
	if (diff & MD_RADIUS)
		WRITEFIXED(save_p, mobj->radius);
	if (diff & MD_HEIGHT)
		WRITEFIXED(save_p, mobj->height);
	if (diff & MD_FLAGS)
		WRITEUINT32(save_p, mobj->flags);
	if (diff & MD_FLAGS2)
		WRITEUINT32(save_p, mobj->flags2);
	if (diff & MD_HEALTH)
		WRITEINT32(save_p, mobj->health);
	if (diff & MD_RTIME)
		WRITEINT32(save_p, mobj->reactiontime);
	if (diff & MD_STATE)
		WRITEUINT16(save_p, mobj->state-states);
	if (diff & MD_TICS)
		WRITEINT32(save_p, mobj->tics);
	if (diff & MD_SPRITE) {
		WRITEUINT16(save_p, mobj->sprite);
		if (mobj->sprite == SPR_PLAY)
			WRITEUINT8(save_p, mobj->sprite2);
	}
	if (diff & MD_FRAME)
	{
		WRITEUINT32(save_p, mobj->frame);
		WRITEUINT16(save_p, mobj->anim_duration);
	}
	if (diff & MD_EFLAGS)
		WRITEUINT16(save_p, mobj->eflags);
	if (diff & MD_PLAYER)
		WRITEUINT8(save_p, mobj->player-players);
	if (diff & MD_MOVEDIR)
		WRITEANGLE(save_p, mobj->movedir);
	if (diff & MD_MOVECOUNT)
		WRITEINT32(save_p, mobj->movecount);
	if (diff & MD_THRESHOLD)
		WRITEINT32(save_p, mobj->threshold);
	if (diff & MD_LASTLOOK)
		WRITEINT32(save_p, mobj->lastlook);
	if (diff & MD_TARGET)
		WRITEUINT32(save_p, mobj->target->mobjnum);
	if (diff & MD_TRACER)
		WRITEUINT32(save_p, mobj->tracer->mobjnum);
	if (diff & MD_FRICTION)
		WRITEFIXED(save_p, mobj->friction);
	if (diff & MD_MOVEFACTOR)
		WRITEFIXED(save_p, mobj->movefactor);
	if (diff & MD_FUSE)
		WRITEINT32(save_p, mobj->fuse);
	if (diff & MD_WATERTOP)
		WRITEFIXED(save_p, mobj->watertop);
	if (diff & MD_WATERBOTTOM)
		WRITEFIXED(save_p, mobj->waterbottom);
	if (diff & MD_SCALE)
		WRITEFIXED(save_p, mobj->scale);
	if (diff & MD_DSCALE)
		WRITEFIXED(save_p, mobj->destscale);
	if (diff2 & MD2_SCALESPEED)
		WRITEFIXED(save_p, mobj->scalespeed);
	if (diff2 & MD2_CUSVAL)
		WRITEINT32(save_p, mobj->cusval);
	if (diff2 & MD2_CVMEM)
		WRITEINT32(save_p, mobj->cvmem);
	if (diff2 & MD2_SKIN)
		WRITEUINT8(save_p, (UINT8)((skin_t *)mobj->skin - skins));
	if (diff2 & MD2_COLOR)
		WRITEUINT16(save_p, mobj->color);
	if (diff2 & MD2_EXTVAL1)
		WRITEINT32(save_p, mobj->extravalue1);
	if (diff2 & MD2_EXTVAL2)
		WRITEINT32(save_p, mobj->extravalue2);
	if (diff2 & MD2_HNEXT)
		WRITEUINT32(save_p, mobj->hnext->mobjnum);
	if (diff2 & MD2_HPREV)
		WRITEUINT32(save_p, mobj->hprev->mobjnum);
	if (diff2 & MD2_SLOPE)
		WRITEUINT16(save_p, mobj->standingslope->id);
	if (diff2 & MD2_COLORIZED)
		WRITEUINT8(save_p, mobj->colorized);
	if (diff2 & MD2_MIRRORED)
		WRITEUINT8(save_p, mobj->mirrored);
	if (diff2 & MD2_ROLLANGLE)
		WRITEANGLE(save_p, mobj->rollangle);
	if (diff2 & MD2_SHADOWSCALE)
		WRITEFIXED(save_p, mobj->shadowscale);
	if (diff2 & MD2_RENDERFLAGS)
		WRITEUINT32(save_p, mobj->renderflags);
	if (diff2 & MD2_BLENDMODE)
		WRITEINT32(save_p, mobj->blendmode);
	if (diff2 & MD2_SPRITEXSCALE)
		WRITEFIXED(save_p, mobj->spritexscale);
	if (diff2 & MD2_SPRITEYSCALE)
		WRITEFIXED(save_p, mobj->spriteyscale);
	if (diff2 & MD2_SPRITEXOFFSET)
		WRITEFIXED(save_p, mobj->spritexoffset);
	if (diff2 & MD2_SPRITEYOFFSET)
		WRITEFIXED(save_p, mobj->spriteyoffset);
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = mobj->floorspriteslope;

		WRITEFIXED(save_p, slope->zdelta);
		WRITEANGLE(save_p, slope->zangle);
		WRITEANGLE(save_p, slope->xydirection);

		WRITEFIXED(save_p, slope->o.x);
		WRITEFIXED(save_p, slope->o.y);
		WRITEFIXED(save_p, slope->o.z);

		WRITEFIXED(save_p, slope->d.x);
		WRITEFIXED(save_p, slope->d.y);

		WRITEFIXED(save_p, slope->normal.x);
		WRITEFIXED(save_p, slope->normal.y);
		WRITEFIXED(save_p, slope->normal.z);
	}

	WRITEUINT32(save_p, mobj->mobjnum);
}

static void SaveNoEnemiesThinker(const thinker_t *th, const UINT8 type)
{
	const noenemies_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
}

static void SaveBounceCheeseThinker(const thinker_t *th, const UINT8 type)
{
	const bouncecheese_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->distance);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITECHAR(save_p, ht->low);
}

static void SaveContinuousFallThinker(const thinker_t *th, const UINT8 type)
{
	const continuousfall_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEFIXED(save_p, ht->destheight);
}

static void SaveMarioBlockThinker(const thinker_t *th, const UINT8 type)
{
	const mariothink_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEINT16(save_p, ht->tag);
}

static void SaveMarioCheckThinker(const thinker_t *th, const UINT8 type)
{
	const mariocheck_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
}

static void SaveThwompThinker(const thinker_t *th, const UINT8 type)
{
	const thwomp_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->crushspeed);
	WRITEFIXED(save_p, ht->retractspeed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEINT32(save_p, ht->delay);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT16(save_p, ht->sound);
}

static void SaveFloatThinker(const thinker_t *th, const UINT8 type)
{
	const floatthink_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT16(save_p, ht->tag);
}

static void SaveEachTimeThinker(const thinker_t *th, const UINT8 type)
{
	const eachtime_t *ht  = (const void *)th;
	size_t i;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITECHAR(save_p, ht->playersInArea[i]);
		WRITECHAR(save_p, ht->playersOnArea[i]);
	}
	WRITECHAR(save_p, ht->triggerOnExit);
}

static void SaveRaiseThinker(const thinker_t *th, const UINT8 type)
{
	const raise_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->ceilingbottom);
	WRITEFIXED(save_p, ht->ceilingtop);
	WRITEFIXED(save_p, ht->basespeed);
	WRITEFIXED(save_p, ht->extraspeed);
	WRITEUINT8(save_p, ht->shaketimer);
	WRITEUINT8(save_p, ht->flags);
}

static void SaveCeilingThinker(const thinker_t *th, const UINT8 type)
{
	const ceiling_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->bottomheight);
	WRITEFIXED(save_p, ht->topheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->oldspeed);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
	WRITEUINT8(save_p, ht->crush);
	WRITEINT32(save_p, ht->texture);
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->tag);
	WRITEINT32(save_p, ht->olddirection);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->sourceline);
}

static void SaveFloormoveThinker(const thinker_t *th, const UINT8 type)
{
	const floormove_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT8(save_p, ht->crush);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->texture);
	WRITEFIXED(save_p, ht->floordestheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
}

static void SaveLightflashThinker(const thinker_t *th, const UINT8 type)
{
	const lightflash_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->minlight);
}

static void SaveStrobeThinker(const thinker_t *th, const UINT8 type)
{
	const strobe_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->count);
	WRITEINT32(save_p, ht->minlight);
	WRITEINT32(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->darktime);
	WRITEINT32(save_p, ht->brighttime);
}

static void SaveGlowThinker(const thinker_t *th, const UINT8 type)
{
	const glow_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->minlight);
	WRITEINT32(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->speed);
}

static inline void SaveFireflickerThinker(const thinker_t *th, const UINT8 type)
{
	const fireflicker_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->count);
	WRITEINT32(save_p, ht->resetcount);
	WRITEINT32(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->minlight);
}

static void SaveElevatorThinker(const thinker_t *th, const UINT8 type)
{
	const elevator_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, SaveSector(ht->actionsector));
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floordestheight);
	WRITEFIXED(save_p, ht->ceilingdestheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->low);
	WRITEFIXED(save_p, ht->high);
	WRITEFIXED(save_p, ht->distance);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
}

static void SaveCrumbleThinker(const thinker_t *th, const UINT8 type)
{
	const crumble_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, SaveSector(ht->actionsector));
	WRITEUINT32(save_p, SavePlayer(ht->player)); // was dummy
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->origalpha);
	WRITEINT32(save_p, ht->timer);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITEUINT8(save_p, ht->flags);
}

static inline void SaveScrollThinker(const thinker_t *th, const UINT8 type)
{
	const scroll_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEFIXED(save_p, ht->dx);
	WRITEFIXED(save_p, ht->dy);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->control);
	WRITEFIXED(save_p, ht->last_height);
	WRITEFIXED(save_p, ht->vdx);
	WRITEFIXED(save_p, ht->vdy);
	WRITEINT32(save_p, ht->accel);
	WRITEINT32(save_p, ht->exclusive);
	WRITEUINT8(save_p, ht->type);
}

static inline void SaveFrictionThinker(const thinker_t *th, const UINT8 type)
{
	const friction_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->friction);
	WRITEINT32(save_p, ht->movefactor);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->referrer);
	WRITEUINT8(save_p, ht->roverfriction);
}

static inline void SavePusherThinker(const thinker_t *th, const UINT8 type)
{
	const pusher_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEINT32(save_p, ht->x_mag);
	WRITEINT32(save_p, ht->y_mag);
	WRITEINT32(save_p, ht->magnitude);
	WRITEINT32(save_p, ht->radius);
	WRITEINT32(save_p, ht->x);
	WRITEINT32(save_p, ht->y);
	WRITEINT32(save_p, ht->z);
	WRITEINT32(save_p, ht->affectee);
	WRITEUINT8(save_p, ht->roverpusher);
	WRITEINT32(save_p, ht->referrer);
	WRITEINT32(save_p, ht->exclusive);
	WRITEINT32(save_p, ht->slider);
}

static void SaveLaserThinker(const thinker_t *th, const UINT8 type)
{
	const laserthink_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT8(save_p, ht->nobosses);
}

static void SaveLightlevelThinker(const thinker_t *th, const UINT8 type)
{
	const lightlevel_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT16(save_p, ht->sourcelevel);
	WRITEINT16(save_p, ht->destlevel);
	WRITEFIXED(save_p, ht->fixedcurlevel);
	WRITEFIXED(save_p, ht->fixedpertic);
	WRITEINT32(save_p, ht->timer);
}

static void SaveExecutorThinker(const thinker_t *th, const UINT8 type)
{
	const executor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->line));
	WRITEUINT32(save_p, SaveMobjnum(ht->caller));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->timer);
}

static void SaveDisappearThinker(const thinker_t *th, const UINT8 type)
{
	const disappear_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, ht->appeartime);
	WRITEUINT32(save_p, ht->disappeartime);
	WRITEUINT32(save_p, ht->offset);
	WRITEUINT32(save_p, ht->timer);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->sourceline);
	WRITEINT32(save_p, ht->exists);
}

static void SaveFadeThinker(const thinker_t *th, const UINT8 type)
{
	const fade_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT32(save_p, ht->sectornum);
	WRITEUINT32(save_p, ht->ffloornum);
	WRITEINT32(save_p, ht->alpha);
	WRITEINT16(save_p, ht->sourcevalue);
	WRITEINT16(save_p, ht->destvalue);
	WRITEINT16(save_p, ht->destlightlevel);
	WRITEINT16(save_p, ht->speed);
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->timer);
	WRITEUINT8(save_p, ht->doexists);
	WRITEUINT8(save_p, ht->dotranslucent);
	WRITEUINT8(save_p, ht->dolighting);
	WRITEUINT8(save_p, ht->docolormap);
	WRITEUINT8(save_p, ht->docollision);
	WRITEUINT8(save_p, ht->doghostfade);
	WRITEUINT8(save_p, ht->exactalpha);
}

static void SaveFadeColormapThinker(const thinker_t *th, const UINT8 type)
{
	const fadecolormap_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->source_exc));
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->duration);
	WRITEINT32(save_p, ht->timer);
}

static void SavePlaneDisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const planedisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->control);
	WRITEFIXED(save_p, ht->last_height);
	WRITEFIXED(save_p, ht->speed);
	WRITEUINT8(save_p, ht->type);
}

static inline void SaveDynamicSlopeThinker(const thinker_t *th, const UINT8 type)
{
	const dynplanethink_t* ht = (const void*)th;

	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSlope(ht->slope));
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEFIXED(save_p, ht->extent);

	WRITEMEM(save_p, ht->tags, sizeof(ht->tags));
    WRITEMEM(save_p, ht->vex, sizeof(ht->vex));
}

static inline void SavePolyrotatetThinker(const thinker_t *th, const UINT8 type)
{
	const polyrotate_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT8(save_p, ht->turnobjs);
}

static void SavePolymoveThinker(const thinker_t *th, const UINT8 type)
{
	const polymove_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEFIXED(save_p, ht->momx);
	WRITEFIXED(save_p, ht->momy);
	WRITEINT32(save_p, ht->distance);
	WRITEANGLE(save_p, ht->angle);
}

static void SavePolywaypointThinker(const thinker_t *th, UINT8 type)
{
	const polywaypoint_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->sequence);
	WRITEINT32(save_p, ht->pointnum);
	WRITEINT32(save_p, ht->direction);
	WRITEUINT8(save_p, ht->returnbehavior);
	WRITEUINT8(save_p, ht->continuous);
	WRITEUINT8(save_p, ht->stophere);
}

static void SavePolyslidedoorThinker(const thinker_t *th, const UINT8 type)
{
	const polyslidedoor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->delay);
	WRITEINT32(save_p, ht->delayCount);
	WRITEINT32(save_p, ht->initSpeed);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->initDistance);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT32(save_p, ht->initAngle);
	WRITEUINT32(save_p, ht->angle);
	WRITEUINT32(save_p, ht->revAngle);
	WRITEFIXED(save_p, ht->momx);
	WRITEFIXED(save_p, ht->momy);
	WRITEUINT8(save_p, ht->closing);
}

static void SavePolyswingdoorThinker(const thinker_t *th, const UINT8 type)
{
	const polyswingdoor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->delay);
	WRITEINT32(save_p, ht->delayCount);
	WRITEINT32(save_p, ht->initSpeed);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->initDistance);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT8(save_p, ht->closing);
}

static void SavePolydisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const polydisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEUINT32(save_p, SaveSector(ht->controlSector));
	WRITEFIXED(save_p, ht->dx);
	WRITEFIXED(save_p, ht->dy);
	WRITEFIXED(save_p, ht->oldHeights);
}

static void SavePolyrotdisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const polyrotdisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEUINT32(save_p, SaveSector(ht->controlSector));
	WRITEFIXED(save_p, ht->rotscale);
	WRITEUINT8(save_p, ht->turnobjs);
	WRITEFIXED(save_p, ht->oldHeights);
}

static void SavePolyfadeThinker(const thinker_t *th, const UINT8 type)
{
	const polyfade_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->sourcevalue);
	WRITEINT32(save_p, ht->destvalue);
	WRITEUINT8(save_p, (UINT8)ht->docollision);
	WRITEUINT8(save_p, (UINT8)ht->doghostfade);
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->duration);
	WRITEINT32(save_p, ht->timer);
}

static void P_NetArchiveThinkers(void)
{
	const thinker_t *th;
	UINT32 i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_THINKERS);

	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		UINT32 numsaved = 0;
		// save off the current thinkers
		for (th = thlist[i].next; th != &thlist[i]; th = th->next)
		{
			if (!(th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed
			 || th->function.acp1 == (actionf_p1)P_NullPrecipThinker))
				numsaved++;
			else
				continue;

			if (th->function.acp1 == (actionf_p1)P_MobjThinker)
			{
				SaveMobjThinker(th, tc_mobj);
				continue;
			}
	#ifdef PARANOIA
			else if (th->function.acp1 == (actionf_p1)P_NullPrecipThinker);
	#endif
			else if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
			{
				SaveCeilingThinker(th, tc_ceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CrushCeiling)
			{
				SaveCeilingThinker(th, tc_crushceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveFloor)
			{
				SaveFloormoveThinker(th, tc_floor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightningFlash)
			{
				SaveLightflashThinker(th, tc_flash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
			{
				SaveStrobeThinker(th, tc_strobe);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Glow)
			{
				SaveGlowThinker(th, tc_glow);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FireFlicker)
			{
				SaveFireflickerThinker(th, tc_fireflicker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveElevator)
			{
				SaveElevatorThinker(th, tc_elevator);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ContinuousFalling)
			{
				SaveContinuousFallThinker(th, tc_continuousfalling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ThwompSector)
			{
				SaveThwompThinker(th, tc_thwomp);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_NoEnemiesSector)
			{
				SaveNoEnemiesThinker(th, tc_noenemies);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_EachTimeThinker)
			{
				SaveEachTimeThinker(th, tc_eachtime);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_RaiseSector)
			{
				SaveRaiseThinker(th, tc_raisesector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CameraScanner)
			{
				SaveElevatorThinker(th, tc_camerascanner);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Scroll)
			{
				SaveScrollThinker(th, tc_scroll);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Friction)
			{
				SaveFrictionThinker(th, tc_friction);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Pusher)
			{
				SavePusherThinker(th, tc_pusher);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_BounceCheese)
			{
				SaveBounceCheeseThinker(th, tc_bouncecheese);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StartCrumble)
			{
				SaveCrumbleThinker(th, tc_startcrumble);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlock)
			{
				SaveMarioBlockThinker(th, tc_marioblock);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlockChecker)
			{
				SaveMarioCheckThinker(th, tc_marioblockchecker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FloatSector)
			{
				SaveFloatThinker(th, tc_floatsector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LaserFlash)
			{
				SaveLaserThinker(th, tc_laserflash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightFade)
			{
				SaveLightlevelThinker(th, tc_lightfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ExecutorDelay)
			{
				SaveExecutorThinker(th, tc_executor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Disappear)
			{
				SaveDisappearThinker(th, tc_disappear);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Fade)
			{
				SaveFadeThinker(th, tc_fade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FadeColormap)
			{
				SaveFadeColormapThinker(th, tc_fadecolormap);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PlaneDisplace)
			{
				SavePlaneDisplaceThinker(th, tc_planedisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotate)
			{
				SavePolyrotatetThinker(th, tc_polyrotate);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjMove)
			{
				SavePolymoveThinker(th, tc_polymove);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjWaypoint)
			{
				SavePolywaypointThinker(th, tc_polywaypoint);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSlide)
			{
				SavePolyslidedoorThinker(th, tc_polyslidedoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSwing)
			{
				SavePolyswingdoorThinker(th, tc_polyswingdoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFlag)
			{
				SavePolymoveThinker(th, tc_polyflag);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjDisplace)
			{
				SavePolydisplaceThinker(th, tc_polydisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotDisplace)
			{
				SavePolyrotdisplaceThinker(th, tc_polyrotdisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFade)
			{
				SavePolyfadeThinker(th, tc_polyfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeLine)
			{
				SaveDynamicSlopeThinker(th, tc_dynslopeline);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeVert)
			{
				SaveDynamicSlopeThinker(th, tc_dynslopevert);
				continue;
			}
#ifdef PARANOIA
			else if (th->function.acp1 != (actionf_p1)P_RemoveThinkerDelayed) // wait garbage collection
				I_Error("unknown thinker type %p", th->function.acp1);
#endif
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers saved in list %d\n", numsaved, i);

		WRITEUINT8(save_p, tc_end);
	}
}

// Now save the pointers, tracer and target, but at load time we must
// relink to this; the savegame contains the old position in the pointer
// field copyed in the info field temporarily, but finally we just search
// for the old position and relink to it.
mobj_t *P_FindNewPosition(UINT32 oldposition)
{
	thinker_t *th;
	mobj_t *mobj;

	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)th;
		if (mobj->mobjnum != oldposition)
			continue;

		return mobj;
	}
	CONS_Debug(DBG_GAMELOGIC, "mobj not found\n");
	return NULL;
}

static inline mobj_t *LoadMobj(UINT32 mobjnum)
{
	if (mobjnum == 0) return NULL;
	return (mobj_t *)(size_t)mobjnum;
}

static sector_t *LoadSector(UINT32 sector)
{
	if (sector >= numsectors) return NULL;
	return &sectors[sector];
}

static line_t *LoadLine(UINT32 line)
{
	if (line >= numlines) return NULL;
	return &lines[line];
}

static inline player_t *LoadPlayer(UINT32 player)
{
	if (player >= MAXPLAYERS) return NULL;
	return &players[player];
}

static inline pslope_t *LoadSlope(UINT32 slopeid)
{
	pslope_t *p = slopelist;
	if (slopeid > slopecount) return NULL;
	do
	{
		if (p->id == slopeid)
			return p;
	} while ((p = p->next));
	return NULL;
}

static thinker_t* LoadMobjThinker(actionf_p1 thinker)
{
	thinker_t *next;
	mobj_t *mobj;
	UINT32 diff;
	UINT32 diff2;
	INT32 i;
	fixed_t z, floorz, ceilingz;
	ffloor_t *floorrover = NULL, *ceilingrover = NULL;

	diff = READUINT32(save_p);
	if (diff & MD_MORE)
		diff2 = READUINT32(save_p);
	else
		diff2 = 0;

	next = (void *)(size_t)READUINT32(save_p);

	z = READFIXED(save_p); // Force this so 3dfloor problems don't arise.
	floorz = READFIXED(save_p);
	ceilingz = READFIXED(save_p);

	if (diff2 & MD2_FLOORROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save_p));
		UINT16 id = READUINT16(save_p);
		floorrover = P_GetFFloorByID(sec, id);
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save_p));
		UINT16 id = READUINT16(save_p);
		ceilingrover = P_GetFFloorByID(sec, id);
	}

	if (diff & MD_SPAWNPOINT)
	{
		UINT16 spawnpointnum = READUINT16(save_p);

		if (mapthings[spawnpointnum].type == 1705 || mapthings[spawnpointnum].type == 1713) // NiGHTS Hoop special case
		{
			P_SpawnHoop(&mapthings[spawnpointnum]);
			return NULL;
		}

		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

		mobj->spawnpoint = &mapthings[spawnpointnum];
		mapthings[spawnpointnum].mobj = mobj;
	}
	else
		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

	// declare this as a valid mobj as soon as possible.
	mobj->thinker.function.acp1 = thinker;

	mobj->z = z;
	mobj->floorz = floorz;
	mobj->ceilingz = ceilingz;
	mobj->floorrover = floorrover;
	mobj->ceilingrover = ceilingrover;

	if (diff & MD_TYPE)
		mobj->type = READUINT32(save_p);
	else
	{
		for (i = 0; i < NUMMOBJTYPES; i++)
			if (mobj->spawnpoint && mobj->spawnpoint->type == mobjinfo[i].doomednum)
				break;
		if (i == NUMMOBJTYPES)
		{
			if (mobj->spawnpoint)
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type %d\n", mobj->spawnpoint->type);
			else
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type NULL\n");
			I_Error("Savegame corrupted");
		}
		mobj->type = i;
	}
	mobj->info = &mobjinfo[mobj->type];
	if (diff & MD_POS)
	{
		mobj->x = READFIXED(save_p);
		mobj->y = READFIXED(save_p);
		mobj->angle = READANGLE(save_p);
		mobj->pitch = READANGLE(save_p);
		mobj->roll = READANGLE(save_p);
	}
	else
	{
		mobj->x = mobj->spawnpoint->x << FRACBITS;
		mobj->y = mobj->spawnpoint->y << FRACBITS;
		mobj->angle = FixedAngle(mobj->spawnpoint->angle*FRACUNIT);
		mobj->pitch = FixedAngle(mobj->spawnpoint->pitch*FRACUNIT);
		mobj->roll = FixedAngle(mobj->spawnpoint->roll*FRACUNIT);
	}
	if (diff & MD_MOM)
	{
		mobj->momx = READFIXED(save_p);
		mobj->momy = READFIXED(save_p);
		mobj->momz = READFIXED(save_p);
		mobj->pmomz = READFIXED(save_p);
	} // otherwise they're zero, and the memset took care of it

	if (diff & MD_RADIUS)
		mobj->radius = READFIXED(save_p);
	else
		mobj->radius = mobj->info->radius;
	if (diff & MD_HEIGHT)
		mobj->height = READFIXED(save_p);
	else
		mobj->height = mobj->info->height;
	if (diff & MD_FLAGS)
		mobj->flags = READUINT32(save_p);
	else
		mobj->flags = mobj->info->flags;
	if (diff & MD_FLAGS2)
		mobj->flags2 = READUINT32(save_p);
	if (diff & MD_HEALTH)
		mobj->health = READINT32(save_p);
	else
		mobj->health = mobj->info->spawnhealth;
	if (diff & MD_RTIME)
		mobj->reactiontime = READINT32(save_p);
	else
		mobj->reactiontime = mobj->info->reactiontime;

	if (diff & MD_STATE)
		mobj->state = &states[READUINT16(save_p)];
	else
		mobj->state = &states[mobj->info->spawnstate];
	if (diff & MD_TICS)
		mobj->tics = READINT32(save_p);
	else
		mobj->tics = mobj->state->tics;
	if (diff & MD_SPRITE) {
		mobj->sprite = READUINT16(save_p);
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = READUINT8(save_p);
	}
	else {
		mobj->sprite = mobj->state->sprite;
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = mobj->state->frame&FF_FRAMEMASK;
	}
	if (diff & MD_FRAME)
	{
		mobj->frame = READUINT32(save_p);
		mobj->anim_duration = READUINT16(save_p);
	}
	else
	{
		mobj->frame = mobj->state->frame;
		mobj->anim_duration = (UINT16)mobj->state->var2;
	}
	if (diff & MD_EFLAGS)
		mobj->eflags = READUINT16(save_p);
	if (diff & MD_PLAYER)
	{
		i = READUINT8(save_p);
		mobj->player = &players[i];
		mobj->player->mo = mobj;
	}
	if (diff & MD_MOVEDIR)
		mobj->movedir = READANGLE(save_p);
	if (diff & MD_MOVECOUNT)
		mobj->movecount = READINT32(save_p);
	if (diff & MD_THRESHOLD)
		mobj->threshold = READINT32(save_p);
	if (diff & MD_LASTLOOK)
		mobj->lastlook = READINT32(save_p);
	else
		mobj->lastlook = -1;
	if (diff & MD_TARGET)
		mobj->target = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff & MD_TRACER)
		mobj->tracer = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff & MD_FRICTION)
		mobj->friction = READFIXED(save_p);
	else
		mobj->friction = ORIG_FRICTION;
	if (diff & MD_MOVEFACTOR)
		mobj->movefactor = READFIXED(save_p);
	else
		mobj->movefactor = FRACUNIT;
	if (diff & MD_FUSE)
		mobj->fuse = READINT32(save_p);
	if (diff & MD_WATERTOP)
		mobj->watertop = READFIXED(save_p);
	if (diff & MD_WATERBOTTOM)
		mobj->waterbottom = READFIXED(save_p);
	if (diff & MD_SCALE)
		mobj->scale = READFIXED(save_p);
	else
		mobj->scale = FRACUNIT;
	if (diff & MD_DSCALE)
		mobj->destscale = READFIXED(save_p);
	else
		mobj->destscale = mobj->scale;
	if (diff2 & MD2_SCALESPEED)
		mobj->scalespeed = READFIXED(save_p);
	else
		mobj->scalespeed = FRACUNIT/12;
	if (diff2 & MD2_CUSVAL)
		mobj->cusval = READINT32(save_p);
	if (diff2 & MD2_CVMEM)
		mobj->cvmem = READINT32(save_p);
	if (diff2 & MD2_SKIN)
		mobj->skin = &skins[READUINT8(save_p)];
	if (diff2 & MD2_COLOR)
		mobj->color = READUINT16(save_p);
	if (diff2 & MD2_EXTVAL1)
		mobj->extravalue1 = READINT32(save_p);
	if (diff2 & MD2_EXTVAL2)
		mobj->extravalue2 = READINT32(save_p);
	if (diff2 & MD2_HNEXT)
		mobj->hnext = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff2 & MD2_HPREV)
		mobj->hprev = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff2 & MD2_SLOPE)
		mobj->standingslope = P_SlopeById(READUINT16(save_p));
	if (diff2 & MD2_COLORIZED)
		mobj->colorized = READUINT8(save_p);
	if (diff2 & MD2_MIRRORED)
		mobj->mirrored = READUINT8(save_p);
	if (diff2 & MD2_ROLLANGLE)
		mobj->rollangle = READANGLE(save_p);
	if (diff2 & MD2_SHADOWSCALE)
		mobj->shadowscale = READFIXED(save_p);
	if (diff2 & MD2_RENDERFLAGS)
		mobj->renderflags = READUINT32(save_p);
	if (diff2 & MD2_BLENDMODE)
		mobj->blendmode = READINT32(save_p);
	else
		mobj->blendmode = AST_TRANSLUCENT;
	if (diff2 & MD2_SPRITEXSCALE)
		mobj->spritexscale = READFIXED(save_p);
	else
		mobj->spritexscale = FRACUNIT;
	if (diff2 & MD2_SPRITEYSCALE)
		mobj->spriteyscale = READFIXED(save_p);
	else
		mobj->spriteyscale = FRACUNIT;
	if (diff2 & MD2_SPRITEXOFFSET)
		mobj->spritexoffset = READFIXED(save_p);
	if (diff2 & MD2_SPRITEYOFFSET)
		mobj->spriteyoffset = READFIXED(save_p);
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = (pslope_t *)P_CreateFloorSpriteSlope(mobj);

		slope->zdelta = READFIXED(save_p);
		slope->zangle = READANGLE(save_p);
		slope->xydirection = READANGLE(save_p);

		slope->o.x = READFIXED(save_p);
		slope->o.y = READFIXED(save_p);
		slope->o.z = READFIXED(save_p);

		slope->d.x = READFIXED(save_p);
		slope->d.y = READFIXED(save_p);

		slope->normal.x = READFIXED(save_p);
		slope->normal.y = READFIXED(save_p);
		slope->normal.z = READFIXED(save_p);
	}

	if (diff & MD_REDFLAG)
	{
		redflag = mobj;
		rflagpoint = mobj->spawnpoint;
	}
	if (diff & MD_BLUEFLAG)
	{
		blueflag = mobj;
		bflagpoint = mobj->spawnpoint;
	}

	// set sprev, snext, bprev, bnext, subsector
	P_SetThingPosition(mobj);

	mobj->mobjnum = READUINT32(save_p);

	if (mobj->player)
	{
		if (mobj->eflags & MFE_VERTICALFLIP)
			mobj->player->viewz = mobj->z + mobj->height - mobj->player->viewheight;
		else
			mobj->player->viewz = mobj->player->mo->z + mobj->player->viewheight;
	}

	mobj->info = (mobjinfo_t *)next; // temporarily, set when leave this function

	return &mobj->thinker;
}

static thinker_t* LoadNoEnemiesThinker(actionf_p1 thinker)
{
	noenemies_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	return &ht->thinker;
}

static thinker_t* LoadBounceCheeseThinker(actionf_p1 thinker)
{
	bouncecheese_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->distance = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->low = READCHAR(save_p);

	if (ht->sector)
		ht->sector->ceilingdata = ht;

	return &ht->thinker;
}

static thinker_t* LoadContinuousFallThinker(actionf_p1 thinker)
{
	continuousfall_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->destheight = READFIXED(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioBlockThinker(actionf_p1 thinker)
{
	mariothink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->tag = READINT16(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioCheckThinker(actionf_p1 thinker)
{
	mariocheck_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	return &ht->thinker;
}

static thinker_t* LoadThwompThinker(actionf_p1 thinker)
{
	thwomp_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->crushspeed = READFIXED(save_p);
	ht->retractspeed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->delay = READINT32(save_p);
	ht->tag = READINT16(save_p);
	ht->sound = READUINT16(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadFloatThinker(actionf_p1 thinker)
{
	floatthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->tag = READINT16(save_p);
	return &ht->thinker;
}

static thinker_t* LoadEachTimeThinker(actionf_p1 thinker)
{
	size_t i;
	eachtime_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		ht->playersInArea[i] = READCHAR(save_p);
		ht->playersOnArea[i] = READCHAR(save_p);
	}
	ht->triggerOnExit = READCHAR(save_p);
	return &ht->thinker;
}

static thinker_t* LoadRaiseThinker(actionf_p1 thinker)
{
	raise_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->ceilingbottom = READFIXED(save_p);
	ht->ceilingtop = READFIXED(save_p);
	ht->basespeed = READFIXED(save_p);
	ht->extraspeed = READFIXED(save_p);
	ht->shaketimer = READUINT8(save_p);
	ht->flags = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadCeilingThinker(actionf_p1 thinker)
{
	ceiling_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->bottomheight = READFIXED(save_p);
	ht->topheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->oldspeed = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	ht->crush = READUINT8(save_p);
	ht->texture = READINT32(save_p);
	ht->direction = READINT32(save_p);
	ht->tag = READINT32(save_p);
	ht->olddirection = READINT32(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->sourceline = READFIXED(save_p);
	if (ht->sector)
		ht->sector->ceilingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFloormoveThinker(actionf_p1 thinker)
{
	floormove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->crush = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->texture = READINT32(save_p);
	ht->floordestheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	if (ht->sector)
		ht->sector->floordata = ht;
	return &ht->thinker;
}

static thinker_t* LoadLightflashThinker(actionf_p1 thinker)
{
	lightflash_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->maxlight = READINT32(save_p);
	ht->minlight = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadStrobeThinker(actionf_p1 thinker)
{
	strobe_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->count = READINT32(save_p);
	ht->minlight = READINT32(save_p);
	ht->maxlight = READINT32(save_p);
	ht->darktime = READINT32(save_p);
	ht->brighttime = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadGlowThinker(actionf_p1 thinker)
{
	glow_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->minlight = READINT32(save_p);
	ht->maxlight = READINT32(save_p);
	ht->direction = READINT32(save_p);
	ht->speed = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFireflickerThinker(actionf_p1 thinker)
{
	fireflicker_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->count = READINT32(save_p);
	ht->resetcount = READINT32(save_p);
	ht->maxlight = READINT32(save_p);
	ht->minlight = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadElevatorThinker(actionf_p1 thinker, boolean setplanedata)
{
	elevator_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->actionsector = LoadSector(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->floordestheight = READFIXED(save_p);
	ht->ceilingdestheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->low = READFIXED(save_p);
	ht->high = READFIXED(save_p);
	ht->distance = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->sourceline = LoadLine(READUINT32(save_p));

	if (ht->sector && setplanedata)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadCrumbleThinker(actionf_p1 thinker)
{
	crumble_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->actionsector = LoadSector(READUINT32(save_p));
	ht->player = LoadPlayer(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->origalpha = READINT32(save_p);
	ht->timer = READINT32(save_p);
	ht->speed = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->flags = READUINT8(save_p);

	if (ht->sector)
		ht->sector->floordata = ht;

	return &ht->thinker;
}

static thinker_t* LoadScrollThinker(actionf_p1 thinker)
{
	scroll_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dx = READFIXED(save_p);
	ht->dy = READFIXED(save_p);
	ht->affectee = READINT32(save_p);
	ht->control = READINT32(save_p);
	ht->last_height = READFIXED(save_p);
	ht->vdx = READFIXED(save_p);
	ht->vdy = READFIXED(save_p);
	ht->accel = READINT32(save_p);
	ht->exclusive = READINT32(save_p);
	ht->type = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadFrictionThinker(actionf_p1 thinker)
{
	friction_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->friction = READINT32(save_p);
	ht->movefactor = READINT32(save_p);
	ht->affectee = READINT32(save_p);
	ht->referrer = READINT32(save_p);
	ht->roverfriction = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPusherThinker(actionf_p1 thinker)
{
	pusher_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->x_mag = READINT32(save_p);
	ht->y_mag = READINT32(save_p);
	ht->magnitude = READINT32(save_p);
	ht->radius = READINT32(save_p);
	ht->x = READINT32(save_p);
	ht->y = READINT32(save_p);
	ht->z = READINT32(save_p);
	ht->affectee = READINT32(save_p);
	ht->roverpusher = READUINT8(save_p);
	ht->referrer = READINT32(save_p);
	ht->exclusive = READINT32(save_p);
	ht->slider = READINT32(save_p);
	ht->source = P_GetPushThing(ht->affectee);
	return &ht->thinker;
}

static inline thinker_t* LoadLaserThinker(actionf_p1 thinker)
{
	laserthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save_p);
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->nobosses = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadLightlevelThinker(actionf_p1 thinker)
{
	lightlevel_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->sourcelevel = READINT16(save_p);
	ht->destlevel = READINT16(save_p);
	ht->fixedcurlevel = READFIXED(save_p);
	ht->fixedpertic = READFIXED(save_p);
	ht->timer = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadExecutorThinker(actionf_p1 thinker)
{
	executor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->line = LoadLine(READUINT32(save_p));
	ht->caller = LoadMobj(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->timer = READINT32(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadDisappearThinker(actionf_p1 thinker)
{
	disappear_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->appeartime = READUINT32(save_p);
	ht->disappeartime = READUINT32(save_p);
	ht->offset = READUINT32(save_p);
	ht->timer = READUINT32(save_p);
	ht->affectee = READINT32(save_p);
	ht->sourceline = READINT32(save_p);
	ht->exists = READINT32(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadFadeThinker(actionf_p1 thinker)
{
	sector_t *ss;
	fade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dest_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->sectornum = READUINT32(save_p);
	ht->ffloornum = READUINT32(save_p);
	ht->alpha = READINT32(save_p);
	ht->sourcevalue = READINT16(save_p);
	ht->destvalue = READINT16(save_p);
	ht->destlightlevel = READINT16(save_p);
	ht->speed = READINT16(save_p);
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->timer = READINT32(save_p);
	ht->doexists = READUINT8(save_p);
	ht->dotranslucent = READUINT8(save_p);
	ht->dolighting = READUINT8(save_p);
	ht->docolormap = READUINT8(save_p);
	ht->docollision = READUINT8(save_p);
	ht->doghostfade = READUINT8(save_p);
	ht->exactalpha = READUINT8(save_p);

	ss = LoadSector(ht->sectornum);
	if (ss)
	{
		size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
		ffloor_t *rover;
		for (rover = ss->ffloors; rover; rover = rover->next)
		{
			if (j == ht->ffloornum)
			{
				ht->rover = rover;
				rover->fadingdata = ht;
				break;
			}
			j++;
		}
	}
	return &ht->thinker;
}

static inline thinker_t* LoadFadeColormapThinker(actionf_p1 thinker)
{
	fadecolormap_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->source_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->dest_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->duration = READINT32(save_p);
	ht->timer = READINT32(save_p);
	if (ht->sector)
		ht->sector->fadecolormapdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadPlaneDisplaceThinker(actionf_p1 thinker)
{
	planedisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->affectee = READINT32(save_p);
	ht->control = READINT32(save_p);
	ht->last_height = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->type = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadDynamicSlopeThinker(actionf_p1 thinker)
{
	dynplanethink_t* ht = Z_Malloc(sizeof(*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->type = READUINT8(save_p);
	ht->slope = LoadSlope(READUINT32(save_p));
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->extent = READFIXED(save_p);
	READMEM(save_p, ht->tags, sizeof(ht->tags));
	READMEM(save_p, ht->vex, sizeof(ht->vex));
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotatetThinker(actionf_p1 thinker)
{
	polyrotate_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->turnobjs = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPolymoveThinker(actionf_p1 thinker)
{
	polymove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->momx = READFIXED(save_p);
	ht->momy = READFIXED(save_p);
	ht->distance = READINT32(save_p);
	ht->angle = READANGLE(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolywaypointThinker(actionf_p1 thinker)
{
	polywaypoint_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->sequence = READINT32(save_p);
	ht->pointnum = READINT32(save_p);
	ht->direction = READINT32(save_p);
	ht->returnbehavior = READUINT8(save_p);
	ht->continuous = READUINT8(save_p);
	ht->stophere = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyslidedoorThinker(actionf_p1 thinker)
{
	polyslidedoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->delay = READINT32(save_p);
	ht->delayCount = READINT32(save_p);
	ht->initSpeed = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->initDistance = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->initAngle = READUINT32(save_p);
	ht->angle = READUINT32(save_p);
	ht->revAngle = READUINT32(save_p);
	ht->momx = READFIXED(save_p);
	ht->momy = READFIXED(save_p);
	ht->closing = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyswingdoorThinker(actionf_p1 thinker)
{
	polyswingdoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->delay = READINT32(save_p);
	ht->delayCount = READINT32(save_p);
	ht->initSpeed = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->initDistance = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->closing = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolydisplaceThinker(actionf_p1 thinker)
{
	polydisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->controlSector = LoadSector(READUINT32(save_p));
	ht->dx = READFIXED(save_p);
	ht->dy = READFIXED(save_p);
	ht->oldHeights = READFIXED(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotdisplaceThinker(actionf_p1 thinker)
{
	polyrotdisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->controlSector = LoadSector(READUINT32(save_p));
	ht->rotscale = READFIXED(save_p);
	ht->turnobjs = READUINT8(save_p);
	ht->oldHeights = READFIXED(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPolyfadeThinker(actionf_p1 thinker)
{
	polyfade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->sourcevalue = READINT32(save_p);
	ht->destvalue = READINT32(save_p);
	ht->docollision = (boolean)READUINT8(save_p);
	ht->doghostfade = (boolean)READUINT8(save_p);
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->duration = READINT32(save_p);
	ht->timer = READINT32(save_p);
	return &ht->thinker;
}

/*
//
// LoadWhatThinker
//
// load a what_t thinker
//
static inline void LoadWhatThinker(actionf_p1 thinker)
{
	what_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
}
*/

// debug lists for the confused programmer
thinker_t* debugThinkerLists[NUM_THINKERLISTS][4096];
mobj_t* debugMobjLists[NUM_THINKERLISTS][4096];
int numDebugThinkers[NUM_THINKERLISTS];

static void CollectDebugObjectList(void) {
	int i;
	for (i = 0; i < NUM_THINKERLISTS; i++) {
		int count = 0;
		thinker_t* test;
		thinker_t* testNext;
		for (test = thlist[i].next; test != &thlist[i]; test = testNext) {
			testNext = test->next;
			debugThinkerLists[i][count] = test;
			debugMobjLists[i][count] = (mobj_t*)test;
			count++;
		}
		numDebugThinkers[i] = count;
	}
}

void S_DetachChannelsFromOrigin(void* origin);

typedef struct
{
	mobj_t* mobj;
	int thinkerListIndex;
} mobjloclink_t;

#define HASHLOC(x, y, z) (((x>>4)-(y>>8)+(z>>12)-(x>>16)+(y>>20)-(z>>24)+(x>>28)-(y>>24)+(z>>20)-(x>>16)+(y>>8)-(z>>4)) & 0x7FFF)
#define MAXNUMMOBJSBYLOC 15

//
// P_LocalArchiveThinkers
// Archives the world's thinkers locally. Used for within-level savestates
//
static void P_LocalArchiveThinkers(void)
{
	int i, j;
	size_t s;
	const thinker_t* thinker;
	mobj_t* savedMobj;
	UINT8* original = save_p;
	WRITEUINT32(save_p, ARCHIVEBLOCK_THINKERS);

	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
// #define RELINK(var) if (var != NULL) var = (mobj_t*)(var->mobjnum)
//thanks to GoldenTails for pointing out some nuances
#define RELINK(var) if (var && var->mobjnum) var = (mobj_t*)(var->mobjnum) 
		for (thinker = thlist[i].next; thinker != &thlist[i]; thinker = thinker->next)
		{
			actionf_p1 acp1 = thinker->function.acp1;
			if (acp1 == (actionf_p1)P_RemoveThinkerDelayed
			  || acp1 == (actionf_p1)P_NullPrecipThinker)
				continue;

			// find the associated special def
			for (j = 0; j < sizeof(specialDefs) / sizeof(specialDefs[0]); j++)
			{
				if (acp1 == specialDefs[j].action)
					break;
			}

			if (j == sizeof(specialDefs) / sizeof(specialDefs[0]))
				continue;

			WRITEUINT8(save_p, j);
			WRITEMEM(save_p, thinker, specialDefs[j].size);

			// relink saved pointers with saved mobjnums
			if (j == tc_mobj)
			{
				savedMobj = &((mobj_t*)save_p)[-1];
				RELINK(savedMobj->tracer);
				RELINK(savedMobj->target);
				RELINK(savedMobj->hnext);
				RELINK(savedMobj->hprev);
			}

			if (j == tc_executor)
			{
				executor_t* savedExecutor;
				savedExecutor = &((executor_t*)save_p)[-1];
				RELINK(savedExecutor->caller);
			}

			if (j == tc_polywaypoint)
			{
				polywaypoint_t* savedWaypoint = &((polywaypoint_t*)save_p)[-1];
				RELINK(savedWaypoint->target);
			}
		}

		WRITEUINT8(save_p, 0xFF); // next list (or end)
	}

	// now write the nothinkers...
	// turns out that MF_NOTHINKs aren't in thinker lists lol....
	// makes sense, but doesn't make life easier. I just wanted an object list. Ah well.
	for (s = 0; s < numsectors; s++)
	{
		mobj_t* thing = sectors[s].thinglist;

		while (thing)
		{
			if (thing->flags & MF_NOTHINK)
			{
				// archive this thing
				WRITEUINT8(save_p, tc_mobj);
				WRITEMEM(save_p, thing, specialDefs[tc_mobj].size);

				savedMobj = &((mobj_t*)save_p)[-1];
				RELINK(savedMobj->tracer);
				RELINK(savedMobj->target);
				RELINK(savedMobj->hnext);
				RELINK(savedMobj->hprev);
			}
			thing = thing->snext;
		}
	}
#undef RELINK

	WRITEUINT8(save_p, 0xFF);
}

static void P_LocalUnArchiveThinkers()
{
	thinker_t *thinker;
	mobj_t* mobj;
	mobj_t* currentmobj;
	UINT8 tclass;
	UINT8 restoreAdditionalStuff = false;
	int32_t i, j, list;
	int skyviewid = 0;
	int skycenterid = 0;
	thinker_t newthinkers[NUM_THINKERLISTS] = { 0 };
	static thinker_t* thinkersbytype[tc_end][16384];
	static uint32_t numthinkersbytype[tc_end];
	static mobjloclink_t mobjByLoc[32768][MAXNUMMOBJSBYLOC];
	static int numMobjsByLoc[32768];
	static mobj_t* mobjByNum[16384];

	memset(mobjByNum, 0, sizeof(mobjByNum));
	memset(mobjByLoc, 0, sizeof(mobjByLoc));
	memset(numMobjsByLoc, 0, sizeof(numMobjsByLoc));
	memset(numthinkersbytype, 0, sizeof(numthinkersbytype));

	int hashHits = 0;
	int hashMisses = 0;

	if (READUINT32(save_p) != ARCHIVEBLOCK_THINKERS)
		I_Error("Bad SaveState at archive block Thinkers");

	// preserve sky box indexes
	for (i = 0; i < 16; i++)
	{	
		if (skyboxmo[0] && skyboxmo[0] == skyboxviewpnts[i])
			skyviewid = i; // save id just in case
		if (skyboxmo[1] && skyboxmo[1] == skyboxcenterpnts[i])
			skycenterid = i; // save id just in case
	}

	// sort thinkers into maps to speed up replacement searches
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		newthinkers[i].next = &newthinkers[i];
		newthinkers[i].prev = &newthinkers[i];

		for (thinker = thlist[i].next; thinker != &thlist[i]; thinker = thinker->next)
		{
			if (thinker->function.acp1 == P_RemoveThinkerDelayed || thinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
				continue;
			for (j = 0; j < tc_end; j++)
			{
				if (thinker->function.acp1 == specialDefs[j].action)
					break;
			}

			// insert the thinker into the appropriate maps
			thinkersbytype[j][numthinkersbytype[j]] = thinker;
			numthinkersbytype[j]++;

			if (thinker->function.acp1 == P_MobjThinker)
			{
				UINT16 locHash = HASHLOC(((mobj_t*)thinker)->x, ((mobj_t*)thinker)->y, ((mobj_t*)thinker)->z);

				if (numMobjsByLoc[locHash] < MAXNUMMOBJSBYLOC)
				{
					mobjByLoc[locHash][numMobjsByLoc[locHash]].mobj = (mobj_t*)thinker;
					mobjByLoc[locHash][numMobjsByLoc[locHash]].thinkerListIndex = numthinkersbytype[j] - 1;
					numMobjsByLoc[locHash]++;
				}
			}
		}
	}

	// remove from item respawn queues
	iquetail = iquehead = 0;

	// clear sector thinker pointers so they don't point to non-existant thinkers for all of eternity
	for (i = 0; i < (int)numsectors; i++)
	{
		mobj_t* thing = sectors[i].thinglist, *next;
		sectors[i].floordata = sectors[i].ceilingdata = sectors[i].lightingdata = sectors[i].fadecolormapdata = NULL;

		// clear nothinkers too
		while (thing)
		{
			next = thing->snext;
			if (thing->flags & MF_NOTHINK)
			{
				thing->thinker.references = 0; // hope this is ok...
				P_RemoveMobj(thing);
			}

			thing = next;
		}
	}

	// read in saved thinkers
	for (list = 0; list < NUM_THINKERLISTS; list++)
	{
		for (;;)
		{
			msecnode_t* preserveTouchingSectorlist = NULL;
			sector_t* preserveSubsector = NULL;

			tclass = READUINT8(save_p);
			
			if (tclass == 0xFF)
				break; // next list

			thinker_t* newthinker = NULL;
			boolean preservePositions = false;

			// find the existing thinker(s) at the same position, if possible
			if (tclass == tc_mobj)
			{
				mobj_t* savedMobj = (mobj_t*)save_p;
				UINT16 locHash = HASHLOC(savedMobj->x, savedMobj->y, savedMobj->z);

				for (i = 0; i < numMobjsByLoc[locHash]; i++)
				{
					if (mobjByLoc[locHash][i].mobj->x == savedMobj->x && mobjByLoc[locHash][i].mobj->y == savedMobj->y && mobjByLoc[locHash][i].mobj->z == savedMobj->z && 
						(mobjByLoc[locHash][i].mobj->flags & (MF_NOSECTOR | MF_NOBLOCKMAP)) == (savedMobj->flags & (MF_NOSECTOR | MF_NOBLOCKMAP)))
					{
						newthinker = (thinker_t*)mobjByLoc[locHash][i].mobj;
						thinkersbytype[tc_mobj][mobjByLoc[locHash][i].thinkerListIndex] = NULL;
						mobjByLoc[locHash][i] = mobjByLoc[locHash][numMobjsByLoc[locHash] - 1];
						numMobjsByLoc[locHash]--;
						preservePositions = true;
						hashHits++;
						break;
					}
				}

				if (newthinker == NULL)
					hashMisses++;
			}

			// if that didn't work, find an existing thinker of the same type
			if (newthinker == NULL && numthinkersbytype[tclass] > 0)
			{
				for (j = numthinkersbytype[tclass] - 1; j >= 0; j--)
				{
					if (!thinkersbytype[tclass][j])
					{
						numthinkersbytype[tclass]--;
						continue;
					}

					// we found an object to replace!
					newthinker = thinkersbytype[tclass][j];
					numthinkersbytype[tclass]--;
					break;
				}
			}

			if (newthinker != NULL)
			{
				// we found a thinker! we just need to set it up to be replaced
				if (tclass == tc_mobj && !preservePositions)
				{
					// remove it from its old position
					P_UnsetThingPosition((mobj_t*)newthinker);
					if (sector_list != NULL)
					{
						P_DelSeclist(sector_list);
						sector_list = NULL;
					}
				}

				// // detach any playing sounds from this object if we can (still need to work on doing this properly!)
				// S_DetachChannelsFromOrigin((mobj_t*)newthinker);

				// unlink it from the old list (if it's not a nothinker)
				if (!(tclass == tc_mobj && (((mobj_t*)newthinker)->flags & MF_NOTHINK)))
				{
					newthinker->prev->next = newthinker->next;
					newthinker->next->prev = newthinker->prev;
				}
			}
			else
			{
				// we couldn't find an object to replace so we gotta create it
				if (tclass == tc_mobj)
					newthinker = Z_Calloc(specialDefs[tclass].size, PU_LEVEL, NULL);
				else
					newthinker = Z_Malloc(specialDefs[tclass].size, PU_LEVSPEC, NULL);
			}

			// preserve vital positioning stuff (cleanup...)
			static mobj_t preservedMobj;
			if (tclass == tc_mobj)
				preservedMobj = *(mobj_t*)newthinker;

			// read in the data
			READMEM(save_p, newthinker, specialDefs[tclass].size);

			newthinker->references = 0; // we'll sort this later

			// set special-specific info
			switch (tclass)
			{
				case tc_mobj:
				{
					mobj_t* mobj = (mobj_t*)newthinker;

					// link global and player pointers
					if (mobj->type == MT_REDFLAG)
					{
						redflag = mobj;
						rflagpoint = mobj->spawnpoint;
					}

					if (mobj->type == MT_BLUEFLAG)
					{
						blueflag = mobj;
						bflagpoint = mobj->spawnpoint;
					}

					if (mobj->type == MT_SKYBOX)
					{
						if ((mobj->extravalue2 >> 16) == 1)
							skyboxcenterpnts[mobj->extravalue2 & 0xFFFF] = mobj;
						else
							skyboxviewpnts[mobj->extravalue2 & 0xFFFF] = mobj;
					}

					if (mobj->player && mobj->type == MT_PLAYER)
						mobj->player->mo = mobj;

					// clear sector links etc
					mobj->bnext = NULL;
					mobj->bprev = NULL;
					mobj->snext = preservedMobj.snext;
					mobj->sprev = preservedMobj.sprev;
					mobj->subsector = preservedMobj.subsector;
					mobj->touching_sectorlist = preservedMobj.touching_sectorlist;
					mobjByNum[mobj->mobjnum] = mobj;
					break; // i dream of a programming language where breaking is the default and fallthrough is the optional (and so much more rarely used) specifier
				}
				case tc_executor:
				case tc_polywaypoint:
					restoreAdditionalStuff = true;
					break;
			}

			// make special-specific sector connections
			if (specialDefs[tclass].sectorTargets)
			{
				sector_t* target = *(sector_t**)((UINT8*)newthinker + specialDefs[tclass].sectorPointer);

				if (specialDefs[tclass].sectorTargets & ST_CEILDATA)
					target->ceilingdata = newthinker;
				if (specialDefs[tclass].sectorTargets & ST_FLOORDATA)
					target->floordata = newthinker;
				if (specialDefs[tclass].sectorTargets & ST_LIGHTDATA)
					target->lightingdata = newthinker;
				if (specialDefs[tclass].sectorTargets & ST_FADEMAPDATA)
					target->fadecolormapdata = newthinker;
			}

			// relink into the new thinker list
			if (!(tclass == tc_mobj && (((mobj_t*)newthinker)->flags & MF_NOTHINK)))
			{
				newthinker->prev = newthinkers[list].prev;
				newthinker->next = &newthinkers[list];
				newthinker->prev->next = newthinker;
				newthinker->next->prev = newthinker;
			}

			if (tclass == tc_mobj)
			{
				if (!preservePositions)
					P_SetThingPosition((mobj_t*)newthinker);
				else
				{
					((mobj_t*)newthinker)->bnext = preservedMobj.bnext;
					((mobj_t*)newthinker)->bprev = preservedMobj.bprev;
				}
			}
		}
	}

	// collect nothinkers as well
	for (;;)
	{
		tclass = READUINT8(save_p);

		if (tclass == 0xFF)
			break; // next list

		mobj = Z_Calloc(sizeof(mobj_t), PU_LEVEL, NULL);
		READMEM(save_p, mobj, specialDefs[tclass].size);

		mobjByNum[mobj->mobjnum] = mobj;
		mobj->bnext = NULL;
		mobj->bprev = NULL;
		mobj->snext = NULL;
		mobj->sprev = NULL;
		mobj->subsector = NULL;
		mobj->touching_sectorlist = NULL;
		P_SetThingPosition(mobj);

		// we just spawner a nothinker so it doesn't go in a thinker list, let it run wild??
	}

	// delete everything remaining in the list that hasn't been linked
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		thinker_t* next;
		int ind = 0;

		for (thinker = thlist[i].next; thinker != &thlist[i]; thinker = next)
		{
			next = thinker->next;

			ind++;
			thinker->references = 0;

			if (thinker->function.acp1 == (actionf_p1)P_MobjThinker)
				P_RemoveSavegameMobj((mobj_t *)thinker, true);
			else if (thinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
				// P_RemovePrecipMobj((precipmobj_t*)thinker); // i bet this is gonna break :D
				// the problem was in using NetUnarchiving, we have to use our function to fix the weather
				continue;
			else
			{
				// we can remove this thinker manually, bye!
				thinker->prev->next = thinker->next;
				thinker->next->prev = thinker->prev;
				Z_Free(thinker);
			}
		}

		// connect the new linker list to the old linker list
		newthinkers[i].prev->next = thlist[i].next;
		thlist[i].next->prev = newthinkers[i].prev;

		thlist[i].next = newthinkers[i].next;
		newthinkers[i].next->prev = &thlist[i];
	}

	// restore execution stuff I guess lol
	if (restoreAdditionalStuff)
	{
		executor_t *delay = NULL;
		polywaypoint_t* polywp = NULL;
		UINT32 mobjnum;
		for (thinker = thlist[THINK_MAIN].next; thinker != &thlist[THINK_MAIN]; thinker = thinker->next)
		{
			if (thinker->function.acp1 != (actionf_p1)T_ExecutorDelay)
				continue;
			delay = (void *)thinker;
			if (!(mobjnum = (UINT32)(size_t)delay->caller))
				continue;
			delay->caller = mobjByNum[mobjnum];
		}
		for (thinker = thlist[THINK_POLYOBJ].next; thinker != &thlist[THINK_POLYOBJ]; thinker = thinker->next)
		{
			if (thinker->function.acp1 != (actionf_p1)T_PolyObjWaypoint)
				continue;
			polywp = (void*)thinker;
			if (!(mobjnum = (UINT32)(size_t)polywp->target))
				continue;
			polywp->target = mobjByNum[mobjnum];
		}
	}

	for (i = 0; i < sizeof(skyboxmo) / sizeof(skyboxmo[0]); i++)
		skyboxmo[i] = NULL;
	for (i = 0; i < sizeof(skyboxcenterpnts) / sizeof(skyboxcenterpnts[0]); i++)
	{
		skyboxcenterpnts[i] = NULL;
		skyboxviewpnts[i] = NULL;
	}

	// restore pointers
#define RELINK(var) if (var) { UINT32 index = (UINT32)var; var = NULL; P_SetTarget(&var, mobjByNum[index]); }
	for (thinker = thlist[THINK_MOBJ].next; thinker != &thlist[THINK_MOBJ]; thinker = thinker->next)
	{
		if (thinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t*)thinker;

		RELINK(mobj->tracer);
		RELINK(mobj->target);
		RELINK(mobj->hnext);
		RELINK(mobj->hprev);
	}

	// and for nothinkers
	for (i = 0; i < (int)numsectors; i++)
	{
		mobj_t* thing = sectors[i].thinglist;

		while (thing)
		{
			if (thing->flags & MF_NOTHINK)
			{
				RELINK(thing->tracer);
				RELINK(thing->target);
				RELINK(thing->hnext);
				RELINK(thing->hprev);
			}

			thing = thing->snext;
		}
	}

	// restore player pointers (the above doesn't always seem to work perhaps because playeringame is false?
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			RELINK(players[i].capsule);
			RELINK(players[i].axis1);
			RELINK(players[i].axis2);
			RELINK(players[i].awayviewmobj);
			RELINK(players[i].followmobj);
			RELINK(players[i].drone);
		}
	}
#undef RELINK

	// restore skyboxes



	for (currentmobj = (mobj_t*)thlist[THINK_MOBJ].next; currentmobj != (mobj_t*)&thlist[THINK_MOBJ]; currentmobj = (mobj_t*)currentmobj->thinker.next)
	{
		if (currentmobj->type == MT_SKYBOX)
		{
			if ((currentmobj->extravalue2 >> 16) == 1)
			{
				skyboxcenterpnts[currentmobj->extravalue2 & 0xFFFF] = currentmobj;
			}
			else
			{
				skyboxviewpnts[currentmobj->extravalue2 & 0xFFFF] = currentmobj;
			}
		}
	}

	skyboxmo[0] = skyboxviewpnts[(skyviewid >= 0) ? skyviewid : 0];
	skyboxmo[1] = skyboxcenterpnts[(skycenterid >= 0) ? skycenterid : 0];
}

// static void P_LocalUnArchiveThinkers(boolean preserveLevel)
// {
// 	thinker_t *currentthinker;
// 	thinker_t *next;
// 	mobj_t* currentmobj;
// 	UINT8 tclass;
// 	UINT8 restoreNum = false;
// 	UINT32 i;
// 	UINT32 numloaded = 0;
// 	int skyviewid = -1;
// 	int skycenterid = -1;

// 	if (READUINT32(save_p) != ARCHIVEBLOCK_THINKERS)
// 		I_Error("Bad $$$.sav at archive block Thinkers");

// 	// preserve sky box index
// 	for (i = 0; i < 16; i++)
// 	{
// 		if (skyboxmo[0] && skyboxmo[0] == skyboxviewpnts[i])
// 			skyviewid = i; // save id just in case
// 		if (skyboxmo[1] && skyboxmo[1] == skyboxcenterpnts[i])
// 			skycenterid = i; // save id just in case
// 	}

// 	// remove all the current thinkers
// 	for (i = 0; i < NUM_THINKERLISTS; i++)
// 	{
// 		currentthinker = thlist[i].next;
// 		for (currentthinker = thlist[i].next; currentthinker != &thlist[i]; currentthinker = next)
// 		{
// 			next = currentthinker->next;

// 			currentthinker->references = 0;

// 			if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
			
// 				P_RemoveSavegameMobj((mobj_t *)currentthinker, preserveLevel); // item isn't saved, don't remove 
// 			else if (currentthinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
// 				P_RemovePrecipMobj((precipmobj_t*)currentthinker);
// 			else
// 			{
// 				// remove it manually, bye!
// 				currentthinker->prev->next = currentthinker->next;
// 				currentthinker->next->prev = currentthinker->prev;
// 				Z_Free(currentthinker);
// 			}
// 		}
// 	}

// 	// we don't want the removed mobjs to come back
// 	iquetail = iquehead = 0;
// 	//P_InitThinkers();

// 	// clear sector thinker pointers so they don't point to non-existant thinkers for all of eternity
// 	for (i = 0; i < numsectors; i++)
// 	{
// 		sectors[i].floordata = sectors[i].ceilingdata = sectors[i].lightingdata = sectors[i].fadecolormapdata = NULL;
// 	}

// 	// clear flags (necessary for level-preserving loads)
// 	redflag = NULL;
// 	blueflag = NULL;

// 	// read in saved thinkers
// 	for (i = 0; i < NUM_THINKERLISTS; i++)
// 	{
// 		for (;;)
// 		{
// 			thinker_t* th = NULL;
// 			tclass = READUINT8(save_p);

// 			if (tclass == tc_end)
// 				break; // leave the saved thinker reading loop
// 			numloaded++;

// 			switch (tclass)
// 			{
// 				case tc_mobj:
// 					th = LoadMobjThinker((actionf_p1)P_MobjThinker);
// 					break;

// 				case tc_ceiling:
// 					th = LoadCeilingThinker((actionf_p1)T_MoveCeiling);
// 					break;

// 				case tc_crushceiling:
// 					th = LoadCeilingThinker((actionf_p1)T_CrushCeiling);
// 					break;

// 				case tc_floor:
// 					th = LoadFloormoveThinker((actionf_p1)T_MoveFloor);
// 					break;

// 				case tc_flash:
// 					th = LoadLightflashThinker((actionf_p1)T_LightningFlash);
// 					break;

// 				case tc_strobe:
// 					th = LoadStrobeThinker((actionf_p1)T_StrobeFlash);
// 					break;

// 				case tc_glow:
// 					th = LoadGlowThinker((actionf_p1)T_Glow);
// 					break;

// 				case tc_fireflicker:
// 					th = LoadFireflickerThinker((actionf_p1)T_FireFlicker);
// 					break;

// 				case tc_elevator:
// 					th = LoadElevatorThinker((actionf_p1)T_MoveElevator, true);
// 					break;

// 				case tc_continuousfalling:
// 					th = LoadContinuousFallThinker((actionf_p1)T_ContinuousFalling);
// 					break;

// 				case tc_thwomp:
// 					th = LoadThwompThinker((actionf_p1)T_ThwompSector);
// 					break;

// 				case tc_noenemies:
// 					th = LoadNoEnemiesThinker((actionf_p1)T_NoEnemiesSector);
// 					break;

// 				case tc_eachtime:
// 					th = LoadEachTimeThinker((actionf_p1)T_EachTimeThinker);
// 					break;

// 				case tc_raisesector:
// 					th = LoadRaiseThinker((actionf_p1)T_RaiseSector);
// 					break;

// 				case tc_camerascanner:
// 					th = LoadElevatorThinker((actionf_p1)T_CameraScanner, false);
// 					break;

// 				case tc_bouncecheese:
// 					th = LoadBounceCheeseThinker((actionf_p1)T_BounceCheese);
// 					break;

// 				case tc_startcrumble:
// 					th = LoadCrumbleThinker((actionf_p1)T_StartCrumble);
// 					break;

// 				case tc_marioblock:
// 					th = LoadMarioBlockThinker((actionf_p1)T_MarioBlock);
// 					break;

// 				case tc_marioblockchecker:
// 					th = LoadMarioCheckThinker((actionf_p1)T_MarioBlockChecker);
// 					break;

// 				case tc_floatsector:
// 					th = LoadFloatThinker((actionf_p1)T_FloatSector);
// 					break;

// 				case tc_laserflash:
// 					th = LoadLaserThinker((actionf_p1)T_LaserFlash);
// 					break;

// 				case tc_lightfade:
// 					th = LoadLightlevelThinker((actionf_p1)T_LightFade);
// 					break;

// 				case tc_executor:
// 					th = LoadExecutorThinker((actionf_p1)T_ExecutorDelay);
// 					restoreNum = true;
// 					break;

// 				case tc_disappear:
// 					th = LoadDisappearThinker((actionf_p1)T_Disappear);
// 					break;

// 				case tc_fade:
// 					th = LoadFadeThinker((actionf_p1)T_Fade);
// 					break;

// 				case tc_fadecolormap:
// 					th = LoadFadeColormapThinker((actionf_p1)T_FadeColormap);
// 					break;

// 				case tc_planedisplace:
// 					th = LoadPlaneDisplaceThinker((actionf_p1)T_PlaneDisplace);
// 					break;
// 				case tc_polyrotate:
// 					th = LoadPolyrotatetThinker((actionf_p1)T_PolyObjRotate);
// 					break;

// 				case tc_polymove:
// 					th = LoadPolymoveThinker((actionf_p1)T_PolyObjMove);
// 					break;

// 				case tc_polywaypoint:
// 					th = LoadPolywaypointThinker((actionf_p1)T_PolyObjWaypoint);
// 					break;

// 				case tc_polyslidedoor:
// 					th = LoadPolyslidedoorThinker((actionf_p1)T_PolyDoorSlide);
// 					break;

// 				case tc_polyswingdoor:
// 					th = LoadPolyswingdoorThinker((actionf_p1)T_PolyDoorSwing);
// 					break;

// 				case tc_polyflag:
// 					th = LoadPolymoveThinker((actionf_p1)T_PolyObjFlag);
// 					break;

// 				case tc_polydisplace:
// 					th = LoadPolydisplaceThinker((actionf_p1)T_PolyObjDisplace);
// 					break;

// 				case tc_polyrotdisplace:
// 					th = LoadPolyrotdisplaceThinker((actionf_p1)T_PolyObjRotDisplace);
// 					break;

// 				case tc_polyfade:
// 					th = LoadPolyfadeThinker((actionf_p1)T_PolyObjFade);
// 					break;

// 				case tc_dynslopeline:
// 					th = LoadDynamicSlopeThinker((actionf_p1)T_DynamicSlopeLine);
// 					break;

// 				case tc_dynslopevert:
// 					th = LoadDynamicSlopeThinker((actionf_p1)T_DynamicSlopeVert);
// 					break;

// 				case tc_scroll:
// 					th = LoadScrollThinker((actionf_p1)T_Scroll);
// 					break;

// 				case tc_friction:
// 					th = LoadFrictionThinker((actionf_p1)T_Friction);
// 					break;

// 				case tc_pusher:
// 					th = LoadPusherThinker((actionf_p1)T_Pusher);
// 					break;

// 				default:
// 					I_Error("P_UnarchiveSpecials: Unknown tclass %d in savegame", tclass);
// 			}
// 			if (th)
// 				P_AddThinker(i, th);
// 		}

// 		CONS_Debug(DBG_NETPLAY, "%u thinkers loaded in list %d\n", numloaded, i);
// 	}

// 	if (restoreNum)
// 	{
// 		executor_t *delay = NULL;
// 		UINT32 mobjnum;
// 		for (currentthinker = thlist[THINK_MAIN].next; currentthinker != &thlist[THINK_MAIN]; currentthinker = currentthinker->next)
// 		{
// 			if (currentthinker->function.acp1 != (actionf_p1)T_ExecutorDelay)
// 				continue;
// 			delay = (void *)currentthinker;
// 			if (!(mobjnum = (UINT32)(size_t)delay->caller))
// 				continue;
// 			delay->caller = P_FindNewPosition(mobjnum);
// 		}
// 	}
// 	for (i = 0; i < sizeof(skyboxmo) / sizeof(skyboxmo[0]); i++)
// 		skyboxmo[i] = NULL;
// 	for (i = 0; i < sizeof(skyboxcenterpnts) / sizeof(skyboxcenterpnts[0]); i++)
// 	{
// 		skyboxcenterpnts[i] = NULL;
// 		skyboxviewpnts[i] = NULL;
// 	}

// 	for (currentmobj = (mobj_t*)thlist[THINK_MOBJ].next; currentmobj != (mobj_t*)&thlist[THINK_MOBJ]; currentmobj = (mobj_t*)currentmobj->thinker.next)
// 	{
// 		if (currentmobj->type == MT_SKYBOX)
// 		{
// 			if ((currentmobj->extravalue2 >> 16) == 1)
// 			{
// 				skyboxcenterpnts[currentmobj->extravalue2 & 0xFFFF] = currentmobj;
// 			}
// 			else
// 			{
// 				skyboxviewpnts[currentmobj->extravalue2 & 0xFFFF] = currentmobj;
// 			}
// 		}
// 	}

// 	skyboxmo[0] = skyboxviewpnts[(skyviewid >= 0) ? skyviewid : 0];
// 	skyboxmo[1] = skyboxcenterpnts[(skycenterid >= 0) ? skycenterid : 0];
// }

static void P_NetUnArchiveThinkers(void)
{
	thinker_t *currentthinker;
	thinker_t *next;
	UINT8 tclass;
	UINT8 restoreNum = false;
	UINT32 i;
	UINT32 numloaded = 0;

	if (READUINT32(save_p) != ARCHIVEBLOCK_THINKERS)
		I_Error("Bad $$$.sav at archive block Thinkers");

	// remove all the current thinkers
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		currentthinker = thlist[i].next;
		for (currentthinker = thlist[i].next; currentthinker != &thlist[i]; currentthinker = next)
		{
			next = currentthinker->next;

			if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
				P_RemoveSavegameMobj((mobj_t *)currentthinker, false); // item isn't saved, don't remove it
			else
				Z_Free(currentthinker);
		}
	}

	// we don't want the removed mobjs to come back
	iquetail = iquehead = 0;
	P_InitThinkers();

	// clear sector thinker pointers so they don't point to non-existant thinkers for all of eternity
	for (i = 0; i < numsectors; i++)
	{
		sectors[i].floordata = sectors[i].ceilingdata = sectors[i].lightingdata = sectors[i].fadecolormapdata = NULL;
	}

	// read in saved thinkers
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		for (;;)
		{
			thinker_t* th = NULL;
			tclass = READUINT8(save_p);

			if (tclass == tc_end)
				break; // leave the saved thinker reading loop
			numloaded++;

			switch (tclass)
			{
				case tc_mobj:
					th = LoadMobjThinker((actionf_p1)P_MobjThinker);
					break;

				case tc_ceiling:
					th = LoadCeilingThinker((actionf_p1)T_MoveCeiling);
					break;

				case tc_crushceiling:
					th = LoadCeilingThinker((actionf_p1)T_CrushCeiling);
					break;

				case tc_floor:
					th = LoadFloormoveThinker((actionf_p1)T_MoveFloor);
					break;

				case tc_flash:
					th = LoadLightflashThinker((actionf_p1)T_LightningFlash);
					break;

				case tc_strobe:
					th = LoadStrobeThinker((actionf_p1)T_StrobeFlash);
					break;

				case tc_glow:
					th = LoadGlowThinker((actionf_p1)T_Glow);
					break;

				case tc_fireflicker:
					th = LoadFireflickerThinker((actionf_p1)T_FireFlicker);
					break;

				case tc_elevator:
					th = LoadElevatorThinker((actionf_p1)T_MoveElevator, true);
					break;

				case tc_continuousfalling:
					th = LoadContinuousFallThinker((actionf_p1)T_ContinuousFalling);
					break;

				case tc_thwomp:
					th = LoadThwompThinker((actionf_p1)T_ThwompSector);
					break;

				case tc_noenemies:
					th = LoadNoEnemiesThinker((actionf_p1)T_NoEnemiesSector);
					break;

				case tc_eachtime:
					th = LoadEachTimeThinker((actionf_p1)T_EachTimeThinker);
					break;

				case tc_raisesector:
					th = LoadRaiseThinker((actionf_p1)T_RaiseSector);
					break;

				case tc_camerascanner:
					th = LoadElevatorThinker((actionf_p1)T_CameraScanner, false);
					break;

				case tc_bouncecheese:
					th = LoadBounceCheeseThinker((actionf_p1)T_BounceCheese);
					break;

				case tc_startcrumble:
					th = LoadCrumbleThinker((actionf_p1)T_StartCrumble);
					break;

				case tc_marioblock:
					th = LoadMarioBlockThinker((actionf_p1)T_MarioBlock);
					break;

				case tc_marioblockchecker:
					th = LoadMarioCheckThinker((actionf_p1)T_MarioBlockChecker);
					break;

				case tc_floatsector:
					th = LoadFloatThinker((actionf_p1)T_FloatSector);
					break;

				case tc_laserflash:
					th = LoadLaserThinker((actionf_p1)T_LaserFlash);
					break;

				case tc_lightfade:
					th = LoadLightlevelThinker((actionf_p1)T_LightFade);
					break;

				case tc_executor:
					th = LoadExecutorThinker((actionf_p1)T_ExecutorDelay);
					restoreNum = true;
					break;

				case tc_disappear:
					th = LoadDisappearThinker((actionf_p1)T_Disappear);
					break;

				case tc_fade:
					th = LoadFadeThinker((actionf_p1)T_Fade);
					break;

				case tc_fadecolormap:
					th = LoadFadeColormapThinker((actionf_p1)T_FadeColormap);
					break;

				case tc_planedisplace:
					th = LoadPlaneDisplaceThinker((actionf_p1)T_PlaneDisplace);
					break;
				case tc_polyrotate:
					th = LoadPolyrotatetThinker((actionf_p1)T_PolyObjRotate);
					break;

				case tc_polymove:
					th = LoadPolymoveThinker((actionf_p1)T_PolyObjMove);
					break;

				case tc_polywaypoint:
					th = LoadPolywaypointThinker((actionf_p1)T_PolyObjWaypoint);
					break;

				case tc_polyslidedoor:
					th = LoadPolyslidedoorThinker((actionf_p1)T_PolyDoorSlide);
					break;

				case tc_polyswingdoor:
					th = LoadPolyswingdoorThinker((actionf_p1)T_PolyDoorSwing);
					break;

				case tc_polyflag:
					th = LoadPolymoveThinker((actionf_p1)T_PolyObjFlag);
					break;

				case tc_polydisplace:
					th = LoadPolydisplaceThinker((actionf_p1)T_PolyObjDisplace);
					break;

				case tc_polyrotdisplace:
					th = LoadPolyrotdisplaceThinker((actionf_p1)T_PolyObjRotDisplace);
					break;

				case tc_polyfade:
					th = LoadPolyfadeThinker((actionf_p1)T_PolyObjFade);
					break;

				case tc_dynslopeline:
					th = LoadDynamicSlopeThinker((actionf_p1)T_DynamicSlopeLine);
					break;

				case tc_dynslopevert:
					th = LoadDynamicSlopeThinker((actionf_p1)T_DynamicSlopeVert);
					break;

				case tc_scroll:
					th = LoadScrollThinker((actionf_p1)T_Scroll);
					break;

				case tc_friction:
					th = LoadFrictionThinker((actionf_p1)T_Friction);
					break;

				case tc_pusher:
					th = LoadPusherThinker((actionf_p1)T_Pusher);
					break;

				default:
					I_Error("P_UnarchiveSpecials: Unknown tclass %d in savegame", tclass);
			}
			if (th)
				P_AddThinker(i, th);
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers loaded in list %d\n", numloaded, i);
	}

	if (restoreNum)
	{
		executor_t *delay = NULL;
		UINT32 mobjnum;
		for (currentthinker = thlist[THINK_MAIN].next; currentthinker != &thlist[THINK_MAIN]; currentthinker = currentthinker->next)
		{
			if (currentthinker->function.acp1 != (actionf_p1)T_ExecutorDelay)
				continue;
			delay = (void *)currentthinker;
			if (!(mobjnum = (UINT32)(size_t)delay->caller))
				continue;
			delay->caller = P_FindNewPosition(mobjnum);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// haleyjd 03/26/06: PolyObject saving code
//
#define PD_FLAGS  0x01
#define PD_TRANS   0x02

static inline void P_ArchivePolyObj(polyobj_t *po)
{
	UINT8 diff = 0;
	WRITEINT32(save_p, po->id);
	WRITEANGLE(save_p, po->angle);

	WRITEFIXED(save_p, po->spawnSpot.x);
	WRITEFIXED(save_p, po->spawnSpot.y);

	if (po->flags != po->spawnflags)
		diff |= PD_FLAGS;
	if (po->translucency != po->spawntrans)
		diff |= PD_TRANS;

	WRITEUINT8(save_p, diff);

	if (diff & PD_FLAGS)
		WRITEINT32(save_p, po->flags);
	if (diff & PD_TRANS)
		WRITEINT32(save_p, po->translucency);
}

static inline void P_UnArchivePolyObj(polyobj_t *po)
{
	INT32 id;
	UINT32 angle;
	fixed_t x, y;
	UINT8 diff;

	// nullify all polyobject thinker pointers;
	// the thinkers themselves will fight over who gets the field
	// when they first start to run.
	po->thinker = NULL;

	id = READINT32(save_p);

	angle = READANGLE(save_p);

	x = READFIXED(save_p);
	y = READFIXED(save_p);

	diff = READUINT8(save_p);

	if (diff & PD_FLAGS)
		po->flags = READINT32(save_p);
	if (diff & PD_TRANS)
		po->translucency = READINT32(save_p);

	// if the object is bad or isn't in the id hash, we can do nothing more
	// with it, so return now
	if (po->isBad || po != Polyobj_GetForNum(id))
		return;

	// rotate and translate polyobject
	Polyobj_MoveOnLoad(po, angle, x, y);
}

static inline void P_ArchivePolyObjects(void)
{
	INT32 i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_POBJS);

	// save number of polyobjects
	WRITEINT32(save_p, numPolyObjects);

	for (i = 0; i < numPolyObjects; ++i)
		P_ArchivePolyObj(&PolyObjects[i]);
}

static inline void P_UnArchivePolyObjects(void)
{
	INT32 i, numSavedPolys;

	if (READUINT32(save_p) != ARCHIVEBLOCK_POBJS)
		I_Error("Bad $$$.sav at archive block Pobjs");

	numSavedPolys = READINT32(save_p);

	if (numSavedPolys != numPolyObjects)
		I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");

	for (i = 0; i < numSavedPolys; ++i)
		P_UnArchivePolyObj(&PolyObjects[i]);
}

static void P_LocalArchivePolyObjects(void)
{
	P_ArchivePolyObjects();
	/*WRITEUINT32(save_p, ARCHIVEBLOCK_POBJS);
	WRITEINT32(save_p, numPolyObjects);
	WRITEMEM(save_p, PolyObjects, numPolyObjects * sizeof(PolyObjects[0]));*/
}

static void P_LocalUnArchivePolyObjects(void)
{
	P_UnArchivePolyObjects();
	/*if (READUINT32(save_p) != ARCHIVEBLOCK_POBJS)
		I_Error("Bad savestate at archive block polyobjects");
	numPolyObjects = READINT32(save_p);
	READMEM(save_p, PolyObjects, numPolyObjects * sizeof(PolyObjects[0]));
	bmap_freelist = NULL;
	for (int i = 0; i < numPolyObjects; i++)
	{
		PolyObjects[i].thinker = NULL;
		PolyObjects[i].linked = false;
		Polyobj_linkToBlockmap(&PolyObjects[i]);
	}*/
}

static inline void P_FinishMobjs(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;

	// put info field there real value
	for (currentthinker = thlist[THINK_MOBJ].next; currentthinker != &thlist[THINK_MOBJ];
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)currentthinker;
		mobj->info = &mobjinfo[mobj->type];
	}
}

static void P_RelinkPointers(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;
	UINT32 temp;

	// use info field (value = oldposition) to relink mobjs
	for (currentthinker = thlist[THINK_MOBJ].next; currentthinker != &thlist[THINK_MOBJ];
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)currentthinker;

		if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER)
			continue;

		if (mobj->tracer)
		{
			temp = (UINT32)(size_t)mobj->tracer;
			mobj->tracer = NULL;
			if (!P_SetTarget(&mobj->tracer, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "tracer not found on %d\n", mobj->type);
		}
		if (mobj->target)
		{
			temp = (UINT32)(size_t)mobj->target;
			mobj->target = NULL;
			if (!P_SetTarget(&mobj->target, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "target not found on %d\n", mobj->type);
		}
		if (mobj->hnext)
		{
			temp = (UINT32)(size_t)mobj->hnext;
			mobj->hnext = NULL;
			if (!(mobj->hnext = P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hnext not found on %d\n", mobj->type);
		}
		if (mobj->hprev)
		{
			temp = (UINT32)(size_t)mobj->hprev;
			mobj->hprev = NULL;
			if (!(mobj->hprev = P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hprev not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->capsule)
		{
			temp = (UINT32)(size_t)mobj->player->capsule;
			mobj->player->capsule = NULL;
			if (!P_SetTarget(&mobj->player->capsule, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "capsule not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->axis1)
		{
			temp = (UINT32)(size_t)mobj->player->axis1;
			mobj->player->axis1 = NULL;
			if (!P_SetTarget(&mobj->player->axis1, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "axis1 not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->axis2)
		{
			temp = (UINT32)(size_t)mobj->player->axis2;
			mobj->player->axis2 = NULL;
			if (!P_SetTarget(&mobj->player->axis2, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "axis2 not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->awayviewmobj)
		{
			temp = (UINT32)(size_t)mobj->player->awayviewmobj;
			mobj->player->awayviewmobj = NULL;
			if (!P_SetTarget(&mobj->player->awayviewmobj, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "awayviewmobj not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->followmobj)
		{
			temp = (UINT32)(size_t)mobj->player->followmobj;
			mobj->player->followmobj = NULL;
			if (!P_SetTarget(&mobj->player->followmobj, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "followmobj not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->drone)
		{
			temp = (UINT32)(size_t)mobj->player->drone;
			mobj->player->drone = NULL;
			if (!P_SetTarget(&mobj->player->drone, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "drone not found on %d\n", mobj->type);
		}
	}
}

static inline void P_NetArchiveSpecials(void)
{
	size_t i, z;

	WRITEUINT32(save_p, ARCHIVEBLOCK_SPECIALS);

	// itemrespawn queue for deathmatch
	i = iquetail;
	while (iquehead != i)
	{
		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] == itemrespawnque[i])
			{
				WRITEUINT32(save_p, z);
				break;
			}
		}
		WRITEUINT32(save_p, itemrespawntime[i]);
		i = (i + 1) & (ITEMQUESIZE-1);
	}

	// end delimiter
	WRITEUINT32(save_p, 0xffffffff);

	// Sky number
	WRITEINT32(save_p, globallevelskynum);

	// Current global weather type
	WRITEUINT8(save_p, globalweather);

	if (metalplayback) // Is metal sonic running?
	{
		WRITEUINT8(save_p, 0x01);
		G_SaveMetal(&save_p);
	}
	else
		WRITEUINT8(save_p, 0x00);
}

static void P_NetUnArchiveSpecials(void)
{
	size_t i;
	INT32 j;

	if (READUINT32(save_p) != ARCHIVEBLOCK_SPECIALS)
		I_Error("Bad $$$.sav at archive block Specials");

	// BP: added save itemrespawn queue for deathmatch
	iquetail = iquehead = 0;
	while ((i = READUINT32(save_p)) != 0xffffffff)
	{
		itemrespawnque[iquehead] = &mapthings[i];
		itemrespawntime[iquehead++] = READINT32(save_p);
	}

	j = READINT32(save_p);
	if (j != globallevelskynum)
		P_SetupLevelSky(j, true);

	globalweather = READUINT8(save_p);

	if (globalweather)
	{
		if (curWeather == globalweather)
			curWeather = PRECIP_NONE;

		P_SwitchWeather(globalweather);
	}
	else // PRECIP_NONE
	{
		if (curWeather != PRECIP_NONE)
			P_SwitchWeather(globalweather);
	}

	if (READUINT8(save_p) == 0x01) // metal sonic
		G_LoadMetal(&save_p);
}

static inline void P_LocalArchiveSpecials(void)
{
	size_t i, z;

	WRITEUINT32(save_p, ARCHIVEBLOCK_SPECIALS);

	// itemrespawn queue for deathmatch
	i = iquetail;
	while (iquehead != i)
	{
		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] == itemrespawnque[i])
			{
				WRITEUINT32(save_p, z);
				break;
			}
		}
		WRITEUINT32(save_p, itemrespawntime[i]);
		i = (i + 1) & (ITEMQUESIZE-1);
	}

	// We don't archive the weather because it's independent from the thinkerlist

	// end delimiter
	WRITEUINT32(save_p, 0xffffffff);

	// Sky number
	WRITEINT32(save_p, globallevelskynum);

	if (metalplayback) // Is metal sonic running?
	{
		WRITEUINT8(save_p, 0x01);
		G_SaveMetal(&save_p);
	}
	else
		WRITEUINT8(save_p, 0x00);
}

static void P_LocalUnArchiveSpecials(void)
{
	size_t i;
	INT32 j;

	if (READUINT32(save_p) != ARCHIVEBLOCK_SPECIALS)
		I_Error("Bad $$$.sav at archive block Specials");

	// BP: added save itemrespawn queue for deathmatch
	iquetail = iquehead = 0;
	while ((i = READUINT32(save_p)) != 0xffffffff)
	{
		itemrespawnque[iquehead] = &mapthings[i];
		itemrespawntime[iquehead++] = READINT32(save_p);
	}

	j = READINT32(save_p);
	if (j != globallevelskynum)
		P_SetupLevelSky(j, true);

	if (READUINT8(save_p) == 0x01) // metal sonic
		G_LoadMetal(&save_p);
}


// =======================================================================
//          Misc
// =======================================================================
static inline void P_ArchiveMisc(INT16 mapnum)
{
	//lastmapsaved = mapnum;
	lastmaploaded = mapnum;

	if (gamecomplete)
		mapnum |= 8192;

	WRITEINT16(save_p, mapnum);
	WRITEUINT16(save_p, emeralds+357);
	WRITESTRINGN(save_p, timeattackfolder, sizeof(timeattackfolder));
}

static inline void P_UnArchiveSPGame(INT16 mapoverride)
{
	char testname[sizeof(timeattackfolder)];

	gamemap = READINT16(save_p);

	if (mapoverride != 0)
	{
		gamemap = mapoverride;
		gamecomplete = 1;
	}
	else
		gamecomplete = 0;

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	//lastmapsaved = gamemap;
	lastmaploaded = gamemap;

	tokenlist = 0;
	token = 0;

	savedata.emeralds = READUINT16(save_p)-357;

	READSTRINGN(save_p, testname, sizeof(testname));

	if (strcmp(testname, timeattackfolder))
	{
		if (modifiedgame)
			I_Error("Save game not for this modification.");
		else
			I_Error("This save file is for a particular mod, it cannot be used with the regular game.");
	}

	memset(playeringame, 0, sizeof(*playeringame));
	playeringame[consoleplayer] = true;
}

static void P_NetArchiveMisc(boolean resending)
{
	INT32 i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_MISC);

	if (resending)
		WRITEUINT32(save_p, gametic);
	WRITEINT16(save_p, gamemap);
	WRITEINT16(save_p, gamestate);
	WRITEINT16(save_p, gametype);

	{
		UINT32 pig = 0;
		for (i = 0; i < MAXPLAYERS; i++)
			pig |= (playeringame[i] != 0)<<i;
		WRITEUINT32(save_p, pig);
	}

	WRITEUINT32(save_p, P_GetRandSeed());

	WRITEUINT32(save_p, tokenlist);

	WRITEUINT32(save_p, leveltime);
	WRITEUINT32(save_p, ssspheres);
	WRITEINT16(save_p, lastmap);
	WRITEUINT16(save_p, bossdisabled);

	WRITEUINT16(save_p, emeralds);
	{
		UINT8 globools = 0;
		if (stagefailed)
			globools |= 1;
		if (stoppedclock)
			globools |= (1<<1);
		WRITEUINT8(save_p, globools);
	}

	WRITEUINT32(save_p, token);
	WRITEINT32(save_p, sstimer);
	WRITEUINT32(save_p, bluescore);
	WRITEUINT32(save_p, redscore);

	WRITEUINT16(save_p, skincolor_redteam);
	WRITEUINT16(save_p, skincolor_blueteam);
	WRITEUINT16(save_p, skincolor_redring);
	WRITEUINT16(save_p, skincolor_bluering);

	WRITEINT32(save_p, modulothing);

	WRITEINT16(save_p, autobalance);
	WRITEINT16(save_p, teamscramble);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save_p, scrambleplayers[i]);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save_p, scrambleteams[i]);

	WRITEINT16(save_p, scrambletotal);
	WRITEINT16(save_p, scramblecount);

	WRITEUINT32(save_p, countdown);
	WRITEUINT32(save_p, countdown2);

	WRITEFIXED(save_p, gravity);

	WRITEUINT32(save_p, countdowntimer);
	WRITEUINT8(save_p, countdowntimeup);

	WRITEUINT32(save_p, hidetime);

	// Is it paused?
	if (paused)
		WRITEUINT8(save_p, 0x2f);
	else
		WRITEUINT8(save_p, 0x2e);
}



static inline boolean P_NetUnArchiveMisc(boolean reloading)
{
	INT32 i;

	if (READUINT32(save_p) != ARCHIVEBLOCK_MISC)
		I_Error("Bad $$$.sav at archive block Misc");

	if (reloading)
		gametic = READUINT32(save_p);

	gamemap = READINT16(save_p);

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	// tell the sound code to reset the music since we're skipping what
	// normally sets this flag
	mapmusflags |= MUSIC_RELOADRESET;

	G_SetGamestate(READINT16(save_p));

	gametype = READINT16(save_p);

	{
		UINT32 pig = READUINT32(save_p);
		for (i = 0; i < MAXPLAYERS; i++)
		{
			playeringame[i] = (pig & (1<<i)) != 0;
			// playerstate is set in unarchiveplayers
		}
	}

	P_SetRandSeed(READUINT32(save_p));

	tokenlist = READUINT32(save_p);

	if (!P_LoadLevel(true, reloading))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Can't load the level!\n"));
		return false;
	}

	// get the time
	leveltime = READUINT32(save_p);
	ssspheres = READUINT32(save_p);
	lastmap = READINT16(save_p);
	bossdisabled = READUINT16(save_p);

	emeralds = READUINT16(save_p);
	{
		UINT8 globools = READUINT8(save_p);
		stagefailed = !!(globools & 1);
		stoppedclock = !!(globools & (1<<1));
	}

	token = READUINT32(save_p);
	sstimer = READINT32(save_p);
	bluescore = READUINT32(save_p);
	redscore = READUINT32(save_p);

	skincolor_redteam = READUINT16(save_p);
	skincolor_blueteam = READUINT16(save_p);
	skincolor_redring = READUINT16(save_p);
	skincolor_bluering = READUINT16(save_p);

	modulothing = READINT32(save_p);

	autobalance = READINT16(save_p);
	teamscramble = READINT16(save_p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleplayers[i] = READINT16(save_p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleteams[i] = READINT16(save_p);

	scrambletotal = READINT16(save_p);
	scramblecount = READINT16(save_p);

	countdown = READUINT32(save_p);
	countdown2 = READUINT32(save_p);

	gravity = READFIXED(save_p);

	countdowntimer = (tic_t)READUINT32(save_p);
	countdowntimeup = (boolean)READUINT8(save_p);

	hidetime = READUINT32(save_p);

	// Is it paused?
	if (READUINT8(save_p) == 0x2f)
		paused = true;

	return true;
}

static inline void P_LocalArchiveCameras()
{
	WRITEMEM(save_p, &camera, sizeof(camera));
	WRITEMEM(save_p, &camera2, sizeof(camera2));
}

static inline void P_LocalUnArchiveCameras()
{
	boolean chase1 = camera.chase;
	boolean chase2 = camera2.chase;

	// this makes things easy!
	READMEM(save_p, &camera, sizeof(camera));
	READMEM(save_p, &camera2, sizeof(camera2));

	// chase is changed outside of tics, so preserve this
	camera.chase = chase1;
	camera2.chase = chase2;
}

static inline void P_ArchiveLuabanksAndConsistency(void)
{
	UINT8 i, banksinuse = NUM_LUABANKS;

	while (banksinuse && !luabanks[banksinuse-1])
		banksinuse--; // get the last used bank

	if (banksinuse)
	{
		WRITEUINT8(save_p, 0xb7); // luabanks marker
		WRITEUINT8(save_p, banksinuse);
		for (i = 0; i < banksinuse; i++)
			WRITEINT32(save_p, luabanks[i]);
	}

	WRITEUINT8(save_p, 0x1d); // consistency marker
}

static inline boolean P_UnArchiveLuabanksAndConsistency(void)
{
	switch (READUINT8(save_p))
	{
		case 0xb7: // luabanks marker
			{
				UINT8 i, banksinuse = READUINT8(save_p);
				if (banksinuse > NUM_LUABANKS)
				{
					CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Too many banks in use)\n"));
					return false;
				}
				for (i = 0; i < banksinuse; i++)
					luabanks[i] = READINT32(save_p);
				if (READUINT8(save_p) != 0x1d) // consistency marker
				{
					CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Failed consistency check)\n"));
					return false;
				}
			}
		case 0x1d: // consistency marker
			break;
		default: // anything else is nonsense
			CONS_Alert(CONS_ERROR, M_GetText("Failed consistency check (???)\n"));
			return false;
	}

	return true;
}

UINT32 I_GetTimeUs(void);

void P_SaveGame(INT16 mapnum)
{
	P_ArchiveMisc(mapnum);
	P_ArchivePlayer();
	P_ArchiveLuabanksAndConsistency();
}

void P_SaveNetGame(boolean resending)
{
	thinker_t *th;
	mobj_t *mobj;
	INT32 i = 1; // don't start from 0, it'd be confused with a blank pointer otherwise

	CV_SaveNetVars(&save_p);
	P_NetArchiveMisc(resending);

	// Assign the mobjnumber for pointer tracking
	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)th;
		if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER)
			continue;
		mobj->mobjnum = i++;
	}

	P_NetArchivePlayers();
	if (gamestate == GS_LEVEL)
	{
		P_NetArchiveWorld();
		P_ArchivePolyObjects();
		P_NetArchiveThinkers();
		P_NetArchiveSpecials();
		P_NetArchiveColormaps();
		P_NetArchiveWaypoints();
	}
	LUA_Archive();

	P_ArchiveLuabanksAndConsistency();
}

boolean P_LoadGame(INT16 mapoverride)
{
	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	G_SetGamestate(GS_NULL); // should be changed in P_UnArchiveMisc

	P_UnArchiveSPGame(mapoverride);
	P_UnArchivePlayer();

	if (!P_UnArchiveLuabanksAndConsistency())
		return false;

	// Only do this after confirming savegame is ok
	G_DeferedInitNew(false, G_BuildMapName(gamemap), savedata.skin, false, true);
	COM_BufAddText("dummyconsvar 1\n"); // G_DeferedInitNew doesn't do this

	return true;
}

boolean P_LoadNetGame(boolean reloading)
{
	mobjnum_ht_linkedList_Init(); //clean up hashtables to avoid lua stuff using them
								  //this is temporary and will be rewritten to use in vanilla code
								  //once rollback netcode will be stable
	CV_LoadNetVars(&save_p);
	if (!P_NetUnArchiveMisc(reloading))
		return false;
	P_NetUnArchivePlayers();
	if (gamestate == GS_LEVEL)
	{
		P_NetUnArchiveWorld();
		P_UnArchivePolyObjects();
		P_NetUnArchiveThinkers();
		P_NetUnArchiveSpecials();
		P_NetUnArchiveColormaps();
		P_NetUnArchiveWaypoints();
		P_RelinkPointers();
		P_FinishMobjs();
	}
	LUA_UnArchive();

	// This is stupid and hacky, but maybe it'll work!
	P_SetRandSeed(P_GetInitSeed());

	// The precipitation would normally be spawned in P_SetupLevel, which is called by
	// P_NetUnArchiveMisc above. However, that would place it up before P_NetUnArchiveThinkers,
	// so the thinkers would be deleted later. Therefore, P_SetupLevel will *not* spawn
	// precipitation when loading a netgame save. Instead, precip has to be spawned here.
	// This is done in P_NetUnArchiveSpecials now.

	return P_UnArchiveLuabanksAndConsistency();
}

// extern UINT64 saveStateBenchmark;
// extern UINT64 loadStateBenchmark;

// P_SaveGameState is a within-level-only mechanism for saving the game state. It must not be used cross level. Used for simulation backtracking.
// It uses a mixture of existing NetArchive functions and faster LocalArchive functions to do the job

// void P_SaveGameState(savestate_t* savestate)
// {
// 	mobj_t* mobj;
// 	thinker_t* th;
// 	size_t s = 0;
// 	uint32_t mobjnum = 1;
// 	uint64_t time = I_GetTimeUs();

// 	if (savestate->buffer == NULL)
// 	{
// 		savestate->buffer = Z_Malloc(10 * 1024 * 1024, PU_LEVEL, NULL); //ten megabytes?
// 	}

// 	save_p = savestate->buffer;

// 	WRITEINT16(save_p, gamemap);
// 	WRITEUINT32(save_p, globalmobjnum);

// 	CV_SaveNetVars(&save_p);

// 	// assign mobj nums for pointer relinking
// 	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
// 	{
// 		if ((th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed) || (th->function.acp1 == (actionf_p1)P_NullPrecipThinker)) 
// 			continue;

// 		mobj = (mobj_t *)th;
// 		if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER)
// 			continue;
// 		mobj->mobjnum = mobjnum++;
// 	}

// 	// including nothinkers...
// 	for (s = 0; s < numsectors; s++)
// 	{
// 		mobj_t* thing = sectors[s].thinglist;

// 		while (thing)
// 		{
// 			if (thing->flags & MF_NOTHINK)
// 			{
// 				thing->mobjnum = mobjnum++;
// 			}
// 			thing = thing->snext;
// 		}
// 	}

// 	P_NetArchiveMisc(true);
// 	P_LocalArchivePlayers();
// 	P_LocalArchiveWorld();
// 	P_LocalArchivePolyObjects();
// 	P_LocalArchiveThinkers();
// 	// P_NetArchiveSpecials();
// 	P_LocalArchiveSpecials();
// 	P_LocalArchiveCameras();

// 	// TODO: Make new P_NetArchiveRandSeed()

// 	LUA_Archive();
// 	// LUA_LocalArchive();
// 	P_ArchiveLuabanksAndConsistency();

// 	saveStateBenchmark = I_GetTimeUs() - time;
// }

// // P_LoadGameState is a within-level-only mechanism for loading the game state. It must not be used cross level. Used for simulation backtracking.
// // It uses a mixture of existing NetUnArchive functions and faster LocalUnArchive functions to do the job

// boolean P_LoadGameState(const savestate_t* savestate)
// {
// 	UINT64 time = I_GetTimeUs();
// 	INT16 savedGameMap;

// 	if (savestate->buffer == NULL)
// 	{
// 		CONS_Alert(CONS_ERROR, "Hell, we are going to load the invalid savestate!!!");
// 	}

// 	save_p = ((unsigned char*)savestate->buffer);

// 	savedGameMap = READINT16(save_p);

// 	if (savedGameMap != gamemap)
// 	{
// 		// savestates do not work cross-level
// 		return false;
// 	}

// 	globalmobjnum = READUINT32(save_p);

// 	CV_LoadNetVars(&save_p);
// 	P_NetUnArchiveMisc(true);
// 	P_LocalUnArchivePlayers();
// 	P_LocalUnArchiveWorld();
// 	P_LocalUnArchivePolyObjects();
// 	P_LocalUnArchiveThinkers();
// 	// P_NetUnArchiveSpecials();
// 	P_LocalUnArchiveSpecials();
// 	P_LocalUnArchiveCameras();

// 	LUA_UnArchive();
// 	// LUA_LocalUnArchive();
// 	// This is stupid and hacky _squared_, but it's in the net load code and it says it might work, so I guess it might work!
// 	P_SetRandSeed(P_GetInitSeed());
	
// 	P_UnArchiveLuabanksAndConsistency();
// 	loadStateBenchmark = I_GetTimeUs() - time;

// 	return true;
// }
