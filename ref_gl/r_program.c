/*
Copyright (C) 2002-2007 Victor Luchits

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
#include "r_local.h"

// r_program.c - OpenGL Shading Language support (cheap and dirty)

#define MAX_GLSL_PROGRAMS			1024

typedef struct
{
	int					bit;
	const char			*define;
	const char			*suffix;
} glsl_feature_t;

typedef struct
{
	char				name[MAX_QPATH];

	int					object;
	int					vertexShader;
	int					fragmentShader;

	int					locEyeOrigin,
						locLightDir,
						locLightOrigin,
						locLightAmbient,
						locLightDiffuse,

						locGlossIntensity,
						locGlossExponent,

						locDeluxemapOffset[MAX_LIGHTMAPS],
						loclsColor[MAX_LIGHTMAPS]
	;
} glsl_program_t;

glsl_program_t r_glslprograms[MAX_GLSL_PROGRAMS];

static void R_GetProgramUniformLocations( glsl_program_t *program );

/*
================
R_InitGLSLPrograms
================
*/
void R_InitGLSLPrograms( void ) 
{
	memset( r_glslprograms, 0, sizeof( r_glslprograms ) );

	if( !glConfig.GLSL )
		return;

	// register programs that are most likely to be used
	R_RegisterGLSLProgram( DEFAULT_GLSL_PROGRAM, NULL, PROGRAM_APPLY_FB_LIGHTMAP|PROGRAM_APPLY_LIGHTSTYLE0 );
	R_RegisterGLSLProgram( DEFAULT_GLSL_PROGRAM, NULL, PROGRAM_APPLY_FB_LIGHTMAP|PROGRAM_APPLY_LIGHTSTYLE0|PROGRAM_APPLY_SPECULAR );
	R_RegisterGLSLProgram( DEFAULT_GLSL_PROGRAM, NULL, PROGRAM_APPLY_DIRECTIONAL_LIGHT );
	R_RegisterGLSLProgram( DEFAULT_GLSL_PROGRAM, NULL, PROGRAM_APPLY_DIRECTIONAL_LIGHT|PROGRAM_APPLY_SPECULAR );
}

/*
================
R_DeleteGLSLProgram
================
*/
static void R_DeleteGLSLProgram( glsl_program_t *program )
{
	if( program->vertexShader ) {
		qglDetachObjectARB( program->object, program->vertexShader );
		qglDeleteObjectARB( program->vertexShader );
		program->vertexShader = 0;
	}
	if( program->fragmentShader ) {
		qglDetachObjectARB( program->object, program->fragmentShader );
		qglDeleteObjectARB( program->fragmentShader );
		program->fragmentShader = 0;
	}
	if( program->object )
		qglDeleteObjectARB( program->object );
	program->object = 0;
	program->name[0] = '\0';
}

/*
================
R_CompileGLSLShader
================
*/
static int R_CompileGLSLShader( int program, const char *programName, const char *shaderName, int shaderType, const char **strings, int numStrings )
{
	int shader, compiled;

	shader = qglCreateShaderObjectARB( (GLenum)shaderType );
	if( !shader )
		return 0;

	// if lengths is NULL, then each string is assumed to be null-terminated
	qglShaderSourceARB( shader, numStrings, strings, NULL );
	qglCompileShaderARB( shader );
	qglGetObjectParameterivARB( shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled );

	if( !compiled ) {
		char log[4096];

		log[4095] = 0;
		qglGetInfoLogARB( shader, sizeof( log ) - 1, NULL, log );
		if( log[0] )
			Com_Printf( S_COLOR_YELLOW "Failed to compile %s shader for program %s:\n%s\n", shaderName, programName, log );

		qglDeleteObjectARB( shader );
		return 0;
	}

	qglAttachObjectARB( program, shader );

	return shader;
}

/*
================
R_FindGLSLProgram
================
*/
int R_FindGLSLProgram( const char *name )
{
	int i;
	glsl_program_t *program;

	for( i = 0, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ ) {
		if( !program->name[0] )
			break;
		if( !Q_stricmp( program->name, name ) )
			return (i+1);
	}

	return 0;
}

#define MAX_DEFINES_FEATURES	255

