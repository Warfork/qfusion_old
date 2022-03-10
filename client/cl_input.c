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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t	*cl_nodelta;

extern	unsigned	sys_frame_time;
unsigned	frame_msec;
unsigned	old_sys_frame_time;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time

===============================================================================
*/

kbutton_t	in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack;
kbutton_t	in_up, in_down;

static void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1)
		return;		// still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2;	// down + impulse down
}

static void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

	val = (float)msec / frame_msec;

	return bound ( 0, val, 1 );
}

//==========================================================================

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;

cvar_t	*cl_freelook;
cvar_t	*cl_run;

// mouse variables
cvar_t	*in_mouse;
cvar_t	*m_filter;
cvar_t  *in_grabinconsole;
qboolean mlooking;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (void)
{
	float	speed;
	float	up, down;

	if (in_speed.state & 1)
		speed = cls.frametime * cl_anglespeedkey->value;
	else
		speed = cls.frametime;

	if (!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState (&in_left);
	}
	if (in_klook.state & 1)
	{
		cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState (&in_lookdown);

	cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * up;
	cl.viewangles[PITCH] += speed*cl_pitchspeed->value * down;
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd)
{
	CL_AdjustAngles ();

	memset (cmd, 0, sizeof(*cmd));

	VectorCopy (cl.viewangles, cmd->angles);
	if (in_strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

	if (! (in_klook.state & 1) )
	{
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_forwardspeed->value * CL_KeyState (&in_back);
	}

//
// adjust for speed key / running
//
	if ( (in_speed.state & 1) ^ cl_run->integer )
	{
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}
}

void CL_ClampPitch (void)
{
	float	pitch;

	pitch = SHORT2ANGLE(cl.frame.playerState.pmove.delta_angles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // wrapped
	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // wrapped

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;
	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}

/*
===========
CL_MouseMove
===========
*/
void CL_MouseMove (usercmd_t *cmd, int mx, int my)
{
	int mouse_x, mouse_y;
	static int old_mouse_x, old_mouse_y;

	if (cls.key_dest == key_menu) 
	{
		CL_UIModule_MouseMove (mx, my);
		return;
	}

	if( (cls.key_dest == key_console) && !in_grabinconsole->integer )
        return;
 
#if 0
	if (!mx && !my)
		return;
#endif

	if (m_filter->integer)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe->integer && mlooking))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || cl_freelook->integer) && !(in_strafe.state & 1))
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	else
		cmd->forwardmove -= m_forward->value * mouse_y;
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove (usercmd_t *cmd)
{
	static double extramsec = 0;
	int		ms;
	int		i;

//
// figure button bits
//
	if ( in_attack.state & 3 )
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;

	if (in_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~2;

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	// send milliseconds of time to apply the move
	extramsec += cls.frametime * 1000;
	ms = extramsec;
	extramsec -= ms;	// fractional part is left for next frame
	if (ms > 250)
		ms = 100;		// time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch ();

	for (i=0 ; i<3 ; i++)
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
}

/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd (void)
{
	usercmd_t	cmd;

	frame_msec = sys_frame_time - old_sys_frame_time;
	clamp ( frame_msec, 1, 200 );

	// get basic movement from keyboard
	CL_BaseMove (&cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (&cmd);

	CL_FinishMove (&cmd);

	old_sys_frame_time = sys_frame_time;

	return cmd;
}


void IN_CenterView (void)
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerState.pmove.delta_angles[PITCH]);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);

	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_freelook = Cvar_Get( "cl_freelook", "1", CVAR_ARCHIVE );
	cl_run = Cvar_Get ("cl_run", "1", CVAR_ARCHIVE);

	// mouse variables
	m_filter = Cvar_Get ("m_filter", "0", 0);
    in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
    in_grabinconsole = Cvar_Get ("in_grabinconsole", "0", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE);
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	sizebuf_t	buf;
	qbyte		data[128];
	int			i;
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;
	int			checksumIndex;

	// build a command even if not connected

	// save this command off for prediction
	i = cl.cmdNum = cls.netchan.outgoing_sequence & CMD_MASK;
	cmd = &cl.cmds[i];
	cl.cmd_time[i] = cls.realtime;	// for netgraph ping calculation

	*cmd = CL_CreateCmd ();

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	if (cls.state == ca_connected)
	{
		if (cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 1000)
			Netchan_Transmit (&cls.netchan, 0, data);
		return;
	}

	// send a userinfo update if needed
	if (userinfo_modified)
	{
		userinfo_modified = qfalse;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
	}

	SZ_Init (&buf, data, sizeof(data));

	if (cmd->buttons && cl.cin.time > 0 && !cl.attractloop 
		&& cls.realtime - cl.cin.time > 1000)
	{	// skip the rest of the cinematic
		SCR_StopCinematic ();
		SCR_FinishCinematic ();
		SCR_UpdateScreen ();
		return;
	}

	// begin a client move command
	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if (cl_nodelta->integer || !cl.frame.valid || cls.demowaiting)
		MSG_WriteLong (&buf, -1);	// no compression
	else
		MSG_WriteLong (&buf, cl.frame.serverFrame);

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	cmd = &cl.cmds[(cls.netchan.outgoing_sequence-2) & CMD_MASK];
	memset (&nullcmd, 0, sizeof(nullcmd));
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence-1) & CMD_MASK];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence) & CMD_MASK];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
		buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
		cls.netchan.outgoing_sequence);

	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);	
}
