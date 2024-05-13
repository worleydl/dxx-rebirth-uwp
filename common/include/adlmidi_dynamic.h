/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
/*
 * This is a dynamic interface to libADLMIDI, the OPL3 synthesizer library.
 */

#include <memory>
#include <stdint.h>
#include "dxxsconf.h"

#if DXX_USE_ADLMIDI
struct ADL_MIDIPlayer;

enum ADLMIDI_SampleType
{
	ADLMIDI_SampleType_S16 = 0
};

struct ADLMIDI_AudioFormat
{
	enum ADLMIDI_SampleType type;
	unsigned containerSize;
	unsigned sampleOffset;
};

enum ADL_Emulator
{
    ADLMIDI_EMU_DOSBOX = 2,
};

// TODO: Just use adlmidi.h?
// ADLMIDI_VolumeModels/ADLMIDI_ChannelAlloc come from libADLMIDI's adlmidi.h
/**
 * @brief Volume scaling models
 */
enum ADLMIDI_VolumeModels
{
    /*! Automatical choice by the specific bank */
    ADLMIDI_VolumeModel_AUTO = 0,
    ADLMIDI_VolumeModel_Generic = 1,
    /*! Linearized scaling model, most standard */
    ADLMIDI_VolumeModel_HMI = 10,
    ADLMIDI_VolumeModel_HMI_OLD = 11,
    /*! Count of available volume model modes */
    ADLMIDI_VolumeModel_Count
};

/*!
 * \brief Algorithms of channel allocation for new notes
 */
enum ADLMIDI_ChannelAlloc
{
    /*! Automatical choise of the method according to the volume model and internal preferrences */
    ADLMIDI_ChanAlloc_AUTO = -1,
    /*! Take only channels that has expired sounding delay */
    ADLMIDI_ChanAlloc_OffDelay,
    /*! Take any first released channel with the same instrument */
    ADLMIDI_ChanAlloc_SameInst,
    /*! Take any first released channel */
    ADLMIDI_ChanAlloc_AnyReleased,
    /*! Count of available channel allocation modes */
    ADLMIDI_ChanAlloc_Count
};


/*
 * A few embedded bank numbers.
 */
enum class ADL_EmbeddedBank
{
	MILES_AIL = 0,
	BISQWIT = 1,
	DESCENT = 2,
	// DESCENT_INT = 3,
	// DESCENT_HAM = 4,
	// DESCENT_RICK = 5,
	// DESCENT2 = 6,
	LBA_4OP = 31,
	THE_FATMAN_2OP = 58,
	THE_FATMAN_4OP = 59,
	JAMIE_OCONNELL = 66,
	NGUYEN_WOHLSTAND_4OP = 68,
	DMXOPL3 = 72,
	APOGEE_IMF90 = 74,
};

extern ADL_MIDIPlayer *(*adl_init)(long sample_rate);
extern void (*adl_close)(ADL_MIDIPlayer *device);
extern void (*adl_reset)(struct ADL_MIDIPlayer *device);
extern int (*adl_switchEmulator)(ADL_MIDIPlayer *device, int emulator);
extern int (*adl_setNumChips)(ADL_MIDIPlayer *device, int numChips);
extern int (*adl_setBank)(ADL_MIDIPlayer *device, int bank);
extern void (*adl_setChannelAllocMode)(ADL_MIDIPlayer *device, int chanAlloc);
extern void (*adl_setSoftPanEnabled)(ADL_MIDIPlayer *device, int softPanEn);
extern void (*adl_setLoopEnabled)(ADL_MIDIPlayer *device, int loopEn);
extern void (*adl_setVolumeRangeModel)(ADL_MIDIPlayer *device, int volumeModel);
extern int (*adl_openData)(ADL_MIDIPlayer *device, const void *mem, unsigned long size);
extern int (*adl_openFile)(ADL_MIDIPlayer *device, const char *filePath);
extern int (*adl_playFormat)(ADL_MIDIPlayer *device, int sampleCount, uint8_t *left, uint8_t *right, const ADLMIDI_AudioFormat *format);

struct ADLMIDI_delete
{
	void operator()(ADL_MIDIPlayer *x)
	{
		adl_close(x);
	}
};

typedef std::unique_ptr<ADL_MIDIPlayer, ADLMIDI_delete> ADL_MIDIPlayer_t;
#endif
