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
	
// q_shared.h -- included first by ALL program modules
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

//#define SKELMOD	// enable for using SKM player models instead of Q3 ones

//==============================================

#ifdef _WIN32

// unknown pragmas are SUPPOSED to be ignored, but....
# pragma warning(disable : 4244)		// MIPS
# pragma warning(disable : 4136)		// X86
# pragma warning(disable : 4051)		// ALPHA

# pragma warning(disable : 4018)		// signed/unsigned mismatch
# pragma warning(disable : 4305)		// truncation from const double to float

# if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
#  pragma warning(disable : 4996)		// function or variable may be unsafe
# endif

# if defined _M_AMD64
#  pragma warning(disable : 4267)		// conversion from 'size_t' to whatever, possible loss of data
# endif

# define HAVE___INLINE

# define HAVE__SNPRINTF

# define HAVE__VSNPRINTF

# define HAVE__STRICMP

# ifdef LCC_WIN32
#  ifndef C_ONLY
#   define C_ONLY
#  endif
#  define HAVE_TCHAR
#  define HAVE_MMSYSTEM
#  define HAVE_DLLMAIN
# endif

# define VID_INITFIRST

# define GL_DRIVERNAME	"opengl32.dll"

# define VORBISFILE_LIBNAME	"vorbisfile.dll"

# define OPENAL_LIBNAME	"openal32.dll"

# ifdef NDEBUG
#  define BUILDSTRING "Win32 RELEASE"
# else
#  define BUILDSTRING "Win32 DEBUG"
# endif

# ifdef _M_IX86
#  define ARCH "x86"
#  define CPUSTRING	"x86"
# elif defined(_M_ALPHA)
#  define ARCH "axp"
#  define CPUSTRING	"AXP"
# elif defined(_M_AMD64)
#  define ARCH "x64"
#  define CPUSTRING	"x64"
# else
#  define ARCH "xxx"
#  define CPUSTRING "Unknown"
# endif

// doh, some compilers need a _ prefix for variables so they can be 
// used in asm code
# ifdef __GNUC__	// mingw
#  define VAR(x)	"_"#x
# else
#  define VAR(x)	#x
# endif

#endif

//==============================================

#if defined(__linux__) || defined(__FreeBSD__)

# define HAVE_INLINE

# define HAVE_STRCASECMP

//# define GL_FORCEFINISH
# define GL_DRIVERNAME	"libGL.so.1"

# define VORBISFILE_LIBNAME	"libvorbisfile.so"

# define OPENAL_LIBNAME	"libopenal.so.0"

# ifdef __FreeBSD__
#  define BUILDSTRING "FreeBSD"
# else
#  define BUILDSTRING "Linux"
# endif

# ifdef __i386__
#  define ARCH "i386"
#  define CPUSTRING "i386"
# elif defined(__alpha__)
#  define ARCH "axp"
#  define CPUSTRING "axp"
# elif defined (__powerpc__)
#  define ARCH "ppc"
#  define CPUSTRING "ppc"
# elif defined(__x86_64__)
#  define ARCH "x86_64"
#  define CPUSTRING "x86_64"
# elif defined(__sparc__)
#  define ARCH "sparc"
#  define CPUSTRING "sparc"
# else
#  define ARCH "xxx"
#  define CPUSTRING "Unknown"
# endif

# define VAR(x)	#x

# ifdef _GNU_SOURCE
#  define HAVE_SINCOSF
# endif

#endif

//==============================================

#ifdef HAVE___INLINE
# ifndef inline
#  define inline __inline
# endif
#elif !defined(HAVE_INLINE)
# ifndef inline
#  define inline
# endif
#endif

#ifdef HAVE__SNPRINTF
# ifndef snprintf 
#  define snprintf _snprintf
# endif
#endif

#ifdef HAVE__VSNPRINTF
# ifndef vsnprintf 
#  define vsnprintf _vsnprintf
# endif
#endif

