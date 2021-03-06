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
// console.c

#include "client.h"

console_t	con;

cvar_t		*con_notifytime;


#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		
void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_Close
================
*/
void Con_Close (void)
{
	scr_con_current = 0;

	Key_ClearTyping ();
	Con_ClearNotify ();
	Key_ClearStates ();

	if( !Cvar_VariableValue( "vid_fullscreen" ) && !Cvar_VariableValue( "in_grabinconsole" ) )
		IN_Activate( qtrue );
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque ();	// get rid of loading plaque

#if 1
	if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}
#endif

	if (cls.state == ca_connecting || cls.state == ca_connected)
		return;

	Key_ClearTyping ();
	Con_ClearNotify ();

	if( cls.key_dest == key_console ) {
		CL_SetKeyDest( cls.old_key_dest );
		Key_ClearStates ();
		Cvar_Set( "paused", "0" );
		
		if( !Cvar_VariableValue( "vid_fullscreen" ) && !Cvar_VariableValue( "in_grabinconsole" ) )
			IN_Activate( qtrue );
	} else {
		Key_ClearStates ();

		CL_SetOldKeyDest( cls.key_dest );
		CL_SetKeyDest( key_console );

		if( Cvar_VariableValue( "sv_maxclients" ) == 1 && Com_ServerState () )
			Cvar_Set( "paused", "1" );
		if( !Cvar_VariableValue( "vid_fullscreen" ) && !Cvar_VariableValue( "in_grabinconsole" ) )
			IN_Activate( qfalse );			
	}
}


/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	memset (con.text, ' ', CON_TEXTSIZE);
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Q_snprintfz (name, sizeof(name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf ("Dumped console text to %s.\n", name);
	FS_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		strncpy (buffer, line, con.linewidth);
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con.times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = qfalse;
	if (cls.state == ca_active)
		CL_SetKeyDest (key_message);
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = qtrue;
	if (cls.state == ca_active)
		CL_SetKeyDest (key_message);
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = viddef.width / SMALL_CHAR_WIDTH - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 78;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset (con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		memset (con.text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con.linewidth = -1;

	Con_CheckResize ();
	
	Com_Printf ("Console initialized.\n");

//
// register our commands
//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = qtrue;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con.x = 0;
	if (con.display/* == con.current*/)
		con.display++;
	con.current++;
	memset (&con.text[(con.current%con.totallines)*con.linewidth]
	, ' ', con.linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		color;

	if (!con.initialized)
		return;

	color = ColorIndex ( COLOR_WHITE );

	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con.linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con.linewidth && (con.x + l > con.linewidth) )
			con.x = 0;

		if (cr)
		{
			con.current--;
			cr = qfalse;
		}

		
		if (!con.x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;

			y = con.current % con.totallines;

			if ( color != ColorIndex ( COLOR_WHITE ) ) {
				con.text[y*con.linewidth] = '^';
				con.text[y*con.linewidth+1] = '0' + color;
				con.x += 2;
			}
		}

		switch (c)
		{
		case '\n':
			color = ColorIndex ( COLOR_WHITE );
			con.x = 0;
			break;

		case '\r':
			color = ColorIndex ( COLOR_WHITE );
			con.x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = c;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;

			if ( Q_IsColorString( txt ) ) {
				color = ColorIndex ( *(txt+1) );
			}
			break;
		}

		txt++;
	}
}

/*
==============================================================================

DRAWING

==============================================================================
*/


int Q_ColorCharCount (char *s, int byteofs)
{
	int c;
	char *end;

	end = s + byteofs;

	for (c = 0; *s && s < end; s++, c++)
		if (Q_IsColorString(s))
			s++, c--;

	return c;
}

int Q_ColorCharOffset (char *s, int charcount)
{
	char *start = s;

	for ( ; *s && charcount; s++)
	{
		if (Q_IsColorString(s))
			s++;
		else
			charcount--;
	}

	return s - start;
}

int Q_ColorStrLastColor (char *s, int byteofs)
{
	int c;
	char *end;
	int lastcolor = ColorIndex(COLOR_WHITE);

	end = s + byteofs - 1;	// don't check last byte

	for (c = 0; s < end && *s; s++, c++)
		if (Q_IsColorString(s))
		{
			lastcolor = ColorIndex(s[1]);
			s++, c--;
		}

	return lastcolor;
}



/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	char	*text;
	extern qboolean	key_insert;
	int		colorlinepos;
	int		startcolor = ColorIndex(COLOR_WHITE);
	int		byteofs;
	int		bytelen;

	if (cls.key_dest != key_console)
		return;

	text = key_lines[edit_line];

	// convert byte offset to visible character count
	colorlinepos = Q_ColorCharCount (text, key_linepos);

	// prestep if horizontally scrolling
	if (colorlinepos >= con.linewidth + 1)
	{
		byteofs = Q_ColorCharOffset (text, colorlinepos - con.linewidth);
		startcolor = Q_ColorStrLastColor (text, byteofs);
		text += byteofs;
		colorlinepos = con.linewidth;
	}

	// draw it
	bytelen = Q_ColorCharOffset (text, con.linewidth);
	Draw_StringLen ( 8, con.vislines-SMALL_CHAR_HEIGHT-14, text, bytelen, color_table[startcolor] );

	// add the cursor frame
	if ((int)(cls.realtime>>8)&1)
		Draw_Char ( 8+colorlinepos*SMALL_CHAR_WIDTH, con.vislines-SMALL_CHAR_HEIGHT-14, key_insert ? '_' : 11, colorWhite);
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		v;
	char	*text;
	int		i;
	int		time;
	char	*s;
	int		skip;

	v = 0;
	for (i= con.current-NUM_CON_TIMES+1 ; i<=con.current ; i++)
	{
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value*1000)
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;
		
		Draw_StringLen (8, v, text, con.linewidth, colorWhite);

		v += SMALL_CHAR_HEIGHT;
	}

	if (cls.key_dest == key_message)
	{
		if (chat_team)
		{
			Draw_String ( 8, v, "say_team:", colorWhite );
			skip = 11;
		}
		else
		{
			Draw_String ( 8, v, "say:", colorWhite );
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen >viddef.width/SMALL_CHAR_WIDTH-(skip+1))
			s += chat_bufferlen - (viddef.width/SMALL_CHAR_WIDTH-(skip+1));

		Draw_String ( skip*SMALL_CHAR_WIDTH, v, s, colorWhite );
		Draw_Char ( (strlen(s)+skip)*SMALL_CHAR_WIDTH, v, 10+((cls.realtime>>8)&1), colorWhite );
		v += SMALL_CHAR_HEIGHT;
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac)
{
	int				i, x, y;
	int				rows;
	char			*text;
	int				row;
	int				lines;
	char			version[64];

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

// draw the background
	R_DrawStretchPic (0, 0, viddef.width, lines, 0, 0, 1, 1, colorWhite, cls.consoleShader );
	Draw_FillRect (0, lines - 2, viddef.width, 2, colorRed);

	Q_snprintfz (version, sizeof(version), APPLICATION " v%4.2f", VERSION);
	Draw_String (viddef.width-strlen(version)*SMALL_CHAR_WIDTH-4, lines-20, version, colorRed );

// draw the text
	con.vislines = lines;
	
	rows = (lines-SMALL_CHAR_HEIGHT-14) / SMALL_CHAR_HEIGHT;		// rows of text to draw

	y = lines - SMALL_CHAR_HEIGHT-14-SMALL_CHAR_HEIGHT;

// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con.linewidth ; x+=4)
			Draw_Char ( (x+1)*SMALL_CHAR_WIDTH, y, '^', colorRed );
	
		y -= SMALL_CHAR_HEIGHT;
		rows--;
	}
	
	row = con.display;
	for (i=0 ; i<rows ; i++, y-=SMALL_CHAR_HEIGHT, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point
			
		text = con.text + (row % con.totallines)*con.linewidth;

		Draw_StringLen (8, y, text, con.linewidth, colorWhite);
	}

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}


