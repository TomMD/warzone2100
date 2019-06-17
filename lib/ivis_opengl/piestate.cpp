/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2019  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** \file
 *  Renderer setup and state control routines for 3D rendering.
 */
#include "lib/framework/frame.h"
#include "lib/framework/opengl.h"

#include <physfs.h>
#include "lib/framework/physfs_ext.h"

#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/piestate.h"
#include "lib/ivis_opengl/piedef.h"
#include "lib/ivis_opengl/tex.h"
#include "lib/ivis_opengl/piepalette.h"
#include "screen.h"
#include "pieclip.h"
#include <glm/gtc/type_ptr.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

#ifndef GLEW_VERSION_4_3
#define GLEW_VERSION_4_3 false
#endif
#ifndef GLEW_KHR_debug
#define GLEW_KHR_debug false
#endif

#include <algorithm>

/*
 *	Global Variables
 */

std::vector<pie_internal::SHADER_PROGRAM> pie_internal::shaderProgram;
static gfx_api::gfxFloat shaderStretch = 0;
gfx_api::buffer* pie_internal::rectBuffer = nullptr;
static RENDER_STATE rendStates;
static int32_t ecmState = 0;
static gfx_api::gfxFloat timeState = 0.0f;

void rendStatesRendModeHack()
{
	rendStates.rendMode = REND_ALPHA;
}

/*
 *	Source
 */

void pie_SetDefaultStates()//Sets all states
{
	PIELIGHT black;

	//fog off
	rendStates.fogEnabled = false;// enable fog before renderer
	rendStates.fog = false;//to force reset to false
	pie_SetFogStatus(false);
	black.rgba = 0;
	black.byte.a = 255;
	pie_SetFogColour(black);

	rendStates.rendMode = REND_ALPHA;	// to force reset to REND_OPAQUE
}

//***************************************************************************
//
// pie_EnableFog(bool val)
//
// Global enable/disable fog to allow fog to be turned of ingame
//
//***************************************************************************
void pie_EnableFog(bool val)
{
	if (rendStates.fogEnabled != val)
	{
		debug(LOG_FOG, "pie_EnableFog: Setting fog to %s", val ? "ON" : "OFF");
		rendStates.fogEnabled = val;
		if (val)
		{
			pie_SetFogColour(WZCOL_FOG);
		}
		else
		{
			pie_SetFogColour(WZCOL_BLACK); // clear background to black
		}
	}
}

bool pie_GetFogEnabled()
{
	return rendStates.fogEnabled;
}

bool pie_GetFogStatus()
{
	return rendStates.fog;
}

void pie_SetFogColour(PIELIGHT colour)
{
	rendStates.fogColour = colour;
}

PIELIGHT pie_GetFogColour()
{
	return rendStates.fogColour;
}

// Read shader into text buffer
static char *readShaderBuf(const char *name)
{
	PHYSFS_file	*fp;
	int	filesize;
	char *buffer;

	fp = PHYSFS_openRead(name);
	debug(LOG_3D, "Reading...[directory: %s] %s", PHYSFS_getRealDir(name), name);
	ASSERT_OR_RETURN(nullptr, fp != nullptr, "Could not open %s", name);
	filesize = PHYSFS_fileLength(fp);
	buffer = (char *)malloc(filesize + 1);
	if (buffer)
	{
		WZ_PHYSFS_readBytes(fp, buffer, filesize);
		buffer[filesize] = '\0';
	}
	PHYSFS_close(fp);

	return buffer;
}

// Retrieve shader compilation errors
static void printShaderInfoLog(code_part part, GLuint shader)
{
	GLint infologLen = 0;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLen);
	if (infologLen > 0)
	{
		GLint charsWritten = 0;
		GLchar *infoLog = (GLchar *)malloc(infologLen);

		glGetShaderInfoLog(shader, infologLen, &charsWritten, infoLog);
		debug(part, "Shader info log: %s", infoLog);
		free(infoLog);
	}
}

// Retrieve shader linkage errors
static void printProgramInfoLog(code_part part, GLuint program)
{
	GLint infologLen = 0;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infologLen);
	if (infologLen > 0)
	{
		GLint charsWritten = 0;
		GLchar *infoLog = (GLchar *)malloc(infologLen);

		glGetProgramInfoLog(program, infologLen, &charsWritten, infoLog);
		debug(part, "Program info log: %s", infoLog);
		free(infoLog);
	}
}