#ifdef HAVE__STRICMP
# ifndef Q_stricmp 
#  define Q_stricmp(s1, s2) _stricmp((s1), (s2))
# endif
# ifndef Q_strnicmp 
#  define Q_strnicmp(s1, s2, n) _strnicmp((s1), (s2), (n))
# endif
#endif

#ifdef HAVE_STRCASECMP
# ifndef Q_stricmp 
#  define Q_stricmp(s1, s2) strcasecmp((s1), (s2))
# endif
# ifndef Q_strnicmp 
#  define Q_strnicmp(s1, s2, n) strncasecmp((s1), (s2), (n))
# endif
#endif

#if (defined(_M_IX86) || defined(__i386__) || defined(__ia64__)) && !defined(C_ONLY)
# define id386
#else
# ifdef id386
#  undef id386
# endif
#endif

#ifndef BUILDSTRING
# define BUILDSTRING "NON-WIN32"
#endif

#ifndef ARCH
# define ARCH "xxx"
#endif

#ifndef CPUSTRING
# define CPUSTRING	"NON-WIN32"
#endif

#ifdef HAVE_TCHAR
# include <tchar.h>
#endif

#if defined(__GNUC__)
# define ALIGN(x)	__attribute__((aligned(x)))
#elif defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
# define ALIGN(x)	__declspec(align(16))
#else
# define ALIGN(x)
#endif

#define HARDWARE_OUTLINES

//==============================================

typedef unsigned char 			qbyte;
typedef enum {qfalse, qtrue}	qboolean;


#ifndef NULL
# define NULL ((void *)0)
#endif

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	256		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			128		// max length of a filesystem pathname

//
// per-level limits
//
#define	MAX_CLIENTS			256		// absolute limit
#define	MAX_EDICTS			1024	// must change protocol to increase more
#define	MAX_LIGHTSTYLES		256
#define	MAX_MODELS			256		// these are sent over the net as bytes
#define	MAX_SOUNDS			256		// so they cannot be blindly increased
#define	MAX_IMAGES			256
#define MAX_SKINFILES	    256
#define	MAX_ITEMS			256
#define MAX_GENERAL			(MAX_CLIENTS*2)	// general config strings

#define MAX_CM_AREAS		0x100

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages

// command line execution flags
#define	EXEC_NOW	0				// don't return until completed
#define	EXEC_INSERT	1				// insert at current position, but don't run yet
#define	EXEC_APPEND	2				// add to end of the command buffer

/*
==============================================================

MATHLIB

==============================================================
*/

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef vec_t quat_t[4];

typedef qbyte byte_vec4_t[4];

struct cplane_s;

extern vec3_t vec3_origin;
extern vec3_t axis_identity[3];
extern quat_t quat_identity;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#ifndef M_PI
# define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#ifndef M_TWOPI
# define M_TWOPI	6.28318530717958647692
#endif

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F
#define RAD2DEG( a ) ( a * 180.0F ) / M_PI


