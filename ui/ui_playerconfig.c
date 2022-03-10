/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "ui_local.h"

void M_SetMultiplayerStatusBar ( const char *string );

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int	nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

static void HandednessCallback( void *unused ) {
	trap_Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		trap_Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
	s_player_skin_box.itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

/*
==============================
FolderIsDuplicated
==============================
*/
static qboolean FolderIsDuplicated( char *folder, int numplayermodels )
{
	int k;

	for ( k = 0; k < numplayermodels; k++ )
		if( !strcmp(s_pmi[k].directory, folder ) )
			return qtrue;
	return qfalse;
}

/*
==============================
SkinIsRegistered
==============================
*/
static qboolean SkinIsRegistered( char *skinptr, int numskins, char **skinnames )
{
	int k;
	char skinname[1024];
#ifdef SKELMOD
	COM_StripExtension( skinptr, skinname );
#else
	char scratch[1024];
	char *check;

	strcpy( scratch, skinptr );
	strcpy( skinname, skinptr );
	*strstr( skinname, "." ) = 0;
	check = strstr( scratch, "_" );

	if( check )
		*strstr( scratch, "_" ) = 0;

	strcpy( skinname, skinname + strlen( scratch ) + 1 );
#endif
	for( k = 0; k < numskins; k++ ) {
		if( !strcmp(skinnames[k], skinname ) )
			return qtrue;
	}
	return qfalse;
}

#ifdef SKELMOD
/*
==============================
ui_PModel_ValidModel
==============================
*/
qboolean ui_PModel_ValidModel( char *model_name )
{
	qboolean found = qfalse;
	char	scratch[MAX_QPATH];
	
	Q_snprintfz( scratch, sizeof(scratch), "models/players/%s/tris.skm", model_name );
	found = ( trap_FS_FOpenFile(scratch, NULL, FS_READ) != -1 );
	if( !found )
		return qfalse;

	Q_snprintfz( scratch, sizeof(scratch), "models/players/%s/animation.cfg", model_name );
	found = ( trap_FS_FOpenFile(scratch, NULL, FS_READ) != -1 );
	if( !found )
		return qfalse;
	
	return qtrue;
}
#endif

/*
==============================
PlayerConfig_ScanDirectories
==============================
*/
static void PlayerConfig_ScanDirectories( void )
{
	int		ndirs, dirlen;
	char	dirnames[1024];
#ifndef SKELMOD
	char	scratch[1024];
#endif
	char	*dirptr;
	int		i;

	s_numplayermodels = 0;

	// get a list of directories
	ndirs = trap_FS_GetFileList( "models/players", "/", dirnames, sizeof(dirnames), 0, 0 );
	if( !ndirs )
		return;

	// go through the subdirectories
	ndirs = ndirs;
	if( ndirs > MAX_PLAYERMODELS )
		ndirs = MAX_PLAYERMODELS;
	dirptr = dirnames;

	for( i = 0; i < ndirs; i++, dirptr += dirlen + 1 ) {
		int		k;
		char	pcxnames[1024];
		char	**skinnames, *skinptr;
		char	**skinfilenames;
		int		npcxfiles;
		int		nskins = 0, skinlen;

		dirlen = strlen( dirptr );

		if( dirlen && dirptr[dirlen-1] == '/' )
			dirptr[dirlen-1] = '\0';

		if( !strcmp( dirptr, "." ) || !strcmp( dirptr, ".." ) )
			continue;
		if( FolderIsDuplicated( dirptr, s_numplayermodels ) )
			continue;	//	check for duplicated folders
		if( !ui_PModel_ValidModel( dirptr ) )
			continue;

		// verify the existence of at least one skin file
		npcxfiles = trap_FS_GetFileList( va( "models/players/%s", dirptr ), ".skin", pcxnames, sizeof( pcxnames ), 0, 0 );
		if( !npcxfiles )
			continue;
#ifndef SKELMOD
		// verify the existence of the animation config
		Q_snprintfz( scratch, sizeof( scratch ), "models/players/%s/animation.cfg", dirptr );
		if( trap_FS_FOpenFile( scratch, NULL, FS_READ ) == -1 )
			continue;
#endif
		skinptr = pcxnames;
		skinlen = 0;

		// there are valid skins. So:
		skinnames = UI_Malloc( sizeof( char * ) * ( npcxfiles + 1 ) );
		skinptr = pcxnames;
		
		// copy the valid skins
		for( nskins = 0, k = 0; k < npcxfiles; k++, skinptr += skinlen + 1 ) {
			skinlen = strlen (skinptr);

			if( !SkinIsRegistered( skinptr, nskins, skinnames ) ) {
#ifdef SKELMOD
				skinnames[nskins] = UI_CopyString( skinptr );
				COM_StripExtension( skinnames[nskins], skinnames[nskins] );	
#else
				char *check;

				skinnames[nskins] = UI_CopyString( skinptr );
				*strstr( skinnames[nskins], "." ) = 0;
				strcpy( scratch, skinnames[nskins] );
				check = strstr( scratch, "_" );
				if( check )
					*strstr( scratch, "_" ) = 0;

				// add & count
				strcpy( skinnames[nskins], skinnames[nskins] + strlen( scratch ) + 1 );
#endif
				nskins++;
			}
		}

		if( !nskins )
			continue;

		// free unused skins slots
		skinfilenames = UI_Malloc( sizeof( char * ) * ( nskins + 1 ) );
		for( k = 0; k < nskins; k++ )
			skinfilenames[k] = UI_CopyString( skinnames[k] );
		for( k = 0; k < npcxfiles; k++ ) skinnames[k] = 0;
		UI_Free( skinnames );

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinfilenames;

		// make short name for the model
		Q_strncpyz( s_pmi[s_numplayermodels].displayname, dirptr, sizeof(s_pmi[s_numplayermodels].displayname) );
		strcpy( s_pmi[s_numplayermodels].directory, dirptr );

		s_numplayermodels++;
	}
}


qboolean PlayerConfig_MenuInit( void )
{
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;
	int currentdirectoryindex = 0;
	int currentskinindex = 0;
	int	hand = trap_Cvar_VariableValue( "hand" );
	char *name = trap_Cvar_VariableString ( "name" );
	char *skin = trap_Cvar_VariableString ( "skin" );
	int y = 0;
	int y_offset = UI_StringHeightOffset ( 0 );
	static char *handedness[] = { "right", "left", "center", 0 };
#ifndef SKELMOD
	pmodelitem_t	*pmodelitem;
#endif
	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return qfalse;

	if ( hand < 0 || hand > 2 )
		trap_Cvar_SetValue( "hand", 0 );

	strcpy( currentdirectory, skin );

	if ( strchr( currentdirectory, '/' ) ) {
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	} else if ( strchr( currentdirectory, '\\' ) ) {
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	} else {
		strcpy( currentdirectory, DEFAULT_PLAYERMODEL );
		strcpy( currentskin, DEFAULT_PLAYERSKIN );
	}

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for( i = 0; i < s_numplayermodels; i++ ) {
		s_pmnames[i] = s_pmi[i].displayname;

		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 ) {
			int j;

			currentdirectoryindex = i;

			for( j = 0; j < s_pmi[i].nskins; j++ ) {
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 ) {
					currentskinindex = j;
					break;
				}
			}
		}
	}

	s_player_config_menu.x = uis.vidWidth / 2 - 95; 
	s_player_config_menu.y = uis.vidHeight / 2 - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= y;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy( s_player_name_field.buffer, name );
	s_player_name_field.cursor = strlen( name );
	y += y_offset*3;

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x    = -8;
	s_player_model_title.generic.y	 = y+=y_offset;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= y+=y_offset;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -72;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x    = -16;
	s_player_skin_title.generic.y	 = y+=y_offset;

	s_player_skin_box.generic.type			= MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x				= -56;
	s_player_skin_box.generic.y				= y+=y_offset;
	s_player_skin_box.generic.name			= 0;
	s_player_skin_box.generic.callback		= 0;
	s_player_skin_box.generic.cursor_offset = -72;
	s_player_skin_box.curvalue				= currentskinindex;
	s_player_skin_box.itemnames				= s_pmi[currentdirectoryindex].skindisplaynames;
	y+=y_offset;

	s_player_hand_title.generic.type		= MTYPE_SEPARATOR;
	s_player_hand_title.generic.name		= "handedness";
	s_player_hand_title.generic.x			= 32;
	s_player_hand_title.generic.y			= y+=y_offset;

	s_player_handedness_box.generic.type	= MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x		= -56;
	s_player_handedness_box.generic.y		= y+=y_offset;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -72;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue		= trap_Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames		= handedness;
	y+=y_offset;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (trap_Cvar_VariableValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x    = 56;
	s_player_rate_title.generic.y	 = y+=y_offset;

	s_player_rate_box.generic.type		= MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x			= -56;
	s_player_rate_box.generic.y			= y+=y_offset;
	s_player_rate_box.generic.name		= 0;
	s_player_rate_box.generic.cursor_offset = -72;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue			= i;
	s_player_rate_box.itemnames			= rate_names;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	if( s_player_skin_box.itemnames ) {
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );

	Menu_Init ( &s_player_config_menu );
#ifndef SKELMOD
	ui_RegisterPModelItem( "playerconfig_menu_model", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue], NULL, 0);
	
	// set up the window
	pmodelitem = ui_PModelItemFindByName( "playerconfig_menu_model" );
	pmodelitem->window.width = 300;
	pmodelitem->window.height = 400;
	pmodelitem->window.x = 80;
	pmodelitem->window.y = 80;
	pmodelitem->window.fov = 30;
	pmodelitem->window.shader = NULL;