static void getLocs(pie_internal::SHADER_PROGRAM *program)
{
	glUseProgram(program->program);

	// Attributes
	program->locVertex = glGetAttribLocation(program->program, "vertex");
	program->locNormal = glGetAttribLocation(program->program, "vertexNormal");
	program->locTexCoord = glGetAttribLocation(program->program, "vertexTexCoord");
	program->locColor = glGetAttribLocation(program->program, "vertexColor");

	// Uniforms, these never change.
	GLint locTex0 = glGetUniformLocation(program->program, "Texture");
	GLint locTex1 = glGetUniformLocation(program->program, "TextureTcmask");
	GLint locTex2 = glGetUniformLocation(program->program, "TextureNormal");
	GLint locTex3 = glGetUniformLocation(program->program, "TextureSpecular");
	glUniform1i(locTex0, 0);
	glUniform1i(locTex1, 1);
	glUniform1i(locTex2, 2);
	glUniform1i(locTex3, 3);
}

void pie_FreeShaders()
{
	while (pie_internal::shaderProgram.size() > SHADER_MAX)
	{
		glDeleteShader(pie_internal::shaderProgram.back().program);
		pie_internal::shaderProgram.pop_back();
	}
}

GLint wz_GetGLIntegerv(GLenum pname, GLint defaultValue = 0)
{
	GLint retVal = defaultValue;
	while(glGetError() != GL_NO_ERROR) { } // clear the OpenGL error queue
	glGetIntegerv(pname, &retVal);
	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
	{
		retVal = defaultValue;
	}
	return retVal;
}

SHADER_VERSION getMinimumShaderVersionForCurrentGLContext()
{
	// Determine the shader version directive we should use by examining the current OpenGL context
	GLint gl_majorversion = wz_GetGLIntegerv(GL_MAJOR_VERSION, 0);
	GLint gl_minorversion = wz_GetGLIntegerv(GL_MINOR_VERSION, 0);

	SHADER_VERSION version = VERSION_120; // for OpenGL < 3.2, we default to VERSION_120 shaders
	if ((gl_majorversion > 3) || ((gl_majorversion == 3) && (gl_minorversion >= 2)))
	{
		// OpenGL 3.2+
		// Since WZ only supports Core contexts with OpenGL 3.2+, we cannot use the version_120 directive
		// (which only works in compatbility contexts). Instead, use GLSL version "150 core".
		version = VERSION_150_CORE;
	}
	return version;
}

SHADER_VERSION getMaximumShaderVersionForCurrentGLContext()
{
	// Instead of querying the GL_SHADING_LANGUAGE_VERSION string and trying to parse it,
	// which is rife with difficulties because drivers can report very different strings (and formats),
	// use the known (and explicit) mapping table between OpenGL version and supported GLSL version.

	GLint gl_majorversion = wz_GetGLIntegerv(GL_MAJOR_VERSION, 0);
	GLint gl_minorversion = wz_GetGLIntegerv(GL_MINOR_VERSION, 0);

	// For OpenGL < 3.2, default to VERSION_120 shaders
	SHADER_VERSION version = VERSION_120;
	if(gl_majorversion == 3)
	{
		switch(gl_minorversion)
		{
			case 0: // 3.0 => 1.30
				version = VERSION_130;
				break;
			case 1: // 3.1 => 1.40
				version = VERSION_140;
				break;
			case 2: // 3.2 => 1.50
				version = VERSION_150_CORE;
				break;
			case 3: // 3.3 => 3.30
				version = VERSION_330_CORE;
				break;
			default:
				// Return the 3.3 value
				version = VERSION_330_CORE;
				break;
		}
	}
	else if (gl_majorversion == 4)
	{
		switch(gl_minorversion)
		{
			case 0: // 4.0 => 4.00
				version = VERSION_400_CORE;
				break;
			case 1: // 4.1 => 4.10
				version = VERSION_410_CORE;
				break;
			default:
				// Return the 4.1 value
				// NOTE: Nothing above OpenGL 4.1 is supported on macOS
				version = VERSION_410_CORE;
				break;
		}
	}
	else if (gl_majorversion > 4)
	{
		// Return the OpenGL 4.1 value (for now)
		version = VERSION_410_CORE;
	}
	return version;
}