// returns b clamped to [a..c] range
//#define bound(a,b,c) (max((a), min((b), (c))))

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define bound(a,b,c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

// clamps a (must be lvalue) to [b..c] range
#define clamp(a,b,c) ((b) >= (c) ? (a)=(b) : (a) < (b) ? (a)=(b) : (a) > (c) ? (a)=(c) : (a))

#define random()		((rand () & 0x7fff) / ((float)0x7fff))	// 0..1
#define brandom(a,b)	((a)+random()*((b)-(a)))				// a..b
#define crandom()		brandom(-1,1)							// -1..1 

int		Q_rand (int *seed);
#define Q_random(seed)		((Q_rand (seed) & 0x7fff) / ((float)0x7fff))	// 0..1
#define Q_brandom(seed,a,b)	((a)+Q_random(seed)*((b)-(a)))						// a..b
#define Q_crandom(seed)		Q_brandom(seed,-1,1)

float	Q_RSqrt (float number);
int		Q_log2 (int val);

#define SQRTFAST( x ) ( (x) * Q_RSqrt( x ) ) // The expression a * rsqrt(b) is intended as a higher performance alternative to a / sqrt(b). The two expressions are comparably accurate, but do not compute exactly the same value in every case. For example, a * rsqrt(a*a + b*b) can be just slightly greater than 1, in rare cases.

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define CrossProduct(v1,v2,cross) ((cross)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1],(cross)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2],(cross)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorAvg(a,b,c)		((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorMA(a,b,c,d)		((d)[0]=(a)[0]+(b)*(c)[0],(d)[1]=(a)[1]+(b)*(c)[1],(d)[2]=(a)[2]+(b)*(c)[2])
#define VectorCompare(v1,v2)	((v1)[0]==(v2)[0] && (v1)[1]==(v2)[1] && (v1)[2]==(v2)[2])
#define VectorLength(v)			(sqrt(DotProduct((v),(v))))
#define VectorInverse(v)		((v)[0]=-(v)[0],(v)[1]=-(v)[1],(v)[2]=-(v)[2])
#define VectorLerp(a,c,b,v)		((v)[0]=(a)[0]+(c)*((b)[0]-(a)[0]),(v)[1]=(a)[1]+(c)*((b)[1]-(a)[1]),(v)[2]=(a)[2]+(c)*((b)[2]-(a)[2]))
#define VectorScale(in,scale,out) ((out)[0]=(in)[0]*(scale),(out)[1]=(in)[1]*(scale),(out)[2]=(in)[2]*(scale))

#define DistanceSquared(v1,v2) (((v1)[0]-(v2)[0])*((v1)[0]-(v2)[0])+((v1)[1]-(v2)[1])*((v1)[1]-(v2)[1])+((v1)[2]-(v2)[2])*((v1)[2]-(v2)[2]))
#define Distance(v1,v2) (sqrt(DistanceSquared(v1,v2)))

#define VectorLengthFast(v)		(SQRTFAST(DotProduct((v),(v))))
#define DistanceFast(v1,v2)		(SQRTFAST(DistanceSquared(v1,v2)))

#define Vector2Set(v,x,y)		((v)[0]=(x),(v)[1]=(y))
#define Vector2Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1])
#define Vector2Avg(a,b,c)		((c)[0]=(((a[0])+(b[0]))*0.5f),(c)[1]=(((a[1])+(b[1]))*0.5f)) 

#define Vector4Set(v, a, b, c, d)	((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3] = (d))
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Scale(in,scale,out)		((out)[0]=(in)[0]*scale,(out)[1]=(in)[1]*scale,(out)[2]=(in)[2]*scale,(out)[3]=(in)[3]*scale)
#define Vector4Add(a,b,c)		((c)[0]=(((a[0])+(b[0]))),(c)[1]=(((a[1])+(b[1]))),(c)[2]=(((a[2])+(b[2]))),(c)[3]=(((a[3])+(b[3])))) 
#define Vector4Avg(a,b,c)		((c)[0]=(((a[0])+(b[0]))*0.5f),(c)[1]=(((a[1])+(b[1]))*0.5f),(c)[2]=(((a[2])+(b[2]))*0.5f),(c)[3]=(((a[3])+(b[3]))*0.5f)) 

vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (const vec3_t v, vec3_t out);
void  VectorNormalizeFast (vec3_t v);

// just in case you do't want to use the macros
void _VectorMA (const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc);
vec_t _DotProduct (const vec3_t v1, const vec3_t v2);
void _VectorSubtract (const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorAdd (const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorCopy (const vec3_t in, vec3_t out);

void ClearBounds (vec3_t mins, vec3_t maxs);
void AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs);
float RadiusFromBounds (const vec3_t mins, const vec3_t maxs);
qboolean BoundsIntersect (const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsAndSphereIntersect (const vec3_t mins, const vec3_t maxs, const vec3_t centre, float radius);

#define NUMVERTEXNORMALS	162
int DirToByte (vec3_t dir);
void ByteToDir (int b, vec3_t dir);

void NormToLatLong ( const vec3_t normal, qbyte latlong[2] );

void MakeNormalVectors (const vec3_t forward, vec3_t right, vec3_t up);
void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, const struct cplane_s *plane);
float anglemod(float a);
float LerpAngle (float a1, float a2, const float frac);
void VecToAngles (const vec3_t vec, vec3_t angles);
void AnglesToAxis (const vec3_t angles, vec3_t axis[3]);
void NormalVectorToAxis( const vec3_t forward, vec3_t axis[3] );

vec_t ColorNormalize( const vec_t *in, vec_t *out );

float CalcFov( float fov_x, float width, float height );
void AdjustFov( float *fov_x, float *fov_y, float width, float height, qboolean lock_x );

#define Q_rint(x)	((x) < 0 ? ((int)((x)-0.5f)) : ((int)((x)+0.5f)))

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );

void Matrix_Identity( vec3_t m[3] );
void Matrix_Copy( vec3_t m1[3], vec3_t m2[3] );
qboolean Matrix_Compare( vec3_t m1[3], vec3_t m2[3] );
void Matrix_Multiply( vec3_t m1[3], vec3_t m2[3], vec3_t out[3] );
void Matrix_TransformVector( vec3_t m[3], vec3_t v, vec3_t out );
void Matrix_Transpose( vec3_t in[3], vec3_t out[3] );
void Matrix_EulerAngles( vec3_t m[3], vec3_t angles );
void Matrix_Rotate( vec3_t m[3], vec_t angle, vec_t x, vec_t y, vec_t z );
void Matrix_FromPoints( vec3_t v1, vec3_t v2, vec3_t v3, vec3_t m[3] );

void Quat_Identity( quat_t q );
void Quat_Copy( const quat_t q1, quat_t q2 );
qboolean Quat_Compare( const quat_t q1, const quat_t q2 );
void Quat_Conjugate( const quat_t q1, quat_t q2 );
vec_t Quat_Normalize( quat_t q );
vec_t Quat_Inverse( const quat_t q1, quat_t q2 );
void Quat_Multiply( const quat_t q1, const quat_t q2, quat_t out );
void Quat_Lerp( const quat_t q1, const quat_t q2, vec_t t, quat_t out );
void Quat_Vectors( const quat_t q, vec3_t f, vec3_t r, vec3_t u );
void Quat_Matrix( const quat_t q, vec3_t m[3] );
void Matrix_Quat( vec3_t m[3], quat_t q );
void Quat_TransformVector( const quat_t q, const vec3_t v, vec3_t out );
void Quat_ConcatTransforms( const quat_t q1, const vec3_t v1, const quat_t q2, const vec3_t v2, quat_t q, vec3_t v );

//=============================================

const char *COM_SkipPath( const char *pathname );
char *COM_StripExtension( const char *in, char *out );
const char *COM_FileExtension( const char *in );
char *COM_FileBase( const char *in, char *out );
char *COM_FilePath( const char *in, char *out );
char *COM_DefaultExtension( char *path, const char *extension );
char *COM_ReplaceExtension( char *path, const char *extension );

int COM_Compress( char *data_p );

// data is an in/out parm, returns a parsed out token
char *COM_ParseExt (const char **data_p, qboolean nl);
#define COM_Parse(data_p)	COM_ParseExt(data_p,qtrue)

//=============================================

extern	vec4_t		colorBlack;
extern	vec4_t		colorRed;
extern	vec4_t		colorGreen;
extern	vec4_t		colorBlue;
extern	vec4_t		colorYellow;
extern	vec4_t		colorMagenta;
extern	vec4_t		colorCyan;
extern	vec4_t		colorWhite;
extern	vec4_t		colorLtGrey;
extern	vec4_t		colorMdGrey;
extern	vec4_t		colorDkGrey;

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE )

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define ColorIndex(c)	( ( (c) - '0' ) & 7 )

#define	COLOR_R(rgba)		((rgba) & 0xFF)
#define	COLOR_G(rgba)		(((rgba) >> 8) & 0xFF)
#define	COLOR_B(rgba)		(((rgba) >> 16) & 0xFF)
#define	COLOR_A(rgba)		(((rgba) >> 24) & 0xFF)
#define COLOR_RGB(r,g,b)	(((r) << 0)|((g) << 8)|((b) << 16))
#define COLOR_RGBA(r,g,b,a) (((r) << 0)|((g) << 8)|((b) << 16)|((a) << 24))

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

extern vec4_t	color_table[8];

//=============================================

void Q_strncpyz( char *dest, const char *src, size_t size );
void Q_strncatz( char *dest, const char *src, size_t size );
void Q_snprintfz( char *dest, size_t size, const char *fmt, ... );
char *Q_strlwr( char *s );
qboolean Q_WildCmp( const char *pattern, const char *text );
qboolean Q_isdigit( const char *str );
const char *Q_strrstr( const char *s, const char *substr );

//=============================================
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
# if defined(__i386__) || defined(__ia64__) || defined(WIN32) || (defined(__alpha__) || defined(__alpha)) || defined(__arm__) || (defined(__mips__) && defined(__MIPSEL__)) || defined(__LITTLE_ENDIAN__) || defined(__x86_64__) || defined(_M_AMD64)
#  define ENDIAN_LITTLE
# else
#  define ENDIAN_BIG
# endif
#endif

short ShortSwap (short l);
int LongSwap (int l);
float FloatSwap (float f);

#ifdef ENDIAN_LITTLE
// little endian
# define BigShort(l) ShortSwap(l)
# define LittleShort(l) (l)
# define BigLong(l) LongSwap(l)
# define LittleLong(l) (l)
# define BigFloat(l) FloatSwap(l)
# define LittleFloat(l) (l)
#elif defined(ENDIAN_BIG)
// big endian
# define BigShort(l) (l)
# define LittleShort(l) ShortSwap(l)
# define BigLong(l) (l)
# define LittleLong(l) LongSwap(l)
# define BigFloat(l) (l)
# define LittleFloat(l) FloatSwap(l)
#else
// figure it out at runtime
extern short (*BigShort) (short l);
extern short (*LittleShort) (short l);
extern int (*BigLong) (int l);
extern int (*LittleLong) (int l);
extern float (*BigFloat) (float l);
extern float (*LittleFloat) (float l);
#endif

void	Swap_Init (void);

float	*tv (float x, float y, float z);
char	*vtos (vec3_t v);
char	*va(char *format, ...);

//=============================================

//
// key / value info strings
//
#define	MAX_INFO_KEY		64
#define	MAX_INFO_VALUE		64
#define	MAX_INFO_STRING		512

char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_SetValueForKey (char *s, char *key, char *value);
qboolean Info_Validate (char *s);

/*
==============================================================

SYSTEM SPECIFIC

==============================================================
*/

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error (char *error, ...);
void Com_Printf (char *msg, ...);


/*
==============================================================

FILESYSTEM

==============================================================
*/

#define FS_READ		0
#define FS_WRITE	1
#define FS_APPEND	2

#define FS_SEEK_CUR	0
#define FS_SEEK_SET	1
#define FS_SEEK_END	2

/*
==========================================================

CVARS (console variables)

==========================================================
*/

#ifndef CVAR
#define	CVAR

#define	CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define	CVAR_USERINFO	2	// added to userinfo  when changed
#define	CVAR_SERVERINFO	4	// added to serverinfo when changed
#define	CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define	CVAR_LATCH		16	// save changes until map restart
#define	CVAR_LATCH_VIDEO	32	// save changes until video restart
#define CVAR_CHEAT		64	// will be reset to default unless cheats are enabled

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	char		*name;
	char		*string;
	char		*dvalue;
	char		*latched_string;	// for CVAR_LATCH vars
	int			flags;
	qboolean	modified;	// set each time the cvar is changed
	float		value;
	int			integer;
	struct cvar_s *next;
	struct cvar_s *hash_next;
} cvar_t;

