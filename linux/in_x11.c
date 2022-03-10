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

static qboolean mouse_active = qfalse;
static qboolean dgamouse = qfalse;

qboolean mouse_avail;
int mouse_buttonstate;
int mouse_oldbuttonstate;
int mx, my;
int p_mouse_x, p_mouse_y;
int old_mouse_x, old_mouse_y;

static qboolean grabs_installed = qfalse;

static cvar_t *in_dgamouse;

static Time myxtime;

static Cursor CreateNullCursor (Display *display, Window root)
{
    Pixmap cursormask; 
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap (display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc = XCreateGC (display, cursormask, GCFunction, &xgc);
    XFillRectangle (display, cursormask, gc, 0, 0, 1, 1);

    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;

    cursor = XCreatePixmapCursor (display, cursormask, cursormask, &dummycolour, &dummycolour, 0, 0);
    XFreePixmap (display, cursormask);
    XFreeGC (display, gc);

    return cursor;
}

void install_grabs (void)
{

	XDefineCursor (x11display.dpy, x11display.win, CreateNullCursor(x11display.dpy, x11display.win));
	XGrabPointer (x11display.dpy, x11display.win, True, 0, GrabModeAsync, GrabModeAsync, x11display.win, None, CurrentTime);

	if (in_dgamouse->integer)
	{
		int MajorVersion, MinorVersion;

		if (XF86DGAQueryVersion (x11display.dpy, &MajorVersion, &MinorVersion)) 
		{ 
			XF86DGADirectVideo (x11display.dpy, x11display.scr, XF86DGADirectMouse);
			XWarpPointer (x11display.dpy, None, x11display.win, 0, 0, 0, 0, 0, 0);
			dgamouse = qtrue;

		} else
		{
			// unable to query, probalby not supported
			Com_Printf( PRINT_ALL, "Failed to detect XF86DGA Mouse\n" );
			Cvar_Set( "in_dgamouse", "0" );
			dgamouse = qfalse;
		}
	} else {
		p_mouse_x = x11display.win_width / 2;
		p_mouse_y = x11display.win_height / 2;
		XWarpPointer (x11display.dpy, None, x11display.win, 0, 0, 0, 0, p_mouse_x, p_mouse_y);
	}

	XGrabKeyboard (x11display.dpy, x11display.win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	mx = 0;
	my = 0;
	old_mouse_x = 0;
	old_mouse_y = 0;
	mouse_active = qtrue;
	grabs_installed = qtrue;
}

void uninstall_grabs (void)
{
	if (!x11display.dpy || !x11display.win)
		return;

	if (dgamouse)
	{
		dgamouse = qfalse;
		XF86DGADirectVideo (x11display.dpy, x11display.scr, 0);
	}

	XUngrabPointer (x11display.dpy, CurrentTime);
	XUngrabKeyboard (x11display.dpy, CurrentTime);

	// inviso cursor
	XUndefineCursor (x11display.dpy, x11display.win);

	mouse_active = qfalse;
	grabs_installed = qtrue;
}

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/

static int XLateKey(XKeyEvent *ev)
{
	int key;
	char buf[64];
	KeySym keysym;

	key = 0;

	XLookupString (ev, buf, sizeof buf, &keysym, 0);

	switch (keysym)
	{
		case XK_KP_Page_Up:	 key = K_KP_PGUP; break;
		case XK_Page_Up:	 key = K_PGUP; break;

		case XK_KP_Page_Down: key = K_KP_PGDN; break;
		case XK_Page_Down:	 key = K_PGDN; break;

		case XK_KP_Home: key = K_KP_HOME; break;
		case XK_Home:	 key = K_HOME; break;

		case XK_KP_End:  key = K_KP_END; break;
		case XK_End:	 key = K_END; break;

		case XK_KP_Left: key = K_KP_LEFTARROW; break;
		case XK_Left:	 key = K_LEFTARROW; break;

		case XK_KP_Right: key = K_KP_RIGHTARROW; break;
		case XK_Right:	key = K_RIGHTARROW;		break;

		case XK_KP_Down: key = K_KP_DOWNARROW; break;
		case XK_Down:	 key = K_DOWNARROW; break;

		case XK_KP_Up:   key = K_KP_UPARROW; break;
		case XK_Up:		 key = K_UPARROW;	 break;

		case XK_Escape: key = K_ESCAPE;		break;

		case XK_KP_Enter: key = K_KP_ENTER;	break;
		case XK_Return: key = K_ENTER;		 break;

		case XK_Tab:		key = K_TAB;			 break;

		case XK_F1:		 key = K_F1;				break;

		case XK_F2:		 key = K_F2;				break;

		case XK_F3:		 key = K_F3;				break;

		case XK_F4:		 key = K_F4;				break;

		case XK_F5:		 key = K_F5;				break;

		case XK_F6:		 key = K_F6;				break;

		case XK_F7:		 key = K_F7;				break;

		case XK_F8:		 key = K_F8;				break;

		case XK_F9:		 key = K_F9;				break;

		case XK_F10:		key = K_F10;			 break;

		case XK_F11:		key = K_F11;			 break;

		case XK_F12:		key = K_F12;			 break;

		case XK_BackSpace: key = K_BACKSPACE; break;

		case XK_KP_Delete: key = K_KP_DEL; break;
		case XK_Delete: key = K_DEL; break;

		case XK_Pause:	key = K_PAUSE;		 break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT;		break;

		case XK_Execute: 
		case XK_Control_L: 
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:	
		case XK_Meta_L: 
		case XK_Alt_R:	
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = K_KP_5;	break;

		case XK_Insert:key = K_INS; break;
		case XK_KP_Insert: key = K_KP_INS; break;

		case XK_KP_Multiply: key = '*'; break;
		case XK_KP_Add:  key = K_KP_PLUS; break;
		case XK_KP_Subtract: key = K_KP_MINUS; break;
		case XK_KP_Divide: key = K_KP_SLASH; break;

#if 0
		case 0x021: key = '1';break;/* [!] */
		case 0x040: key = '2';break;/* [@] */
		case 0x023: key = '3';break;/* [#] */
		case 0x024: key = '4';break;/* [$] */
		case 0x025: key = '5';break;/* [%] */
		case 0x05e: key = '6';break;/* [^] */
		case 0x026: key = '7';break;/* [&] */
		case 0x02a: key = '8';break;/* [*] */
		case 0x028: key = '9';;break;/* [(] */
		case 0x029: key = '0';break;/* [)] */
		case 0x05f: key = '-';break;/* [_] */
		case 0x02b: key = '=';break;/* [+] */
		case 0x07c: key = '\'';break;/* [|] */
		case 0x07d: key = '[';break;/* [}] */
		case 0x07b: key = ']';break;/* [{] */
		case 0x022: key = '\'';break;/* ["] */
		case 0x03a: key = ';';break;/* [:] */
		case 0x03f: key = '/';break;/* [?] */
		case 0x03e: key = '.';break;/* [>] */
		case 0x03c: key = ',';break;/* [<] */
#endif

		default:
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			if (key >= 1 && key <= 26) /* ctrl+alpha */
				key = key + 'a' - 1;
			break;
	} 

	return key;
}


void HandleEvents(void)
{
	XEvent event;
	qboolean dowarp = qfalse;
	int mwx = x11display.win_width / 2;
	int mwy = x11display.win_height / 2;

	if (!x11display.dpy || !x11display.win) 
		return;

	while (XPending(x11display.dpy))
	{
		XNextEvent (x11display.dpy, &event);

		switch (event.type) 
		{
			case KeyPress:
				myxtime = event.xkey.time;
			case KeyRelease:
				Key_Event (XLateKey(&event.xkey), event.type == KeyPress, Sys_Milliseconds());
				break;

			case MotionNotify:
				if (mouse_active)
				{
					if (dgamouse)
					{
						mx += event.xmotion.x_root * 2;
						my += event.xmotion.y_root * 2;
					}
					else
					{
						if( !event.xmotion.send_event ) {
    						mx += event.xmotion.x - p_mouse_x;
						    my += event.xmotion.y - p_mouse_y;

						    if( abs( mwx - event.xmotion.x ) > mwx / 2 || abs( mwy - event.xmotion.y ) > mwy / 2 )
								dowarp = qtrue;
						}
						p_mouse_x = event.xmotion.x;
						p_mouse_y = event.xmotion.y;
					}
				}
				break;

			case ButtonPress:
				myxtime = event.xbutton.time;
				if (event.xbutton.button == 1) Key_Event(K_MOUSE1, 1, Sys_Milliseconds());
				else if (event.xbutton.button == 2) Key_Event(K_MOUSE3, 1, Sys_Milliseconds());
				else if (event.xbutton.button == 3) Key_Event(K_MOUSE2, 1, Sys_Milliseconds());
				else if (event.xbutton.button == 4) Key_Event(K_MWHEELUP, 1, Sys_Milliseconds());
				else if (event.xbutton.button == 5) Key_Event(K_MWHEELDOWN, 1, Sys_Milliseconds());
				else if (event.xbutton.button >= 6 && event.xbutton.button <= 10) Key_Event(K_MOUSE4+event.xbutton.button-6, 1, Sys_Milliseconds());
				break;

			case ButtonRelease:
				if (event.xbutton.button == 1) Key_Event(K_MOUSE1, 0, Sys_Milliseconds());
				else if (event.xbutton.button == 2) Key_Event(K_MOUSE3, 0, Sys_Milliseconds());
				else if (event.xbutton.button == 3) Key_Event(K_MOUSE2, 0, Sys_Milliseconds());
				else if (event.xbutton.button == 4) Key_Event(K_MWHEELUP, 0, Sys_Milliseconds());
				else if (event.xbutton.button == 5) Key_Event(K_MWHEELDOWN, 0, Sys_Milliseconds());
				else if (event.xbutton.button >= 6 && event.xbutton.button <= 10) Key_Event(K_MOUSE4+event.xbutton.button-6, 0, Sys_Milliseconds());
				break;

			case ClientMessage:
				if (event.xclient.data.l[0] == x11display.wmDeleteWindow) 
					Cbuf_ExecuteText(EXEC_NOW, "quit");
				break;

			case MapNotify:
				if (mouse_avail) {
					uninstall_grabs ();
					install_grabs ();
				}
				break;
		}
	}

	if (dowarp) 
	{
		/* move the mouse to the window center again */
		p_mouse_x = mwx;
		p_mouse_y = mwy;
		XWarpPointer (x11display.dpy, None, x11display.win, 0, 0, 0, 0, mwx, mwy);
	}
}

/*****************************************************************************/

char *Sys_GetClipboardData(void)
{
	Window sowner;
	Atom type, property;
	unsigned long len, bytes_left, tmp;
	unsigned char *data;
	int format, result;
	char *ret = NULL;
		
	if (!x11display.dpy && x11display.win) 
		return NULL;
	
	sowner = XGetSelectionOwner (x11display.dpy, XA_PRIMARY);
			
	if (sowner != None)
	{
		property = XInternAtom(x11display.dpy, "GETCLIPBOARDDATA_PROP", False);
				
		XConvertSelection (x11display.dpy, XA_PRIMARY, XA_STRING, property, x11display.win, myxtime); /* myxtime == time of last X event */
		XFlush (x11display.dpy);

		XGetWindowProperty (x11display.dpy, x11display.win, property,  0, 0, False, AnyPropertyType, &type, &format, &len, &bytes_left, &data);

		if (bytes_left > 0) 
		{
			result = XGetWindowProperty(x11display.dpy, x11display.win, property,
				0, bytes_left, True, AnyPropertyType,  &type, &format, &len, &tmp, &data);

			if (result == Success){
				ret = strdup(data);
			}

			XFree(data);
		}
	}

	return ret;
}

/*****************************************************************************/

void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	if (mouse_avail)
	{
		CL_MouseMove (cmd, mx, my);
		mx = my = 0;
	}
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
	if (!mouse_avail)
		return;

	if (active) 
		IN_ActivateMouse ();
	else 
		IN_DeactivateMouse ();
}

void IN_Init(void)
{
	// mouse variables
	in_dgamouse = Cvar_Get ("in_dgamouse", "1", CVAR_ARCHIVE);

	mx = my = 0.0;
	mouse_avail = qtrue;
}

void IN_Shutdown (void)
{
	IN_Activate (qfalse);

	mouse_active = qfalse;
	dgamouse = qfalse;
}

void IN_Frame (void)
{
    if( (cls.key_dest == key_console) && !in_grabinconsole->integer && !Cvar_VariableValue( "vid_fullscreen" ) )
		IN_Activate( qfalse );
	else
		IN_Activate( qtrue );

	HandleEvents();
}
