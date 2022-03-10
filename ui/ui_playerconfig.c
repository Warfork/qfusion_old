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
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

static void HandednessCallback( void *unused )
{
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

static qboolean IconOfSkinExists( char *skin, char *pcxfiles, int npcxfiles )
{
	int i;
	char scratch[1024];
	char *ptr, len = 0;

	strcpy( scratch, skin );
	*strrchr( scratch, '.' ) = 0;
	strcat( scratch, "_i.pcx" );

	ptr = pcxfiles;

	for ( i = 0; i < npcxfiles; i++, ptr += len+1 )
	{
		len = strlen (ptr);

		if ( strcmp( ptr, scratch ) == 0 )
			return true;
	}

	return false;
}

static void PlayerConfig_ScanDirectories( void )
{
	int ndirs, dirlen;
	char dirnames[1024];
	char *path = NULL, *dirptr;
	int i;

	s_numplayermodels = 0;

	/*
	** get a list of directories
	*/
	do 
	{
		if ( ndirs = trap_FS_ListFiles( "players", "/", dirnames, sizeof(dirnames) ) )
			break;
	} while ( path );

	if ( !ndirs ) {
		return;
	}

	/*
	** go through the subdirectories
	*/
	ndirs = ndirs;
	if ( ndirs > MAX_PLAYERMODELS )
		ndirs = MAX_PLAYERMODELS;
	dirptr = dirnames;

	for ( i = 0; i < ndirs; i++, dirptr += dirlen+1 )
	{
		int k, s;
		char pcxnames[1024];
		char **skinnames, *skinptr;
		int npcxfiles;
		int nskins = 0, skinlen;

		dirlen = strlen(dirptr);

		if (dirlen && dirptr[dirlen-1]=='/') dirptr[dirlen-1]='\0';

		if (!strcmp(dirptr,".") || !strcmp(dirptr,".."))
			continue;

		// verify the existence of at least one pcx skin
		npcxfiles = trap_FS_ListFiles( va("players/%s",dirptr), ".pcx", pcxnames, sizeof(pcxnames) );

		if ( !npcxfiles )
			continue;

		skinptr = pcxnames;
		skinlen = 0;

		// count valid skins, which consist of a skin with a matching "_i" icon
		for ( k = 0; k < npcxfiles; k++, skinptr += skinlen+1 )
		{
			skinlen = strlen (skinptr);
			if ( !strstr( skinptr, "_i.pcx" ) )
			{
				if ( IconOfSkinExists( skinptr, pcxnames, npcxfiles ) )
				{
					nskins++;
				}
			}
		}
		if ( !nskins )
			continue;

		skinnames = UI_malloc( sizeof( char * ) * ( nskins + 1 ) );
		skinptr = pcxnames;

		// copy the valid skins
		for ( s = 0, k = 0; k < npcxfiles; k++, skinptr += skinlen+1 )
		{
			skinlen = strlen (skinptr);

			if ( !strstr( skinptr, "_i.pcx" ) )
			{
				if ( IconOfSkinExists( skinptr, pcxnames, npcxfiles ) )
				{
					skinnames[s] = strdup( skinptr );
					*strstr(skinnames[s], ".") = 0;
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		Q_strncpyz( s_pmi[s_numplayermodels].displayname, dirptr, sizeof(s_pmi[s_numplayermodels].displayname) );
		strcpy( s_pmi[s_numplayermodels].directory, dirptr );

		s_numplayermodels++;
	}
}

static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
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
	int w, h;
	int y = 0;
	int y_offset = PROP_SMALL_HEIGHT - 2;

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	if ( hand < 0 || hand > 2 )
		trap_Cvar_SetValue( "hand", 0 );

	strcpy( currentdirectory, skin );

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
		strcpy( currentdirectory, "male" );
		strcpy( currentskin, "grunt" );
	}

	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 )
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	w = uis.vidWidth;
	h = uis.vidHeight;

	s_player_config_menu.x = w / 2 - 95; 
	s_player_config_menu.y = h / 2 - 97;
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
	if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );

	Menu_Init ( &s_player_config_menu );

	return true;
}

void PlayerConfig_MenuDraw( void )
{
	refdef_t refdef;
	char scratch[MAX_QPATH];
	int w, h;

	memset( &refdef, 0, sizeof( refdef ) );

	trap_Vid_GetCurrentInfo ( &w, &h );

	refdef.x = w / 2 + 60;
	refdef.y = h / 2 - 72;
	refdef.width = 200;
	refdef.height = 200;
	refdef.fov_x = 30;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = trap_CL_GetTime_f()*0.001;

	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
	{
		static vec3_t angles;
		int maxframe = 29;
		entity_t entity;

		memset( &entity, 0, sizeof( entity ) );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
		entity.model = trap_RegisterModel( scratch );
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		entity.customShader = trap_RegisterSkin( scratch );
		entity.flags = RF_FULLBRIGHT;
		entity.origin[0] = 100;
		entity.scale = 1.0f;
		VectorCopy( entity.origin, entity.oldorigin );
		angles[1] += 1.0f;

		if ( angles[1] > 360 )
			angles[1] -= 360;

		AnglesToAxis ( angles, entity.axis );

		entity.oldframe = entity.frame;
		entity.frame = 0;

		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( &s_player_config_menu );

		trap_RenderFrame( &refdef );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		trap_DrawStretchPic( s_player_config_menu.x - 40, refdef.y, 32, 32, 0, 0, 1, 1, colorWhite, trap_RegisterPic(scratch) );
	}
}

const char *PlayerConfig_MenuKey (int key)
{
	int i;
	menucommon_s *item;

	item = Menu_ItemAtCursor ( &s_player_config_menu );

	if ( key == K_ESCAPE || ( (key == K_MOUSE2) && (item->type != MTYPE_SPINCONTROL) &&
		(item->type != MTYPE_SLIDER)) )
	{
		char scratch[1024];

		trap_Cvar_Set( "name", s_player_name_field.buffer );

		Com_sprintf( scratch, sizeof( scratch ), "%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		trap_Cvar_Set( "skin", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			int j;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				UI_free( s_pmi[i].skindisplaynames[j] );
				s_pmi[i].skindisplaynames[j] = 0;
			}
			UI_free( s_pmi[i].skindisplaynames );
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

