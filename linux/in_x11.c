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
#include "linux/x11.h"
#include "client/client.h"

extern qboolean	mlooking;
extern cvar_t *m_filter;
extern cvar_t *in_mouse;
extern cvar_t *in_dgamouse;
extern cvar_t *freelook;
extern qboolean mouse_active;

extern qboolean mouse_avail;
extern int mouse_buttonstate;
extern int mouse_oldbuttonstate;
extern int mx, my;
extern int old_mouse_x, old_mouse_y;

extern x11display_t x11display;

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}


void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	if (!mouse_avail) 
		return;

	if ( cls.key_dest == key_menu ) {
		CL_UIModule_MouseMove ( mx, my );
		mx = my = 0;
		return;
	}

	if (m_filter->value)
	{
		mx = (mx + old_mouse_x) * 0.5;
		my = (my + old_mouse_y) * 0.5;
	}
  
	old_mouse_x = mx;
	old_mouse_y = my;
  
	mx *= sensitivity->value;
	my *= sensitivity->value;
  
	cl.viewangles[YAW] -= m_yaw->value * mx;
	cl.viewangles[PITCH] += m_pitch->value * my;
    
	mx = my = 0;
}

static void IN_DeactivateMouse( void )
{
	if (!x11display.dpy || !x11display.win) 
		return;

	if (mouse_active)
	{
		uninstall_grabs ();
		mouse_active = qfalse;
	}
}

static void IN_ActivateMouse( void )
{
	if (!x11display.dpy || !x11display.win) 
		return;

	if (!mouse_active)
	{
		mx = my = 0; // don't spazz
		install_grabs ();
		mouse_active = qtrue;
	}
}


void IN_Activate (qboolean active)
{
	if (active) 
		IN_ActivateMouse ();
	else 
		IN_DeactivateMouse ();
}


void IN_Init(void)
{
	// mouse variables
	m_filter = Cvar_Get ("m_filter", "0", 0);
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
	in_dgamouse = Cvar_Get ("in_dgamouse", "1", CVAR_ARCHIVE);
	freelook = Cvar_Get ("cl_freelook", "0", 0);
	lookstrafe = Cvar_Get ("lookstrafe", "0", 0);
	sensitivity = Cvar_Get ("sensitivity", "3", 0);
	m_pitch = Cvar_Get ("m_pitch", "0.022", 0);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "0.8", 0);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);

	mx = my = 0.0;
	mouse_avail = qtrue;
}

void IN_Shutdown (void)
{
	if (mouse_avail)
	{
		mouse_avail = qfalse;
		Cmd_RemoveCommand ("force_centerview");
	}
}

void IN_Frame (void)
{
	IN_Activate (qtrue);
	HandleEvents();
}