const char * shaderVersionString(SHADER_VERSION version)
{
	switch(version)
	{
		case VERSION_120:
			return "#version 120\n";
		case VERSION_130:
			return "#version 130\n";
		case VERSION_140:
			return "#version 140\n";
		case VERSION_150_CORE:
			return "#version 150 core\n";
		case VERSION_330_CORE:
			return "#version 330 core\n";
		case VERSION_400_CORE:
			return "#version 400 core\n";
		case VERSION_410_CORE:
			return "#version 410 core\n";
		case VERSION_AUTODETECT_FROM_LEVEL_LOAD:
			return "";
		case VERSION_FIXED_IN_FILE:
			return "";
		// Deliberately omit "default:" case to trigger a compiler warning if the SHADER_VERSION enum is expanded but the new cases aren't handled here
	}
	return ""; // Should not not reach here - silence a GCC warning
}

std::string getShaderVersionDirective(const char* shaderData)
{
	// Relying on the GLSL documentation, which says:
	// "The #version directive must occur in a shader before anything else, except for comments and white space."
	// Look for the first line that contains a non-whitespace character (that isn't preceeded by a comment indicator)
	//
	// White space: the space character, horizontal tab, vertical tab, form feed, carriage-return, and line-feed.
	const char glsl_whitespace_chars[] = " \t\v\f\r\n";

	enum CommentMode {
		NONE,
		LINE_DELIMITED_COMMENT,
		ASTERISK_SLASH_DELIMITED_COMMENT
	};

	const size_t shaderLen = strlen(shaderData);
	CommentMode currentCommentMode = CommentMode::NONE;
	const char* pChar = shaderData;
	// Find first non-whitespace, non-comment character
	for (; pChar < shaderData + shaderLen; ++pChar)
	{
		bool foundFirstNonIgnoredChar = false;
		switch (*pChar)
		{
			// the non-newline whitespace chars
			case ' ':
			case '\t':
			case '\v':
			case '\f':
				// ignore whitespace
				break;
			case '\r':
			case '\n':
				// newlines reset line-comment state
				if (currentCommentMode == CommentMode::LINE_DELIMITED_COMMENT)
				{
					currentCommentMode = CommentMode::NONE;
				}
				// otherwise, ignore
				break;
			case '/':
				if (currentCommentMode == CommentMode::NONE)
				{
					// peek-ahead at next char to see if this starts a comment, or should be treated as a usable character
					switch(*(pChar + 1))
					{
						case '/':
							// "//" starts a comment on a line
							currentCommentMode = CommentMode::LINE_DELIMITED_COMMENT;
							++pChar;
							break;
						case '*':
							// "/*" starts a comment until "*/" is detected
							currentCommentMode = CommentMode::ASTERISK_SLASH_DELIMITED_COMMENT;
							++pChar;
							break;
						default:
							// this doesn't start a comment - it's the first usable character
							foundFirstNonIgnoredChar = true;
							break;
					}
				}
				else
				{
					// ignore, part of a comment
				}
				break;
			case '*':
				if (currentCommentMode == CommentMode::ASTERISK_SLASH_DELIMITED_COMMENT)
				{
					// peek-ahead at next char to see if this ends the current comment
					if (*(pChar + 1) == '/')
					{
						// this is the beginning of a "*/" that terminates the current comment
						currentCommentMode = CommentMode::NONE;
						++pChar;
						break;
					}
				}
				else if (currentCommentMode == CommentMode::LINE_DELIMITED_COMMENT)
				{
					// ignore - part of a comment
					break;
				}
				else if (currentCommentMode == CommentMode::NONE)
				{
					// this is the first usable character
					foundFirstNonIgnoredChar = true;
				}
				break;
			default:
				if (currentCommentMode == CommentMode::NONE)
				{
					foundFirstNonIgnoredChar = true;
				}
		}

		if (foundFirstNonIgnoredChar) break;
	}

	if (pChar >= shaderData + shaderLen)
	{
		// Did not find a non-ignored (whitespace/comment) character before the end of the shader?
		return "";
	}

	// Check if the first non-ignored characters start a #version directive
	std::string shaderStringTrimmedBeginning(pChar);
	std::string versionPrefix("#version");
	if (shaderStringTrimmedBeginning.length() < versionPrefix.length())
	{
		// not enough remaining characters to form the "#version" prefix
		return "";
	}
	if (shaderStringTrimmedBeginning.compare(0, versionPrefix.length(), versionPrefix) != 0)
	{
		// Does not start with a version directive
		return "";
	}

	// Starts with a #version directive - extract all the characters after versionPrefix until a newline
	size_t posNextNewline = shaderStringTrimmedBeginning.find_first_of("\r\n", versionPrefix.length());
	size_t lenVersionLine = (posNextNewline != std::string::npos) ? (posNextNewline - versionPrefix.length()) : std::string::npos;
	std::string versionLine = shaderStringTrimmedBeginning.substr(versionPrefix.length(), lenVersionLine);
	// Remove any trailing comment (starting with "//" or "/*")
	size_t posCommentStart = versionLine.find("//");
	if (posCommentStart != std::string::npos)
	{
		versionLine = versionLine.substr(0, posCommentStart);
	}
	posCommentStart = versionLine.find("/*");
	if (posCommentStart != std::string::npos)
	{
		versionLine = versionLine.substr(0, posCommentStart);
	}

	// trim whitespace chars from beginning/end
	versionLine = versionLine.substr(versionLine.find_first_not_of(glsl_whitespace_chars));
	versionLine.erase(versionLine.find_last_not_of(glsl_whitespace_chars)+1);

	return versionLine;
}

