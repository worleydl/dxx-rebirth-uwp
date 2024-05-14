/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
/*
 * This is a dynamic interface to libADLMIDI, the OPL3 synthesizer library.
 */

#include "adlmidi_dynamic.h"
#include "console.h"
#if !defined(_WIN32)
#include <dlfcn.h>
#else
#include <windows.h>
#endif

namespace {

#if defined(_WIN32)
enum
{
	RTLD_LAZY = 1, RTLD_NOW = 2
};

HMODULE dlopen(const char *const filename, int)
{
	return LoadLibraryA(filename);
}

void dlclose(const HMODULE handle)
{
	FreeLibrary(handle);
}

void *dlsym(const HMODULE handle, const char *const symbol)
{
	return reinterpret_cast<void *>(
		GetProcAddress(handle, symbol));
}
#else
using HMODULE = void *;
#endif

}

namespace dcx {

static ADL_MIDIPlayer *adl_init_failure(long)
{
	return nullptr;
}

static void reported_failed_load_function(const char *const name)
{
	con_printf(CON_NORMAL, "ADLMIDI: failed to load the dynamic function \"%s\"", name);
}

template <class F>
static bool load_function(const HMODULE handle, const char *const name, F *&fptr)
{
	const auto f = reinterpret_cast<F *>(dlsym(handle, name));
	fptr = f;
	if (!f)
		/* Use out of line report function to prevent redundant instantiations
		 * on a per-type basis.
		 */
		reported_failed_load_function(name);
	return f;
}

static ADL_MIDIPlayer *adl_init_first_call(long sample_rate)
{
#if defined(_WIN32)
	const char *library_name = "ADLMIDI.dll";
#elif defined(__APPLE__)
	const char *library_name = "libADLMIDI.dylib";
#else
	const char *library_name = "libADLMIDI.so";
#endif
	const auto handle = dlopen(library_name, RTLD_NOW);
	if (!handle ||
	    !load_function(handle, "adl_init", adl_init) ||
	    !load_function(handle, "adl_close", adl_close) ||
	    !load_function(handle, "adl_reset", adl_reset) ||
	    !load_function(handle, "adl_switchEmulator", adl_switchEmulator) ||
	    !load_function(handle, "adl_setNumChips", adl_setNumChips) ||
	    !load_function(handle, "adl_setBank", adl_setBank) ||
	    !load_function(handle, "adl_setChannelAllocMode", adl_setChannelAllocMode) ||
	    !load_function(handle, "adl_setSoftPanEnabled", adl_setSoftPanEnabled) ||
	    !load_function(handle, "adl_setLoopEnabled", adl_setLoopEnabled) ||
	    !load_function(handle, "adl_setVolumeRangeModel", adl_setVolumeRangeModel) ||
	    !load_function(handle, "adl_openData", adl_openData) ||
	    !load_function(handle, "adl_openFile", adl_openFile) ||
	    !load_function(handle, "adl_playFormat", adl_playFormat))
	{
		adl_init = &adl_init_failure;
		if (handle)
			dlclose(handle);
		else
			con_printf(CON_NORMAL, "ADLMIDI: failed to load the dynamic library \"%s\"", library_name);
	}
	else
		con_printf(CON_NORMAL, "ADLMIDI: loaded the dynamic OPL3 synthesizer");
	return adl_init(sample_rate);
}

}

ADL_MIDIPlayer *(*adl_init)(long sample_rate) = &dcx::adl_init_first_call;
void (*adl_close)(ADL_MIDIPlayer *device);
void (*adl_reset)(ADL_MIDIPlayer *device);
int (*adl_switchEmulator)(ADL_MIDIPlayer *device, int emulator);
int (*adl_setNumChips)(ADL_MIDIPlayer *device, int numChips);
int (*adl_setBank)(ADL_MIDIPlayer *device, int bank);
void (*adl_setChannelAllocMode)(ADL_MIDIPlayer *device, int chanalloc);
void (*adl_setSoftPanEnabled)(ADL_MIDIPlayer *device, int softPanEn);
void (*adl_setLoopEnabled)(ADL_MIDIPlayer *device, int loopEn);
void (*adl_setVolumeRangeModel)(ADL_MIDIPlayer *device, int volumeModel);
int (*adl_openData)(ADL_MIDIPlayer *device, const void *mem, unsigned long size);
int (*adl_openFile)(ADL_MIDIPlayer *device, const char *filePath);
int (*adl_playFormat)(ADL_MIDIPlayer *device, int sampleCount, uint8_t *left, uint8_t *right, const ADLMIDI_AudioFormat *format);