/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

*/
void Con_DisplayList(char **list)
{
	int	i = 0;
	int	pos = 0;
	int	len = 0;
	int	maxlen = 0;
	int	width = (con.linewidth - 4);
	char	**walk = list;

	while (*walk) {
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen(*list);

		if (pos + maxlen >= width) {
			Com_Printf("\n");
			pos = 0;
		}

		Com_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Com_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Com_Printf("\n\n");
}

/*
	Con_CompleteCommandLine

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
void Con_CompleteCommandLine (void)
{
	char	*cmd = "";
	char	*s;
	int		c, v, a, i;
	int		cmd_len;
	char	**list[3] = {0, 0, 0};

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;
	if (!*s)
		return;

	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);
	
	if (!(c + v + a)) {	// No possible matches, let the user know they're insane
		Com_Printf("\nNo matching aliases, commands, or cvars were found.\n\n");
		return;
	}
	
	if (c + v + a == 1) {
		if (c)
			list[0] = Cmd_CompleteBuildList(s);
		else if (v)
			list[0] = Cvar_CompleteBuildList(s);
		else
			list[0] = Cmd_CompleteAliasBuildList(s);
		cmd = *list[0];
		cmd_len = strlen (cmd);
	} else {
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList(s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList(s));
		if (a)
			cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));

		cmd_len = strlen (s);
		do {
			for (i = 0; i < 3; i++) {
				char ch = cmd[cmd_len];
				char **l = list[i];
				if (l) {
					while (*l && (*l)[cmd_len] == ch)
						l++;
					if (*l)
						break;
				}
			}
			if (i == 3)
				cmd_len++;
		} while (i == 3);

		// Print Possible Commands
		if (c) {
			Com_Printf(S_COLOR_RED "%i possible command%s%s\n", c, (c > 1) ? "s: " : ":", S_COLOR_WHITE);
			Con_DisplayList(list[0]);
		}
		
		if (v) {
			Com_Printf(S_COLOR_RED "%i possible variable%s%s\n", v, (v > 1) ? "s: " : ":", S_COLOR_WHITE);
			Con_DisplayList(list[1]);
		}
		
		if (a) {
			Com_Printf(S_COLOR_RED "%i possible alias%s%s\n", a, (a > 1) ? "es: " : ":", S_COLOR_WHITE);
			Con_DisplayList(list[2]);
		}
	}
	
	if (cmd) {
		strcpy(key_lines[edit_line] + 1, cmd);
		key_linepos = cmd_len + 1;
		if (c + v + a == 1) {
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
		}
		key_lines[edit_line][key_linepos] = 0;
	}

	for (i = 0; i < 3; i++)
		if (list[i])
			Mem_TempFree (list[i]);
}