SHADER_VERSION pie_detectShaderVersion(const char* shaderData)
{
	std::string shaderVersionDirective = getShaderVersionDirective(shaderData);

	// Special case for external loaded shaders that want to opt-in to an automatic #version header
	// that is the minimum supported on the current OpenGL context.
	// To properly support this, the shaders should be written to support GLSL "version 120" through "version 150 core".
	//
	// For examples on how to do this, see the built-in shaders in the data/shaders directory, and pay
	// particular attention to their use of "#if __VERSION__" preprocessor directives.
	if (shaderVersionDirective == "WZ_GLSL_VERSION_MINIMUM_FOR_CONTEXT")
	{
		return getMinimumShaderVersionForCurrentGLContext();
	}
	// Special case for external loaded shaders that want to opt-in to an automatic #version header
	// that is the maximum supported on the current OpenGL context.
	//
	// Important: For compatibility with the same systems that Warzone supports, you should strive to ensure the
	// shaders are compatible with a minimum of GLSL version 1.20 / GLSL version 1.50 core (and / or fallback gracefully).
	else if (shaderVersionDirective == "WZ_GLSL_VERSION_MAXIMUM_FOR_CONTEXT")
	{
		return getMaximumShaderVersionForCurrentGLContext();
	}
	else
	{
		return SHADER_VERSION::VERSION_FIXED_IN_FILE;
	}
}

SHADER_VERSION autodetectShaderVersion_FromLevelLoad(const char* filePath, const char* shaderContents)
{
	SHADER_VERSION version = pie_detectShaderVersion(shaderContents);
	if (version == SHADER_VERSION::VERSION_FIXED_IN_FILE)
	{
		debug(LOG_WARNING, "SHADER '%s' specifies a fixed #version directive. This may not work with Warzone's ability to use either OpenGL < 3.2 Compatibility Profiles, or OpenGL 3.2+ Core Profiles.", filePath);
	}
	return version;
}