#endif		// CVAR

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)



#define	SURF_NODAMAGE			0x1		// never give falling damage
#define	SURF_SLICK				0x2		// effects game physics
#define	SURF_SKY				0x4		// lighting from environment map
#define	SURF_LADDER				0x8
#define	SURF_NOIMPACT			0x10	// don't make missile explosions
#define	SURF_NOMARKS			0x20	// don't leave missile marks
#define	SURF_FLESH				0x40	// make flesh sounds and effects
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	SURF_HINT				0x100	// make a primary bsp splitter
#define	SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	SURF_METALSTEPS			0x1000	// clanking footsteps
#define	SURF_NOSTEPS			0x2000	// no footstep sounds
#define	SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	SURF_NODLIGHT			0x20000	// never add dynamic lights
#define SURF_DUST				0x40000 // leave a dust trail when walking on this surface



// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP)
#define	MASK_MONSTERSOLID		(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BODY)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE)


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define	AREA_SOLID		1
#define	AREA_TRIGGERS	2

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3 is not axial
#define	PLANE_NONAXIAL	3

// cplane_t structure
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	short	type;			// for fast side tests
	short	signbits;		// signx + (signy<<1) + (signz<<1)
} cplane_t;

int SignbitsForPlane ( const cplane_t *out );
int PlaneTypeForNormal ( const vec3_t normal );
void CategorizePlane ( cplane_t *plane );
void PlaneFromPoints ( vec3_t verts[3], cplane_t *plane );