static const glsl_feature_t glsl_features[] = 
{
	{ PROGRAM_APPLY_LIGHTSTYLE0, "#define APPLY_LIGHTSTYLE0\n", "_ls0" },
	{ PROGRAM_APPLY_FB_LIGHTMAP, "#define APPLY_FBLIGHTMAP\n",	"_fb" },
	{ PROGRAM_APPLY_LIGHTSTYLE1, "#define APPLY_LIGHTSTYLE1\n", "_ls1" },
	{ PROGRAM_APPLY_LIGHTSTYLE2, "#define APPLY_LIGHTSTYLE2\n", "_ls2" },
	{ PROGRAM_APPLY_LIGHTSTYLE3, "#define APPLY_LIGHTSTYLE3\n", "_ls3" },
	{ PROGRAM_APPLY_DIRECTIONAL_LIGHT, "#define APPLY_DIRECTIONAL_LIGHT\n", "_dirlight" },
	{ PROGRAM_APPLY_SPECULAR, "#define APPLY_SPECULAR\n", "_gloss" }
};

static const char *r_defaultGLSLProgram =
"\n"
"// " APPLICATION " GLSL shader"
"\n"
"varying vec2 TexCoord;\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"varying vec4 LightmapTexCoord01;\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"varying vec4 LightmapTexCoord23;\n"
"#endif\n"
"#endif\n"
"\n"
"#ifdef APPLY_SPECULAR\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"varying vec3 LightVector;\n"
"#endif\n"
"\n"
"varying mat3 strMatrix; // directions of S/T/R texcoords (tangent, binormal, normal)\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"// Vertex shader\n"
"\n"
"uniform vec3 EyeOrigin;\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"uniform vec3 LightDir;\n"
"#endif\n"
"\n"
"void main()\n"
"{\n"
"gl_FrontColor = gl_Color;\n"
"\n"
"TexCoord = vec2 (gl_TextureMatrix[0] * gl_MultiTexCoord0);\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"LightmapTexCoord01.st = gl_MultiTexCoord3.st;\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"LightmapTexCoord01.pq = gl_MultiTexCoord4.st;\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"LightmapTexCoord23.st = gl_MultiTexCoord5.st;\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"LightmapTexCoord23.pq = gl_MultiTexCoord6.st;\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"strMatrix[0] = gl_MultiTexCoord1.xyz;\n"
"strMatrix[2] = gl_Normal.xyz;\n"
"strMatrix[1] = gl_MultiTexCoord1.w * cross (strMatrix[2], strMatrix[0]);\n"
"\n"
"#ifdef APPLY_SPECULAR\n"
"vec3 EyeVectorWorld;\n"
"EyeVectorWorld = EyeOrigin - gl_Vertex.xyz;\n"
"\n"
"EyeVector = EyeVectorWorld * strMatrix;\n"
"#endif\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"LightVector = LightDir * strMatrix;\n"
"#endif\n"
"\n"
"gl_Position = ftransform ();\n"
"}\n"
"\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"// Fragment shader\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"uniform sampler2D LightmapTexture0;\n"
"uniform float DeluxemapOffset0; // s-offset for LightmapTexCoord\n"
"uniform vec3 lsColor0; // lightstyle color\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"uniform sampler2D LightmapTexture1;\n"
"uniform float DeluxemapOffset1;\n"
"uniform vec3 lsColor1;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"uniform sampler2D LightmapTexture2;\n"
"uniform float DeluxemapOffset2;\n"
"uniform vec3 lsColor2;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"uniform sampler2D LightmapTexture3;\n"
"uniform float DeluxemapOffset3;\n"
"uniform vec3 lsColor3;\n"
"\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"uniform sampler2D BaseTexture;\n"
"uniform sampler2D NormalmapTexture;\n"
"uniform sampler2D GlossTexture;\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"uniform vec3 LightAmbient;\n"
"uniform vec3 LightDiffuse;\n"
"#endif\n"
"\n"
"uniform float GlossIntensity; // gloss scaling factor\n"
"uniform float GlossExponent; // gloss exponent factor\n"
"\n"
"void main()\n"
"{\n"
"vec3 surfaceNormal;\n"
"vec3 diffuseNormalModelspace;\n"
"vec3 diffuseNormal = vec3 (0.0, 0.0, -1.0);\n"
"float diffuseProduct;\n"
"\n"
"vec3 weightedDiffuseNormal;\n"
"vec3 specularNormal;\n"
"float specularProduct;\n"
"\n"
"vec4 color = vec4 (0.0, 0.0, 0.0, 1.0);\n"
"\n"
"// get the surface normal\n"
"surfaceNormal = normalize (vec3 (texture2D (NormalmapTexture, TexCoord)) - vec3 (0.5));\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"diffuseNormal = vec3 (LightVector);\n"
"weightedDiffuseNormal = diffuseNormal;\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"color.rgb += LightDiffuse.rgb * max (diffuseProduct, 0.0) + LightAmbient.rgb;\n"
"#endif\n"
"\n"
"// deluxemapping using light vectors in modelspace\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"\n"
"// get light normal\n"
"diffuseNormalModelspace = vec3 (texture2D (LightmapTexture0, vec2(LightmapTexCoord01.s+DeluxemapOffset0,LightmapTexCoord01.t))) - vec3 (0.5);\n"
"diffuseNormal = normalize (diffuseNormalModelspace * strMatrix);\n"
"// calculate directional shading\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"\n"
"#ifdef APPLY_FBLIGHTMAP\n"
"weightedDiffuseNormal = diffuseNormal;\n"
"// apply lightmap color\n"
"color.rgb += max (diffuseProduct, 0.0) * texture2D (LightmapTexture0, LightmapTexCoord01.st).rgb;\n"
"#else\n"
"\n"
"#define NORMALIZE_DIFFUSE_NORMAL\n"
"\n"
"weightedDiffuseNormal = lsColor0.rgb * diffuseNormal;\n"
"// apply lightmap color\n"
"color.rgb += lsColor0.rgb * max (diffuseProduct, 0.0) * texture2D (LightmapTexture0, LightmapTexCoord01.st).rgb;\n"
"#endif\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"diffuseNormalModelspace = vec3 (texture2D (LightmapTexture1, vec2(LightmapTexCoord01.p+DeluxemapOffset1,LightmapTexCoord01.q))) - vec3 (0.5);\n"
"diffuseNormal = normalize (diffuseNormalModelspace * strMatrix);\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor1.rgb * diffuseNormal;\n"
"color.rgb += lsColor1.rgb * max (diffuseProduct, 0.0) * texture2D (LightmapTexture1, LightmapTexCoord01.pq).rgb;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"diffuseNormalModelspace = vec3 (texture2D (LightmapTexture2, vec2(LightmapTexCoord23.s+DeluxemapOffset2,LightmapTexCoord23.t))) - vec3 (0.5);\n"
"diffuseNormal = normalize (diffuseNormalModelspace * strMatrix);\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor2.rgb * diffuseNormal;\n"
"color.rgb += lsColor2.rgb * max (diffuseProduct, 0.0) * texture2D (LightmapTexture2, LightmapTexCoord23.st).rgb;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"diffuseNormalModelspace = vec3 (texture2D (LightmapTexture3, vec2(LightmapTexCoord23.p+DeluxemapOffset3,LightmapTexCoord23.q))) - vec3 (0.5);\n"
"diffuseNormal = normalize (diffuseNormalModelspace * strMatrix);\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor3.rgb * diffuseNormal;\n"
"color.rgb += lsColor3.rgb * max (diffuseProduct, 0.0) * texture2D (LightmapTexture3, LightmapTexCoord23.pq).rgb;\n"
"\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"#ifdef APPLY_SPECULAR\n"
"\n"
"#ifdef NORMALIZE_DIFFUSE_NORMAL\n"
"specularNormal = vec3 (normalize (vec3 (normalize (weightedDiffuseNormal)) + vec3 (normalize (EyeVector))));\n"
"#else\n"
"specularNormal = vec3 (normalize (weightedDiffuseNormal + vec3 (normalize (EyeVector))));\n"
"#endif\n"
"\n"
"specularProduct = float (dot (surfaceNormal, specularNormal));\n"
"color.rgb += (vec3(texture2D(GlossTexture, TexCoord)) * GlossIntensity) * float(pow(float(max(specularProduct, 0.0)), GlossExponent));\n"
"#endif\n"
"\n"
"color.rgb *= vec3(texture2D(BaseTexture, TexCoord)).rgb;\n"
"\n"
"gl_FragColor = vec4 (color) * vec4 (gl_Color);\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
"\n";

