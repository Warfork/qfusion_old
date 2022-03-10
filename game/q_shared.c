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
#include "q_shared.h"

//============================================================================

/*
============
COM_SkipPath
============
*/
const char *COM_SkipPath( const char *pathname )
{
	const char *last;

	last = pathname;
	while( *pathname ) {
		if( *pathname == '/' )
			last = pathname + 1;
		pathname++;
	}

	return last;
}

/*
============
COM_StripExtension
============
*/
char *COM_StripExtension( const char *in, char *out )
{
	const char *last = NULL;

	if( in && *in ) {
		last = strrchr( in, '.' );
		if( !last || strrchr( in, '/' ) > last )
			last = in + strlen( in );

		if( out != in )
			memcpy( out, in, last - in );
		out[last-in] = 0;
	}

	return out;
}

/*
============
COM_FileExtension
============
*/
const char *COM_FileExtension( const char *in )
{
	const char *src, *last = NULL;

	if ( !in || !*in )
		return in;

	src = in + strlen( in ) - 1;
	while( *src != '/' && src != in ) {
		if( *src == '.' )
			last = src;
		src--;
	}

	return last;
}

/*
============
COM_FileBase
============
*/
char *COM_FileBase( const char *in, char *out )
{
	const char *p, *start, *end;
	
	end = in + strlen( in );
	start = in;
	
	for( p = end - 1; p > in ; p-- ) {
		if( *p == '.' ) {
			end = p;
		} else if( *p == '/' ) {
			start = p + 1;
			break;
		}
	}
	
	if( end <= start ) {
		*out = 0;
		return out;
	}

	memmove( out, start, end - start );
	out[end - start] = 0;
	return out;
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
char *COM_FilePath( const char *in, char *out )
{
	const char *s;

	if( in && *in ) {
		s = in + strlen( in ) - 1;
		while( s != in && *s != '/' )
			s--;

		memmove( out, in, s - in );
		out[s - in] = 0;
	}

	return out;
}


/*
==================
COM_DefaultExtension
==================
*/
char *COM_DefaultExtension( char *path, const char *extension )
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	if( path && *path ) {
		src = path + strlen( path ) - 1;

		while( *src != '/' && src != path ) {
			if( *src == '.' )
				return path;		// it has an extension
			src--;
		}

		strcat( path, extension );
	}

	return path;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int     (*BigLong) (int l);
int     (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);
#endif

short   ShortSwap (short l)
{
	qbyte    b1, b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   ShortNoSwap (short l)
{
	return l;
}
#endif

int    LongSwap (int l)
{
	qbyte    b1, b2, b3, b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
int     LongNoSwap (int l)
{
	return l;
}
#endif

float FloatSwap (float f)
{
	union
	{
		float   f;
		qbyte   b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
float FloatNoSwap (float f)
{
	return f;
}
#endif


/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	qbyte    swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner
	if ( *(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
#endif
}



/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv (float x, float y, float z)
{
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/*
=============
VectorToString

This is just a convenience function for printing vectors
=============
*/
char	*vtos (vec3_t v)
{
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Q_snprintfz (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char	*va(char *format, ...)
{
	va_list		argptr;
	static int  str_index;
	static char	string[2][2048];

	str_index = !str_index;
	va_start (argptr, format);
	vsnprintf (string[str_index], sizeof(string[str_index]), format, argptr);
	va_end (argptr);

	return string[str_index];
}


char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_ParseExt

Parse a token out of a string
==============
*/
char *COM_ParseExt (char **data_p, qboolean nl)
{
	int		c;
	int		len;
	char	*data;
	qboolean newlines = qfalse;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		if (c == '\n')
			newlines = qtrue;
		data++;
	}

	if ( newlines && !nl ) {
		*data_p = data;
		return com_token;
	}

// skip // comments
	if (c == '/' && data[1] == '/')
	{
		data += 2;

		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// skip /* */ comments
	if (c == '/' && data[1] == '*')
	{
		data += 2;

		while (1)
		{
			if (!*data)
				break;
			if (*data != '*' || *(data+1) != '/')
				data++;
			else
			{
				data += 2;
				break;
			}
		}
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)
				{
//					Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
					len = 0;
				}
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

/*
==============
Q_strncpyz
==============
*/
void Q_strncpyz( char *dest, const char *src, size_t size )
{
#ifdef HAVE_STRLCPY
	strlcpy( dest, src, size );
#else
	if( size ) {
		while( --size && (*dest++ = *src++) );
		*dest = '\0';
	}
#endif
}

/*
==============
Q_strncatz
==============
*/
void Q_strncatz( char *dest, const char *src, size_t size )
{
#ifdef HAVE_STRLCAT
	strlcat( dest, src, size );
#else
	if( size ) {
		while( --size && *dest++ );
		if( size ) {
			dest--;
			while( --size && (*dest++ = *src++) );
		}
		*dest = '\0';
	}
#endif
}

/*
==============
Q_snprintfz
==============
*/
void Q_snprintfz( char *dest, size_t size, const char *fmt, ... )
{
	va_list	argptr;

	if( size ) {
		va_start( argptr, fmt );
		vsnprintf( dest, size, fmt, argptr );
		va_end( argptr );

		dest[size-1] = 0;
	}
}

/*
==============
Q_strlwr
==============
*/
char *Q_strlwr( char *s )
{
	char *p;

	if( s ) {
		for( p = s; *s; s++ )
			*s = tolower( *s );
		return p;
	}

	return NULL;
}

/*
==============
Q_isdigit
==============
*/
qboolean Q_isdigit( char *str )
{
	if( str && *str ) {
		while( isdigit( *str ) ) str++;
		if( !*str )
			return qtrue;
	}
	return qfalse;
}

/*
============================================================================

					WILDCARD COMPARES FUNCTIONS

============================================================================
*/

/*
==============
Q_WildCmpAfterStar
==============
*/
static qboolean Q_WildCmpAfterStar( const char *pattern, const char *text )
{
	char c, c1;
	const char *p = pattern, *t = text;

	while( (c = *p++) == '?' || c == '*' ) {
		if( c == '?' && *t++ == '\0' )
			return qfalse;
	}

	if( c == '\0' )
		return qtrue;

	for( c1 = ((c == '\\') ? *p : c); ; ) {
		if( tolower( *t ) == c1 && Q_WildCmp( p - 1, t ) )
			return qtrue;
		if( *t++ == '\0' )
			return qfalse;
	}
}

/*
==============
Q_WildCmp
==============
*/
qboolean Q_WildCmp( const char *pattern, const char *text )
{
	char c;

	while( (c = *pattern++) != '\0' ) {
		switch( c ) {
			case '?':
				if( *text++ == '\0' )
					return qfalse;
				break;
			case '\\':
				if( tolower( *pattern++ ) != tolower( *text++ ) )
					return qfalse;
				break;
			case '*':
				return Q_WildCmpAfterStar( pattern, text );
			default:
				if( tolower( c ) != tolower( *text++ ) )
					return qfalse;
		}
	}

	return (*text == '\0');
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return qfalse;
	if (strstr (s, ";"))
		return qfalse;

	// wsw: r1q2: check for end-of-message-in-string exploit
	if( strchr (s, '\xFF') )
		return qfalse;
	// wsw : r1q2[end]

	return qtrue;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be less than %i characters.\n", MAX_INFO_KEY);
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Q_snprintfz (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_INFO_STRING)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}