qboolean ComparePlanes ( const vec3_t p1normal, vec_t p1dist, const vec3_t p2normal, vec_t p2dist );
void SnapVector ( vec3_t normal );
void SnapPlane ( vec3_t normal, vec_t *dist );

// a trace is returned when a box is swept through the world
typedef struct
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	int			surfFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			ent;		// not set by CM_*() functions
} trace_t;


// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct
{
	int			pm_type;

	int			origin[3];		// 12.3
	int			velocity[3];	// 12.3
	int			pm_flags;		// ducked, jump_held, etc
	int			pm_time;		// each unit = 8 ms
	int			gravity;
	short		delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters
} pmove_state_t;


//
// button bits
//
#define	BUTTON_ATTACK		1
#define	BUTTON_USE			2
#define	BUTTON_ANY			128			// any key whatsoever


// usercmd_t is sent to the server each client frame
typedef struct usercmd_s
{
	qbyte	msec;
	qbyte	buttons;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
} usercmd_t;


#define	MAXTOUCH	32
typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t		cmd;
	qboolean		snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	int			touchents[MAXTOUCH];
	qboolean	step;				// true if walked up a step

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	int			groundentity;
	int			watertype;
	int			waterlevel;
	float		airaccelerate;

	// callbacks to test the world
	void		(*trace) (trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int			(*pointcontents) (vec3_t point);
} pmove_t;


// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define	EF_ROTATE_AND_BOB	1		// rotate and bob (bonus items)
#define EF_POWERSCREEN		2
#define EF_CORPSE			4		// treat as CONTENTS_CORPSE for collision (for clients)
#define	EF_QUAD				8
#define	EF_PENT				16
#define EF_FLAG1			32
#define EF_FLAG2			64

// entity_state_t->renderfx flags
#define	RF_MINLIGHT			1		// always have some light (viewmodel)
#define	RF_FULLBRIGHT		2		// always draw full intensity
#define	RF_FRAMELERP		4
#define RF_SHELL_RED		8
#define	RF_SHELL_GREEN		16
#define RF_SHELL_BLUE		32
#define RF_NOSHADOW			64
#define RF_SCALEHACK		128		// scale 1.5 times (weapons)

// these flags are set locally by client
#define	RF_VIEWERMODEL		256		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		512		// only draw through eyes and depth hack
#define RF_CULLHACK			1024
#define RF_FORCENOLOD		2048
#define RF_PLANARSHADOW		4096
#define RF_OCCLUSIONTEST	8192

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4
// modifier flags
#define	CHAN_NO_PHS_ADD			8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE			16	// send by reliable message, not datagram

// sound attenuation values
#define	ATTN_NONE               0	// full volume the entire level
#define	ATTN_NORM               1
#define	ATTN_IDLE               2
#define	ATTN_STATIC             3	// diminish very rapidly with distance

