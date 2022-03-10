/*
Copyright (C) 2002-2003 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "cg_local.h"

cgs_media_handle_t *sfx_headnode;

/*
=================
CG_RegisterMediaSfx
=================
*/
static cgs_media_handle_t *CG_RegisterMediaSfx ( char *name )
{
	cgs_media_handle_t *mediasfx;

	for ( mediasfx = sfx_headnode; mediasfx ; mediasfx = mediasfx->next )
	{
		if ( !Q_stricmp (mediasfx->name, name) ) {
			return mediasfx;
		}
	}

	mediasfx = CG_Malloc ( sizeof(cgs_media_handle_t) );
	mediasfx->name = CG_CopyString ( name );
	mediasfx->next = sfx_headnode;
	sfx_headnode = mediasfx;

	return mediasfx;
}

/*
=================
CG_MediaSfx
=================
*/
struct sfx_s *CG_MediaSfx ( cgs_media_handle_t *mediasfx )
{
	if ( !mediasfx->data ) {
		mediasfx->data = ( void * )trap_S_RegisterSound ( mediasfx->name );
	}
	return ( struct sfx_s * )mediasfx->data;
}

/*
=================
CG_RegisterMediaSounds
=================
*/
void CG_RegisterMediaSounds (void)
{
	int i;
	char *name;

	for ( i = 0; i < 3; i++ ) {
		cgs.media.sfxRic[i] = CG_RegisterMediaSfx ( va("sound/weapons/machinegun/ric%i.wav", i+1) );
	}

	cgs.media.sfxLashit = CG_RegisterMediaSfx ( "sound/weapons/lashit.wav" );

	cgs.media.sfxSpark5 = CG_RegisterMediaSfx ( "sound/world/spark5.wav" );
	cgs.media.sfxSpark6 = CG_RegisterMediaSfx ( "sound/world/spark6.wav" );
	cgs.media.sfxSpark7 = CG_RegisterMediaSfx ( "sound/world/spark7.wav" );

	cgs.media.sfxRailg = CG_RegisterMediaSfx ( "sound/weapons/railgun/railgf1a.wav" );

	cgs.media.sfxRockexp = CG_RegisterMediaSfx ( "sound/weapons/rocket/rocklx1a.wav" );
	cgs.media.sfxGrenexp = CG_RegisterMediaSfx ( "sound/weapons/grenlx1a.wav" );
	cgs.media.sfxWatrexp = CG_RegisterMediaSfx ( "sound/weapons/xpld_wat.wav" );

	cgs.media.sfxItemRespawn = CG_RegisterMediaSfx ( "sound/items/respawn1.wav" );
	cgs.media.sfxTeleportIn = CG_RegisterMediaSfx ( "sound/world/telein.wav" );
	cgs.media.sfxTeleportOut = CG_RegisterMediaSfx ( "sound/world/teleout.wav" );
	cgs.media.sfxJumpPad = CG_RegisterMediaSfx ( "sound/world/jumppad.wav" );
	cgs.media.sfxLand = CG_RegisterMediaSfx ( "sound/player/land1.wav" );
	cgs.media.sfxGibSound = CG_RegisterMediaSfx ( "sound/player/gibsplt1.wav" );

	for ( i = 0; i < 4; i++ ) {
		name = va ( "sound/player/footsteps/step%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_NORMAL][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/boot%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_BOOT][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/flesh%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_FLESH][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/mech%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_MECH][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/energy%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_ENERGY][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/splash%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_SPLASH][i] = CG_RegisterMediaSfx ( name );

		name = va ( "sound/player/footsteps/clank%i.wav", i+1 );
		cgs.media.sfxFootsteps[FOOTSTEP_METAL][i] = CG_RegisterMediaSfx ( name );
	}

	for ( i = 0; i < 4; i++ ) {
		name = va ( "sound/weapons/machinegun/machgf%ib.wav", i+1 );
		cgs.media.sfxMachinegunSplashes[i] = CG_RegisterMediaSfx ( name );
	}

	cgs.media.sfxBlasterSplash = CG_RegisterMediaSfx ( "sound/weapons/blastf1a.wav" );
	cgs.media.sfxHyperblasterSplash = CG_RegisterMediaSfx ( "sound/weapons/hyprbf1a.wav" );

	cgs.media.sfxShotgunSplashes[0] = CG_RegisterMediaSfx ( "sound/weapons/shotgf1b.wav" );
	cgs.media.sfxShotgunSplashes[1] = CG_RegisterMediaSfx ( "sound/weapons/shotgr1b.wav" );
	cgs.media.sfxSuperShotgunSplash = CG_RegisterMediaSfx ( "sound/weapons/sshotf1b.wav" );

	cgs.media.sfxRocketLauncherSplash = CG_RegisterMediaSfx ( "sound/weapons/rocket/rocklf1a.wav" );
	cgs.media.sfxGrenadeLauncherSplash = CG_RegisterMediaSfx ( "sound/weapons/grenade/grenlf1a.wav" );

	cgs.media.sfxBFGSplash = CG_RegisterMediaSfx ( "sound/weapons/bfg__f1y.wav" );

	cgs.media.sfxLightning = CG_RegisterMediaSfx ( "sound/weapons/tesla.wav" );
	cgs.media.sfxDisrexp = CG_RegisterMediaSfx ( "sound/weapons/disrupthit.wav" );

	cgs.media.sfxGrenBounce1 = CG_RegisterMediaSfx ( "sound/weapons/grenade/hgrenb1a.wav" );
	cgs.media.sfxGrenBounce2 = CG_RegisterMediaSfx ( "sound/weapons/grenade/hgrenb2a.wav" );
}

//======================================================================

cgs_media_handle_t *model_headnode;

/*
=================
CG_RegisterMediaModel
=================
*/
static cgs_media_handle_t *CG_RegisterMediaModel ( char *name )
{
	cgs_media_handle_t *mediamodel;

	for ( mediamodel = model_headnode; mediamodel ; mediamodel = mediamodel->next )
	{
		if ( !Q_stricmp (mediamodel->name, name) ) {
			return mediamodel;
		}
	}

	mediamodel = CG_Malloc ( sizeof(cgs_media_handle_t) );
	mediamodel->name = CG_CopyString ( name );
	mediamodel->next = model_headnode;
	model_headnode = mediamodel;

	return mediamodel;
}

/*
=================
CG_MediaModel
=================
*/
struct model_s *CG_MediaModel ( cgs_media_handle_t *mediamodel )
{
	if ( !mediamodel->data ) {
		mediamodel->data = ( void * )trap_R_RegisterModel ( mediamodel->name );
	}
	return ( struct model_s * )mediamodel->data;
}

/*
=================
CG_RegisterMediaModels
=================
*/
void CG_RegisterMediaModels (void)
{
	cgs.media.modBulletExplode = CG_RegisterMediaModel ( "models/weaphits/bullet.md3" );
	cgs.media.modFlash = CG_RegisterMediaModel ( "models/objects/flash/tris.md2" );
	cgs.media.modParasiteSegment = CG_RegisterMediaModel ( "models/monsters/parasite/segment/tris.md2" );
	cgs.media.modGrappleCable = CG_RegisterMediaModel ( "models/ctf/segment/tris.md2" );
	cgs.media.modParasiteTip = CG_RegisterMediaModel ( "models/monsters/parasite/tip/tris.md2" );
	cgs.media.modBfgExplo = CG_RegisterMediaModel ( "sprites/s_bfg2.sp2" );
	cgs.media.modBfgBigExplo = CG_RegisterMediaModel ( "sprites/s_bfg3.sp2" );
	cgs.media.modPowerScreen = CG_RegisterMediaModel ( "models/items/armor/effect/tris.md2" );
	cgs.media.modLightning = CG_RegisterMediaModel ( "models/proj/lightning/tris.md2" );
	cgs.media.modMeatyGib = CG_RegisterMediaModel ( "models/objects/gibs/sm_meat/tris.md2" );
	cgs.media.modTeleportEffect = CG_RegisterMediaModel ( "models/misc/telep.md3" );
}

//======================================================================

cgs_media_handle_t *shader_headnode;

/*
=================
CG_RegisterMediaShader
=================
*/
static cgs_media_handle_t *CG_RegisterMediaShader ( char *name )
{
	cgs_media_handle_t *mediashader;

	for ( mediashader = shader_headnode; mediashader ; mediashader = mediashader->next )
	{
		if ( !Q_stricmp (mediashader->name, name) ) {
			return mediashader;
		}
	}

	mediashader = CG_Malloc ( sizeof(cgs_media_handle_t) );
	mediashader->name = CG_CopyString ( name );
	mediashader->next = shader_headnode;
	shader_headnode = mediashader;

	return mediashader;
}

/*
=================
CG_MediaShader
=================
*/
struct shader_s *CG_MediaShader ( cgs_media_handle_t *mediashader )
{
	if ( !mediashader->data ) {
		mediashader->data = ( void * )trap_R_RegisterPic ( mediashader->name );
	}
	return ( struct shader_s * )mediashader->data;
}

char *sb_nums[11] = 
{
	"gfx/2d/numbers/zero_32b", "gfx/2d/numbers/one_32b", 
	"gfx/2d/numbers/two_32b", "gfx/2d/numbers/three_32b", 
	"gfx/2d/numbers/four_32b", "gfx/2d/numbers/five_32b",
	"gfx/2d/numbers/six_32b", "gfx/2d/numbers/seven_32b",
	"gfx/2d/numbers/eight_32b", "gfx/2d/numbers/nine_32b", 
	"gfx/2d/numbers/minus_32b"
};

/*
=================
CG_RegisterMediaShaders
=================
*/
void CG_RegisterMediaShaders (void)
{
	int i;

	cgs.media.shaderParticle = CG_RegisterMediaShader ( "particle" );

	cgs.media.shaderNet = CG_RegisterMediaShader ( "gfx/2d/net" );
	cgs.media.shaderBackTile = CG_RegisterMediaShader ( "gfx/2d/backtile" );
	cgs.media.shaderSelect = CG_RegisterMediaShader ( "gfx/2d/select" );

	cgs.media.shaderGrenadeExplosion = CG_RegisterMediaShader ( "grenadeExplosion" );
	cgs.media.shaderRocketExplosion = CG_RegisterMediaShader ( "rocketExplosion" );
	cgs.media.shaderBulletExplosion = CG_RegisterMediaShader ( "bulletExplosion" );
	cgs.media.shaderWaterBubble = CG_RegisterMediaShader ( "waterBubble" );
	cgs.media.shaderTeleportEffect = CG_RegisterMediaShader ( "teleportEffect" );
	cgs.media.shaderSmokePuff = CG_RegisterMediaShader ( "smokePuff" );
	cgs.media.shaderBulletMark = CG_RegisterMediaShader ( "gfx/damage/bullet_mrk" );
	cgs.media.shaderExplosionMark = CG_RegisterMediaShader ( "gfx/damage/burn_med_mrk" );
	cgs.media.shaderEnergyMark = CG_RegisterMediaShader ( "gfx/damage/plasma_mrk" );
	cgs.media.shaderLaser = CG_RegisterMediaShader ( "laser" );

	cgs.media.shaderPowerupQuad = CG_RegisterMediaShader ( "powerups/quad" );
	cgs.media.shaderQuadWeapon = CG_RegisterMediaShader ( "powerups/quadWeapon" );
	cgs.media.shaderPowerupPenta = CG_RegisterMediaShader ( "powerups/blueflag" );
	cgs.media.shaderShellEffect = CG_RegisterMediaShader ( "shellEffect" );

	for ( i = 0; i < 11; i++ ) {
		cgs.media.sbNums[i] = CG_RegisterMediaShader ( sb_nums[i] );
	}

	for ( i = 0; i < NUM_CROSSHAIRS; i++ ) {
		cgs.media.shaderCrosshair[i] = CG_RegisterMediaShader ( va ("gfx/2d/crosshair%c", 'a'+i) );
	}
}

/*
=================
CG_RegisterLevelShot
=================
*/
void CG_RegisterLevelShot (void)
{
	char *name, levelshot[MAX_QPATH];

	name = cgs.configStrings[CS_MAPNAME];

	Q_snprintfz ( levelshot, sizeof(levelshot), "levelshots/%s.jpg", name );

	if ( trap_FS_FOpenFile( levelshot, NULL, FS_READ ) == -1 ) 
		Q_snprintfz ( levelshot, sizeof(levelshot), "levelshots/%s.tga", name );

	if ( trap_FS_FOpenFile( levelshot, NULL, FS_READ ) == -1 ) 
		Q_snprintfz ( levelshot, sizeof(levelshot), "menu/art/unknownmap" );

	cgs.shaderLevelshot = trap_R_RegisterPic ( levelshot );
	cgs.shaderLevelshotDetail = trap_R_RegisterPic ( "levelShotDetail" );
}