// Read/compile/link shaders
SHADER_MODE pie_LoadShader(SHADER_VERSION vertex_version, SHADER_VERSION fragment_version, const char *programName, const std::string &vertexPath, const std::string &fragmentPath,
	const std::vector<std::string> &uniformNames)
{
	pie_internal::SHADER_PROGRAM program;
	GLint status;
	bool success = true; // Assume overall success

	program.program = glCreateProgram();
	glBindAttribLocation(program.program, 0, "vertex");
	glBindAttribLocation(program.program, 1, "vertexTexCoord");
	glBindAttribLocation(program.program, 2, "vertexColor");
	ASSERT_OR_RETURN(SHADER_NONE, program.program, "Could not create shader program!");

	char* shaderContents = nullptr;

	if (!vertexPath.empty())
	{
		success = false; // Assume failure before reading shader file

		if ((shaderContents = readShaderBuf(vertexPath.c_str())))
		{
			GLuint shader = glCreateShader(GL_VERTEX_SHADER);

			if (vertex_version == SHADER_VERSION::VERSION_AUTODETECT_FROM_LEVEL_LOAD)
			{
				vertex_version = autodetectShaderVersion_FromLevelLoad(vertexPath.c_str(), shaderContents);
			}

			const char* ShaderStrings[2] = { shaderVersionString(vertex_version), shaderContents };

			glShaderSource(shader, 2, ShaderStrings, nullptr);
			glCompileShader(shader);

			// Check for compilation errors
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (!status)
			{
				debug(LOG_ERROR, "Vertex shader compilation has failed [%s]", vertexPath.c_str());
				printShaderInfoLog(LOG_ERROR, shader);
			}
			else
			{
				printShaderInfoLog(LOG_3D, shader);
				glAttachShader(program.program, shader);
				success = true;
			}
			if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
			{
				glObjectLabel(GL_SHADER, shader, -1, vertexPath.c_str());
			}
			free(shaderContents);
			shaderContents = nullptr;
		}
	}

	if (success && !fragmentPath.empty())
	{
		success = false; // Assume failure before reading shader file

		if ((shaderContents = readShaderBuf(fragmentPath.c_str())))
		{
			GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

			if (fragment_version == SHADER_VERSION::VERSION_AUTODETECT_FROM_LEVEL_LOAD)
			{
				fragment_version = autodetectShaderVersion_FromLevelLoad(fragmentPath.c_str(), shaderContents);
			}

			const char* ShaderStrings[2] = { shaderVersionString(fragment_version), shaderContents };

			glShaderSource(shader, 2, ShaderStrings, nullptr);
			glCompileShader(shader);

			// Check for compilation errors
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (!status)
			{
				debug(LOG_ERROR, "Fragment shader compilation has failed [%s]", fragmentPath.c_str());
				printShaderInfoLog(LOG_ERROR, shader);
			}
			else
			{
				printShaderInfoLog(LOG_3D, shader);
				glAttachShader(program.program, shader);
				success = true;
			}
			if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
			{
				glObjectLabel(GL_SHADER, shader, -1, fragmentPath.c_str());
			}
			free(shaderContents);
			shaderContents = nullptr;
		}
	}

	if (success)
	{
		glLinkProgram(program.program);

		// Check for linkage errors
		glGetProgramiv(program.program, GL_LINK_STATUS, &status);
		if (!status)
		{
			debug(LOG_ERROR, "Shader program linkage has failed [%s, %s]", vertexPath.c_str(), fragmentPath.c_str());
			printProgramInfoLog(LOG_ERROR, program.program);
			success = false;
		}
		else
		{
			printProgramInfoLog(LOG_3D, program.program);
		}
		if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
		{
			glObjectLabel(GL_PROGRAM, program.program, -1, programName);
		}
	}
	GLuint p = program.program;
	std::transform(uniformNames.begin(), uniformNames.end(),
		std::back_inserter(program.locations),
		[p](const std::string name) { return glGetUniformLocation(p, name.data()); });

	getLocs(&program);
	glUseProgram(0);

	pie_internal::shaderProgram.push_back(program);

	return SHADER_MODE(pie_internal::shaderProgram.size() - 1);
}

//static float fogBegin;
//static float fogEnd;

// Run from screen.c on init.
bool pie_LoadShaders()
{
	// note: actual loading of shaders now occurs in gfx_api

	gfx_api::gfxUByte rect[] {
		0, 255, 0, 255,
		0, 0, 0, 255,
		255, 255, 0, 255,
		255, 0, 0, 255
	};
	if (!pie_internal::rectBuffer)
		pie_internal::rectBuffer = gfx_api::context::get().create_buffer_object(gfx_api::buffer::usage::vertex_buffer);
	pie_internal::rectBuffer->upload(16 * sizeof(gfx_api::gfxUByte), rect);

	return true;
}

void pie_SetShaderTime(uint32_t shaderTime)
{
	uint32_t base = shaderTime % 1000;
	if (base > 500)
	{
		base = 1000 - base;	// cycle
	}
	timeState = (gfx_api::gfxFloat)base / 1000.0f;
}

float pie_GetShaderTime()
{
	return timeState;
}

void pie_SetShaderEcmEffect(bool value)
{
	ecmState = (int)value;
}

int pie_GetShaderEcmEffect()
{
	return ecmState;
}

void pie_SetShaderStretchDepth(float stretch)
{
	shaderStretch = stretch;
}

float pie_GetShaderStretchDepth()
{
	return shaderStretch;
}

/// Set the OpenGL fog start and end
void pie_UpdateFogDistance(float begin, float end)
{
	rendStates.fogBegin = begin;
	rendStates.fogEnd = end;
}

void pie_SetFogStatus(bool val)
{
	if (rendStates.fogEnabled)
	{
		//fog enabled so toggle if required
		rendStates.fog = val;
	}
	else
	{
		rendStates.fog = false;
	}
}

RENDER_STATE getCurrentRenderState()
{
	return rendStates;
}

int pie_GetMaxAntialiasing()
{
	int32_t maxSamples = gfx_api::context::get().get_context_value(gfx_api::context::context_value::MAX_SAMPLES);
	return maxSamples;
}