typedef enum
{
	key_game, 
	key_console, 
	key_message, 
	key_menu
} keydest_t;

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

// note that Q_rint was causing problems here
// (spawn looking straight up\down at delta_angles wrapping)

#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	ANGLE2BYTE(x)	((int)((x)*256/360) & 255)
#define	BYTE2ANGLE(x)	((x)*(360.0/256))

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define	CS_MESSAGE			0
#define	CS_MAPNAME			1
#define	CS_AUDIOTRACK		2
#define	CS_STATUSBAR		3		// display program string
#define	CS_GAMETYPE			4
#define CS_SKYBOX			5		// sky portal origin

#define CS_AIRACCEL			29		// air acceleration control
#define	CS_MAXCLIENTS		30
#define	CS_MAPCHECKSUM		31		// for catching cheater maps

#define	CS_MODELS			32
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)
#define	CS_ITEMS			(CS_LIGHTS+MAX_LIGHTSTYLES)
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)
#define CS_GENERAL			(CS_PLAYERSKINS+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS	(CS_GENERAL+MAX_GENERAL)


//==============================================


// entity_state_t->event values
// entity events are for effects that take place reletive
// to an existing entities origin.  Very network efficient.

#define EV_INVERSE		128

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
#define SOLID_BMODEL	31	// special value for bmodel

#define ET_INVERSE		128

typedef struct entity_state_s
{
	int				number;			// edict index

	int				type;			// ET_GENERIC, ET_BEAM, etc

	vec3_t			origin;
	vec3_t			angles;

	union
	{
		vec3_t		old_origin;		// for lerping
		vec3_t		origin2;		// ET_BEAM, ET_PORTALSURFACE, ET_EVENT specific
	};

	int				modelindex;
	int				modelindex2;
	int				modelindex3;	// weapons, CTF flags, etc

	union
	{
		int			frame;
		int			ownerNum;		// ET_EVENT specific
	};

	union
	{
		int			skinnum;
		int			targetNum;		// ET_EVENT specific
		int			colorRGBA;		// ET_BEAM, ET_EVENT specific
	};

	int				weapon;			// WEAP_ for players, MZ2_ for monsters
	unsigned int	effects;

	union
	{
		int			renderfx;
		int			eventCount;		// ET_EVENT specific
	};

	int				solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
									// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
									// trap_LinkEntity sets this properly

	int				sound;			// for looping sounds, to guarantee shutoff

	int				events[2];		// impulse events -- muzzle flashes, footsteps, etc
									// events only go out for a single frame, they
									// are automatically cleared each frame
	int				eventParms[2];

	int				light;			// constant light glow
} entity_state_t;

//==============================================

typedef enum
{
	ca_uninitialized,
	ca_disconnected, 	// not talking to a server
	ca_connecting,		// sending request packets to the server
	ca_connected,		// netchan_t established, waiting for svc_serverdata
	ca_loading,
	ca_active			// game views should be displayed
} connstate_t;

//==============================================

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
#define	MAX_STATS	32

typedef struct
{
	pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t		viewangles;		// for fixed views
	vec3_t		viewoffset;		// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	float		blend[4];		// rgba full screen effect

	float		fov;			// horizontal field of view

	short		stats[MAX_STATS];		// fast status bar updates
} player_state_t;