#endif
	return qtrue;
}

#ifdef SKELMOD
/*
=================
PlayerConfig_MenuDraw
=================
*/
void PlayerConfig_MenuDraw( void )
{
	refdef_t refdef;
	char scratch[MAX_QPATH];

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = uis.vidWidth / 2;
	refdef.y = 0;
	refdef.width = uis.vidWidth / 2;
	refdef.height = uis.vidHeight;

	refdef.fov_x = 30;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.areabits = 0;
	refdef.time = uis.time * 0.001;
	refdef.rdflags = RDF_NOWORLDMODEL;

	if (( s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] ) && (s_pmi[s_player_model_box.curvalue].directory))
	{
		static vec3_t angles;
		entity_t entity;
		vec3_t mins, maxs;
		cgs_skeleton_t	*skel;

		Menu_Draw( &s_player_config_menu );//draw menu first

		memset( &entity, 0, sizeof( entity ) );

		Q_snprintfz( scratch, sizeof( scratch ), "models/players/%s/%s.skin", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		entity.customShader = NULL;
		entity.customSkin = trap_R_RegisterSkinFile( scratch );
		if( !entity.customSkin )
			return;

		Q_snprintfz( scratch, sizeof( scratch ), "models/players/%s/tris.skm", s_pmi[s_player_model_box.curvalue].directory );
		entity.model = trap_R_RegisterModel( scratch );
		if( trap_R_SkeletalGetNumBones( entity.model, NULL ) ) {
			skel = UI_SkeletonForModel( entity.model );
			if( !skel ) return;
		}
		
		trap_R_ModelBounds ( entity.model, mins, maxs );
		//jalfixme: modelbounds seem to not be working for skeletal models.
		entity.origin[0] = 180;
		entity.origin[1] = 0;
		entity.origin[2] = -48;
		entity.flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_FORCENOLOD;
		entity.scale = 1.0f;
		VectorCopy( entity.origin, entity.origin2 );
		VectorCopy( entity.origin, entity.lightingOrigin );
		angles[1] += 1.0f;

		if ( angles[1] > 360 )
			angles[1] -= 360;

		AnglesToAxis ( angles, entity.axis );

		entity.frame = 0;
		entity.oldframe = entity.frame;

		UI_SetBoneposesForTemporaryEntity( &entity );

		trap_R_ClearScene ();
		
		trap_R_AddEntityToScene( &entity );
		trap_R_RenderScene( &refdef );

		Q_snprintfz( scratch, sizeof( scratch ), "players/%s/%s_i", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		trap_R_DrawStretchPic( s_player_config_menu.x - 40, s_player_config_menu.y + 32, 32, 32, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic(scratch) );
	}

	UI_ResetTemporaryBoneposesCache();
}

#else // SKELMOD

/*
=================
PlayerConfig_MenuDraw
=================
*/
void PlayerConfig_MenuDraw( void )
{
	char	scratch[MAX_QPATH];
	char	model_name[MAX_QPATH];
	char	skin_name[MAX_QPATH];
	pmodelitem_t			*pmodelitem;

	if( ( s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] ) &&
		( s_pmi[s_player_model_box.curvalue].directory ) ) {

		Menu_Draw( &s_player_config_menu );		// draw the model over the menu

		strcpy( model_name, s_pmi[s_player_model_box.curvalue].directory );
		strcpy( skin_name, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		// update registration for model changes
		pmodelitem = ui_PModelItem_UpdateRegistration( "playerconfig_menu_model", model_name, skin_name);
		ui_DrawPModel( "playerconfig_menu_model", 180, 0, 0, 0 );

		// icon
		Q_snprintfz( scratch, sizeof( scratch ), "models/players/%s/icon_%s", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
		trap_R_DrawStretchPic( s_player_config_menu.x - 40, s_player_config_menu.y + 32, 32, 32, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic(scratch) );
	}	
}

#endif // SKELMOD

const char *PlayerConfig_MenuKey( int key )
{
	int i;
	menucommon_s *item;

	item = Menu_ItemAtCursor ( &s_player_config_menu );

	if ( key == K_ESCAPE || ( (key == K_MOUSE2) && (item->type != MTYPE_SPINCONTROL) &&
		(item->type != MTYPE_SLIDER)) )
	{
		char scratch[1024];

		trap_Cvar_Set( "name", s_player_name_field.buffer );

		Q_snprintfz( scratch, sizeof( scratch ), "%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		trap_Cvar_Set( "skin", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			int j;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				UI_Free( s_pmi[i].skindisplaynames[j] );
				s_pmi[i].skindisplaynames[j] = 0;
			}
			UI_Free( s_pmi[i].skindisplaynames );
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
	}

	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		M_SetMultiplayerStatusBar ( "No valid player models found" );
		return;
	}

	M_SetMultiplayerStatusBar ( NULL );
	M_PushMenu( &s_player_config_menu, PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}