/*
================
R_ProgramFeatures2Defines

Return an array of strings for bitflags
================
*/
static const char **R_ProgramFeatures2Defines( int features, char *name, size_t size )
{
	int i, p;
	static const char *headers[MAX_DEFINES_FEATURES+1];	// +1 for NULL safe-guard

	for( i = 0, p = 0; i < sizeof( glsl_features ) / sizeof( glsl_features[0] ); i++ ) {
		if( features & glsl_features[i].bit ) {
			headers[p++] = glsl_features[i].define;
			if( name )
				Q_strncatz( name, glsl_features[i].suffix, size );

			if( p == MAX_DEFINES_FEATURES )
				break;
		}
	}

	if( p ) {
		headers[p] = NULL;
		return headers;
	}

	return NULL;
}

/*
================
R_RegisterGLSLProgram
================
*/
int R_RegisterGLSLProgram( const char *name, const char *string, int features )
{
	int i, f;
	int linked, body;
	glsl_program_t *program;
	char newName[MAX_QPATH];
	const char **header;
	const char *vertexShaderStrings[MAX_DEFINES_FEATURES+2];
	const char *fragmentShaderStrings[MAX_DEFINES_FEATURES+2];

	if( !glConfig.GLSL )
		return 0;	// fail early

	Q_strncpyz( newName, name, sizeof( newName ) );
	header = R_ProgramFeatures2Defines( features, newName, sizeof( newName ) );

	for( i = 0, f = -1, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ ) {
		if( !Q_stricmp( program->name, newName ) )
			return (i+1);
		if( (f == -1) && !program->object )
			f = i;
	}

	if( f == -1 ) {
		Com_Printf( S_COLOR_YELLOW "R_RegisterGLSLProgram: GLSL programs limit exceeded\n");
		return 0;
	}

	if( !string )
		string = r_defaultGLSLProgram;

	program = r_glslprograms + f;
	program->object = qglCreateProgramObjectARB ();
	if( !program->object )
		goto cleanup;

	body = 1;
	if( header )
		for( ; header[body-1] && *header[body-1]; body++ );

	vertexShaderStrings[0] = "#define VERTEX_SHADER\n";
	for( i = 1; i < body; i++ )
		vertexShaderStrings[i] = ( char * )header[i-1];
	// find vertex shader header
	vertexShaderStrings[body] = string;

	fragmentShaderStrings[0] = "#define FRAGMENT_SHADER\n";
	for( i = 1; i < body; i++ )
		fragmentShaderStrings[i] = ( char * )header[i-1];
	// find fragment shader header
	fragmentShaderStrings[body] = string;

	// compile vertex shader
	program->vertexShader = R_CompileGLSLShader( program->object, newName, "vertex", GL_VERTEX_SHADER_ARB, vertexShaderStrings, body+1 );
	if( !program->vertexShader )
		goto cleanup;

	// compile fragment shader
	program->fragmentShader = R_CompileGLSLShader( program->object, newName, "fragment", GL_FRAGMENT_SHADER_ARB, fragmentShaderStrings, body+1 );
	if( !program->fragmentShader )
		goto cleanup;

	// link
	qglLinkProgramARB( program->object );
	qglGetObjectParameterivARB( program->object, GL_OBJECT_LINK_STATUS_ARB, &linked );
	if( !linked ) {
		char log[8192];

		qglGetInfoLogARB( program->object, sizeof( log ), NULL, log );
		if( log[0] )
			Com_Printf( S_COLOR_YELLOW "Failed to compile link object for program %s:\n%s\n", newName, log );

		goto cleanup;
	}

	Q_strncpyz( program->name, newName, sizeof( program->name ) );

	qglUseProgramObjectARB( program->object );

	R_GetProgramUniformLocations( program );

	qglUseProgramObjectARB( 0 );

	return f+1;

cleanup:
	R_DeleteGLSLProgram( program );

	return 0;
}

