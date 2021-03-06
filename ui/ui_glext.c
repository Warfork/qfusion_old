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

/*
=======================================================================

OPENGL EXTENSIONS MENU

=======================================================================
*/

static menuframework_s	s_glext_menu;

static menulist_s		s_extensions_list;
static menulist_s		s_cva_list;
static menulist_s		s_multitexture_list;
static menulist_s		s_texenvcombine_list;
static menulist_s		s_glsl_list;
static menulist_s		s_compressedtex_list;
static menulist_s		s_cubemap_list;
static menuslider_s		s_texfilteranisotropic_slider;

static menuaction_s		s_apply_action;

static void ApplyChanges( void *unused )
{
	trap_Cvar_SetValue( "gl_extensions", s_extensions_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_compiled_vertex_array", s_cva_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_multitexture", s_multitexture_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_texture_env_combine", s_texenvcombine_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_GLSL", s_glsl_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_compressed_textures", s_compressedtex_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_texture_cube_map", s_cubemap_list.curvalue );
	trap_Cvar_SetValue( "gl_ext_texture_filter_anisotropic", s_texfilteranisotropic_slider.curvalue );

	trap_Cmd_ExecuteText (EXEC_APPEND, "vid_restart\n");
	trap_Cmd_Execute();
}

/*
** GLExt_MenuInit
*/
static void GLExt_MenuInit( void )
{
	static char *on_off_names[] = {	"off", "on", 0 };
	static char *sbar = "Note: values below 2 do not make any effect";

	int y = 0;
	int y_offset = UI_StringHeightOffset ( 0 );

	s_glext_menu.x = uis.vidWidth / 2;
	s_glext_menu.nitems = 0;

	s_extensions_list.generic.type	= MTYPE_SPINCONTROL;
	s_extensions_list.generic.name	= "OpenGL Extensions";
	s_extensions_list.generic.x		= 0;
	s_extensions_list.generic.y		= y;
	s_extensions_list.itemnames		= on_off_names;
	s_extensions_list.curvalue		= trap_Cvar_VariableValue( "gl_extensions" );
	s_extensions_list.generic.statusbar = "set this to 'off' to disable all extensions";
	clamp ( s_extensions_list.curvalue, 0, 1 );

	s_cva_list.generic.type		= MTYPE_SPINCONTROL;
	s_cva_list.generic.name		= "Compiled vertex array";
	s_cva_list.generic.x		= 0;
	s_cva_list.generic.y		= y+=y_offset;
	s_cva_list.itemnames		= on_off_names;
	s_cva_list.curvalue			= trap_Cvar_VariableValue( "gl_ext_compiled_vertex_array" );
	clamp ( s_cva_list.curvalue, 0, 1 );

	s_multitexture_list.generic.type	= MTYPE_SPINCONTROL;
	s_multitexture_list.generic.name	= "Multitexturing";
	s_multitexture_list.generic.x		= 0;
	s_multitexture_list.generic.y		= y+=y_offset;
	s_multitexture_list.itemnames		= on_off_names;
	s_multitexture_list.curvalue		= trap_Cvar_VariableValue( "gl_ext_multitexture" );
	clamp ( s_multitexture_list.curvalue, 0, 1 );

	s_texenvcombine_list.generic.type	= MTYPE_SPINCONTROL;
	s_texenvcombine_list.generic.name	= "Env Combine";
	s_texenvcombine_list.generic.x		= 0;
	s_texenvcombine_list.generic.y		= y+=y_offset;
	s_texenvcombine_list.itemnames		= on_off_names;
	s_texenvcombine_list.curvalue		= trap_Cvar_VariableValue( "gl_ext_texture_env_combine" );
	clamp ( s_texenvcombine_list.curvalue, 0, 1 );

	s_glsl_list.generic.type			= MTYPE_SPINCONTROL;
	s_glsl_list.generic.name			= "GLSL";
	s_glsl_list.generic.x				= 0;
	s_glsl_list.generic.y				= y+=y_offset;
	s_glsl_list.itemnames				= on_off_names;
	s_glsl_list.curvalue				= trap_Cvar_VariableValue( "gl_ext_GLSL" );
	clamp ( s_glsl_list.curvalue, 0, 1 );

	s_compressedtex_list.generic.type	= MTYPE_SPINCONTROL;
	s_compressedtex_list.generic.name	= "Texture compression";
	s_compressedtex_list.generic.x		= 0;
	s_compressedtex_list.generic.y		= y+=y_offset;
	s_compressedtex_list.itemnames		= on_off_names;
	s_compressedtex_list.curvalue		= trap_Cvar_VariableValue( "gl_ext_compressed_textures" );
	s_compressedtex_list.generic.statusbar = "trades texture quality for speed";
	clamp ( s_compressedtex_list.curvalue, 0, 1 );

	s_cubemap_list.generic.type			= MTYPE_SPINCONTROL;
	s_cubemap_list.generic.name			= "Cubemaps";
	s_cubemap_list.generic.x			= 0;
	s_cubemap_list.generic.y			= y+=y_offset;
	s_cubemap_list.itemnames			= on_off_names;
	s_cubemap_list.curvalue				= trap_Cvar_VariableValue( "gl_ext_texture_cube_map" );
	clamp ( s_cubemap_list.curvalue, 0, 1 );

	s_texfilteranisotropic_slider.generic.type	= MTYPE_SLIDER;
	s_texfilteranisotropic_slider.generic.x		= 0;
	s_texfilteranisotropic_slider.generic.y		= y += y_offset;
	s_texfilteranisotropic_slider.generic.name	= "texture anisotropic filter";
	s_texfilteranisotropic_slider.minvalue		= 0;
	s_texfilteranisotropic_slider.maxvalue		= trap_Cvar_VariableValue( "gl_ext_texture_filter_anisotropic_max" );
	s_texfilteranisotropic_slider.curvalue		= trap_Cvar_VariableValue( "gl_ext_texture_filter_anisotropic" );
	s_texfilteranisotropic_slider.generic.statusbar	= sbar;
	clamp ( s_texfilteranisotropic_slider.curvalue, s_texfilteranisotropic_slider.minvalue, s_texfilteranisotropic_slider.maxvalue );

	y += y_offset;
	s_apply_action.generic.type		= MTYPE_ACTION;
	s_apply_action.generic.flags	= QMF_CENTERED;
	s_apply_action.generic.name		= "apply changes";
	s_apply_action.generic.x		= 0;
	s_apply_action.generic.y		= y+=y_offset;
	s_apply_action.generic.callback = ApplyChanges;

	Menu_AddItem( &s_glext_menu, ( void * ) &s_extensions_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_cva_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_multitexture_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_texenvcombine_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_glsl_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_compressedtex_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_cubemap_list );
	Menu_AddItem( &s_glext_menu, ( void * ) &s_texfilteranisotropic_slider );

	Menu_AddItem( &s_glext_menu, ( void * ) &s_apply_action );

	Menu_Center( &s_glext_menu );

	Menu_Init ( &s_glext_menu );
}

/*
================
GLExt_MenuDraw
================
*/
void GLExt_MenuDraw (void)
{
	/*
	** move cursor to a reasonable starting position
	*/
	Menu_AdjustCursor( &s_glext_menu, 1 );

	/*
	** draw the menu
	*/
	Menu_Draw( &s_glext_menu );
}

/*
================
GLExt_MenuKey
================
*/
const char *GLExt_MenuKey( int key )
{
	return Default_MenuKey( &s_glext_menu, key );
}

void M_Menu_GLExt_f (void)
{
	GLExt_MenuInit ();
	M_PushMenu( &s_glext_menu, GLExt_MenuDraw, GLExt_MenuKey );
}
