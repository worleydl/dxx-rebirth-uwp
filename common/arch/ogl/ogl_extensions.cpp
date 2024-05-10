/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */

/* OpenGL extensions:
 * We use global function pointer for any extension function we want to use.
 */

#include <string.h>
#include <SDL.h>

#include "ogl_extensions.h"
#include "console.h"

#include "dxxsconf.h"
#include "dsx-ns.h"
#include <array>

namespace dcx {

/* GL_ARB_sync */
bool ogl_have_ARB_sync = false;
PFNGLFENCESYNCPROC glFenceSyncFunc = NULL;
PFNGLDELETESYNCPROC glDeleteSyncFunc = NULL;
PFNGLCLIENTWAITSYNCPROC glClientWaitSyncFunc = NULL;

/* GL_EXT_texture_filter_anisotropic */
GLfloat ogl_maxanisotropy = 0.0f;

namespace {

#ifdef LEGACY_GL_INIT
static std::array<long, 2> parse_version_str(const char *v)
{
	std::array<long, 2> version;
	version[0]=1;
	version[1]=0;
	if (v) {
		char *ptr;
		if (v[0] == 'O') {
			// OpenGL ES uses the format "OpenGL ES-xx major.minor"
			const auto &prefix_gles = "OpenGL ES-";
			if (!strncmp(v, prefix_gles, sizeof(prefix_gles)-1)) {
				// skip the prefix
				v += sizeof(prefix_gles)-1;
				// skip the profile marker
				if (v[0] && v[1]) {
					v +=2;
				}
			}
		}
		version[0]=strtol(v,&ptr,10);
		if (ptr[0]) 
			version[1]=strtol(ptr+1,NULL,10);
	}
	return version;
}

static bool is_ext_supported(const char *extensions, const std::span<const char> name)
{
	if (extensions)
	{
		const auto found = strstr(extensions, name.data());
		if (found) {
			// check that the name is actually complete */
			const char c = found[name.size() - 1];
			if (c == ' ' || c == 0)
				return true;
		}
	}
	return false;
}

enum support_mode {
	NO_SUPPORT=0,
	SUPPORT_CORE=1,
	SUPPORT_EXT=2
};

static support_mode is_supported(const char *extensions, const std::array<long, 2> &version, const std::span<const char> name, long major, long minor, long major_es, long minor_es)
{
#if DXX_USE_OGLES
	static_cast<void>(major);
	static_cast<void>(minor);
	if ( (major_es > 0) && ((version[0] > major_es) || (version[0] == major_es && version[1] >= minor_es)) )
		return SUPPORT_CORE;
#else
	static_cast<void>(major_es);
	static_cast<void>(minor_es);
	if ( (major > 0) && ((version[0] > major) || (version[0] == major && version[1] >= minor)) )
		return SUPPORT_CORE;
#endif

	if (is_ext_supported(extensions, name))
		return SUPPORT_EXT;
	return NO_SUPPORT;
}

}

void ogl_extensions_init()
{
	const auto version_str = reinterpret_cast<const char *>(glGetString(GL_VERSION));
	if (!version_str) {
		con_puts(CON_URGENT, "DXX-Rebirth: no valid OpenGL context when querying GL extensions!");
		return;
	}
	const auto version = parse_version_str(version_str);
	const auto extension_str = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));

#if DXX_USE_OGLES
#define DXX_OGL_STRING  " ES"
#else
#define DXX_OGL_STRING  ""
#endif
	con_printf(CON_VERBOSE, "DXX-Rebirth: OpenGL" DXX_OGL_STRING ": version %ld.%ld (%s)", version[0], version[1], version_str);
#undef DXX_OGL_STRING

	/* GL_EXT_texture_filter_anisotropic */
	if (is_supported(extension_str, version, "GL_EXT_texture_filter_anisotropic", -1, -1, -1, -1)) {
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ogl_maxanisotropy);
		con_printf(CON_VERBOSE, "DXX-Rebirth: OpenGL: GL_EXT_texture_filter_anisotropic available, max anisotropy: %f", ogl_maxanisotropy);
	} else {
		ogl_maxanisotropy=0.0f;
		con_puts(CON_VERBOSE, "DXX-Rebirth: OpenGL: GL_EXT_texture_filter_anisotropic not available");
	}

	/* GL_ARB_sync */
	if (is_supported(extension_str, version, "GL_ARB_sync", 3, 2, 3, 0)) {
		glFenceSyncFunc = reinterpret_cast<PFNGLFENCESYNCPROC>(SDL_GL_GetProcAddress("glFenceSync"));
		glDeleteSyncFunc = reinterpret_cast<PFNGLDELETESYNCPROC>(SDL_GL_GetProcAddress("glDeleteSync"));
		glClientWaitSyncFunc = reinterpret_cast<PFNGLCLIENTWAITSYNCPROC>(SDL_GL_GetProcAddress("glClientWaitSync"));

	}
	const auto s = (glFenceSyncFunc && glDeleteSyncFunc && glClientWaitSyncFunc)
		? (ogl_have_ARB_sync = true, std::span<const char>{"DXX-Rebirth: OpenGL: GL_ARB_sync available"})
		: std::span<const char>{"DXX-Rebirth: OpenGL: GL_ARB_sync not available"};
	con_puts(CON_VERBOSE, s);
#endif
}

}