/*
================
R_GetProgramObject
================
*/
int R_GetProgramObject( int index ) {
	return r_glslprograms[index - 1].object;
}

/*
================
R_ProgramList_f
================
*/
void R_ProgramList_f( void )
{
	int i;
	glsl_program_t *program;

	Com_Printf( "------------------\n" );
	for( i = 0, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ ) {
		if( !program->name[0] )
			break;
		Com_Printf( " %3i %s\n", i+1, program->name );
	}
	Com_Printf( "%i programs total\n", i );
}

/*
================
R_UpdateProgramUniforms
================
*/
void R_UpdateProgramUniforms( int index, vec3_t eyeOrigin, 
							 vec3_t lightOrigin, vec3_t lightDir, vec4_t ambient, vec4_t diffuse, 
							 superLightStyle_t *superLightStyle )
{
	glsl_program_t *program = r_glslprograms + index - 1;

	if( program->locEyeOrigin >= 0 && eyeOrigin )
		qglUniform3fARB( program->locEyeOrigin, eyeOrigin[0], eyeOrigin[1], eyeOrigin[2] );

	if( program->locLightOrigin >= 0 && lightOrigin )
		qglUniform3fARB( program->locLightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2] );
	if( program->locLightDir >= 0 && lightDir )
		qglUniform3fARB( program->locLightDir, lightDir[0], lightDir[1], lightDir[2] );

	if( program->locLightAmbient >= 0 && ambient )
		qglUniform3fARB( program->locLightAmbient, ambient[0], ambient[1], ambient[2] );
	if( program->locLightDiffuse >= 0 && diffuse )
		qglUniform3fARB( program->locLightDiffuse, diffuse[0], diffuse[1], diffuse[2] );

	if( program->locGlossIntensity >= 0 )
		qglUniform1fARB( program->locGlossIntensity, r_lighting_glossintensity->value );
	if( program->locGlossExponent >= 0 )
		qglUniform1fARB( program->locGlossExponent, r_lighting_glossexponent->value );

	if( superLightStyle ) {
		int i;

		for( i = 0; i < MAX_LIGHTMAPS && superLightStyle->lightmapStyles[i] != 255; i++ ) {
			vec_t *rgb = r_lightStyles[superLightStyle->lightmapStyles[i]].rgb;

			if( program->locDeluxemapOffset[i] >= 0 )
				qglUniform1fARB( program->locDeluxemapOffset[i], superLightStyle->stOffset[i][0] );
			if( program->loclsColor[i] >= 0 )
				qglUniform3fARB( program->loclsColor[i], rgb[0], rgb[1], rgb[2] );
		}

		for( ; i < MAX_LIGHTMAPS; i++ ) {
			if( program->loclsColor[i] >= 0 )
				qglUniform3fARB( program->loclsColor[i], 0, 0, 0 );
		}
	}
}

/*
================
R_GetProgramUniformLocations
================
*/
static void R_GetProgramUniformLocations( glsl_program_t *program )
{
	int i;
	int	locBaseTexture,
		locNormalmapTexture,
		locGlossTexture,
		locLightmapTexture[MAX_LIGHTMAPS];

	program->locEyeOrigin = qglGetUniformLocationARB( program->object, "EyeOrigin" );
	program->locLightDir = qglGetUniformLocationARB( program->object, "LightDir" );
	program->locLightOrigin = qglGetUniformLocationARB( program->object, "LightOrigin" );
	program->locLightAmbient = qglGetUniformLocationARB( program->object, "LightAmbient" );
	program->locLightDiffuse = qglGetUniformLocationARB( program->object, "LightDiffuse" );

	locBaseTexture = qglGetUniformLocationARB( program->object, "BaseTexture" );
	locNormalmapTexture = qglGetUniformLocationARB( program->object, "NormalmapTexture" );
	locGlossTexture = qglGetUniformLocationARB( program->object, "GlossTexture" );

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		locLightmapTexture[i] = qglGetUniformLocationARB( program->object, va( "LightmapTexture%i", i ) );

		program->locDeluxemapOffset[i] = qglGetUniformLocationARB( program->object, va( "DeluxemapOffset%i", i ) );
		program->loclsColor[i] = qglGetUniformLocationARB( program->object, va( "lsColor%i", i ) );
	}

	program->locGlossIntensity = qglGetUniformLocationARB( program->object, "GlossIntensity" );
	program->locGlossExponent = qglGetUniformLocationARB( program->object, "GlossExponent" );

	if( locBaseTexture >= 0 )
		qglUniform1iARB( locBaseTexture, 0 );
	if( locNormalmapTexture >= 0 )
		qglUniform1iARB( locNormalmapTexture, 1 );
	if( locGlossTexture >= 0 )
		qglUniform1iARB( locGlossTexture, 2 );

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		if( locLightmapTexture[i] >= 0 )
			qglUniform1iARB( locLightmapTexture[i], i+3 );
	}
}

/*
================
R_ShutdownGLSLPrograms
================
*/
void R_ShutdownGLSLPrograms( void )
{
	int i;
	glsl_program_t *program;

	if( !glConfig.GLSL )
		return;

	for( i = 0, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ ) {
		if( program->object )
			R_DeleteGLSLProgram( program );
	}
}
