/*
 * Portions of this file are copyright Rebirth contributors and licensed as
 * described in COPYING.txt.
 * Portions of this file are copyright Parallax Software and licensed
 * according to the Parallax license below.
 * See COPYING.txt for license details.

THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Routines to manage the songs in Descent.
 *
 */

#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dxxerror.h"
#include "pstypes.h"
#include "songs.h"
#include "strutil.h"
#include "digi.h"
#include "rbaudio.h"
#if DXX_USE_SDLMIXER
#include "digi_mixer_music.h"
#endif
#include "jukebox.h"
#include "config.h"
#include "u_mem.h"
#include "args.h"
#include "physfsx.h"
#include "game.h"
#include "console.h"
#include <memory>

namespace dcx {

namespace {

class user_configured_level_songs : std::unique_ptr<bim_song_info[]>
{
	using base_type = std::unique_ptr<bim_song_info[]>;
	std::size_t count{};
public:
	using base_type::operator[];
	using base_type::operator bool;
	constexpr user_configured_level_songs() = default;
	explicit user_configured_level_songs(const std::size_t length) :
		base_type{std::make_unique<bim_song_info[]>(length)}, count{length}
	{
	}
	auto size() const
	{
		return count;
	}
	void reset()
	{
		count = {};
		base_type::reset();
	}
	void resize(const std::size_t length)
	{
		base_type::operator=(std::make_unique<bim_song_info[]>(length));
		count = {length};
	}
	user_configured_level_songs &operator=(user_configured_level_songs &&) = default;
	void operator=(std::vector<bim_song_info> &&v);
};

void user_configured_level_songs::operator=(std::vector<bim_song_info> &&v)
{
	if (v.empty())
	{
		/* If the vector is empty, use `reset` so that this object
		 * stores `nullptr` instead of a pointer to a zero-length array.
		 * Storing `nullptr` causes `operator bool()` to return false.
		 * Storing a zero-length array causes `operator bool()` to
		 * return true.
		 *
		 * A true return causes a divide-by-zero later when the object
		 * is considered true, but then reports a length of zero.
		 */
		reset();
		return;
	}
	resize(v.size());
	std::move(v.begin(), v.end(), &this->operator[](0u));
}

static int Song_playing = -1; // -1 if no song playing, else the Descent song number
#if DXX_USE_SDL_REDBOOK_AUDIO
static int Redbook_playing = 0; // Redbook track num differs from Song_playing. We need this for Redbook repeat hooks.
#endif

user_configured_level_songs BIMSongs;
user_configured_level_songs BIMSecretSongs;

}

#define EXTMUSIC_VOLUME_SCALE	(255)

//takes volume in range 0..8
//NOTE that we do not check what is playing right now (except Redbook) This is because here we don't (want) know WHAT we're playing - let the subfunctions do it (i.e. digi_win32_set_music_volume() knows if a MIDI plays or not)
void songs_set_volume(int volume)
{
#ifdef _WIN32
	digi_win32_set_midi_volume(volume);
#endif
#if DXX_USE_SDL_REDBOOK_AUDIO
	if (CGameCfg.MusicType == music_type::Redbook)
	{
		RBASetVolume(0);
		RBASetVolume(volume);
	}
#else
	(void)volume;
#endif
#if DXX_USE_SDLMIXER
	mix_set_music_volume(volume);
#endif
}

namespace {

static bool is_valid_song_extension(const std::ranges::subrange<std::span<char>::iterator> extension)
{
	/* This is a special purpose tolower, which only works due to the special
	 * circumstances in this function.
	 * - the user-supplied `extension` may be any set of characters, including
	 *   non-letters
	 * - candidate extensions are known to be lowercase letters only, and are
	 *   not transformed
	 * - applying this transform to a user-defined input character has 3 cases:
	 *   - if the input is ASCII uppercase, it becomes ASCII lowercase
	 *   - if the input is ASCII lowercase, it is unchanged
	 *   - if the input is not a letter, it is changed to some other non-letter
	 * Since the comparison only seeks matches, and all candidate extensions
	 * are all lowercase letters, changing one non-letter to a different
	 * non-letter preserves the inequality.
	 */
	auto &&lower_letter = [](const char c) -> char {
		return c | ('a' - 'A');
	};
	using namespace std::string_view_literals;
#define SONG_STRING_VIEW_CONCAT_UDL(EXTENSION)	EXTENSION##sv
#define SONG_STRING_VIEW(EXTENSION)	SONG_STRING_VIEW_CONCAT_UDL(EXTENSION)
	const auto is_HMP{std::ranges::equal(extension, SONG_STRING_VIEW(SONG_EXT_HMP), {}, lower_letter)};
#if DXX_USE_SDLMIXER
	if (!is_HMP)
	{
		if (const auto is_MID{std::ranges::equal(extension, SONG_STRING_VIEW(SONG_EXT_MID), {}, lower_letter)})
			return is_MID;
		if (const auto is_OGG{std::ranges::equal(extension, SONG_STRING_VIEW(SONG_EXT_OGG), {}, lower_letter)})
			return is_OGG;
		if (const auto is_FLAC{std::ranges::equal(extension, SONG_STRING_VIEW(SONG_EXT_FLAC), {}, lower_letter)})
			return is_FLAC;
		if (const auto is_MP3{std::ranges::equal(extension, SONG_STRING_VIEW(SONG_EXT_MP3), {}, lower_letter)})
			return is_MP3;
	}
#endif
#undef SONG_STRING_VIEW
#undef SONG_STRING_VIEW_CONCAT_UDL
	return is_HMP;
}

template <std::size_t N>
requires(
	/* Compile-time check that the input string will not overflow the output
	 * buffer.
	 */
	requires(bim_song_info song) {
		requires(std::size(song.filename) >= N);
	}
)
static inline void assign_builtin_song(bim_song_info &song, const char (&str)[N])
{
	std::ranges::copy(str, std::ranges::begin(song.filename));
}

static void add_song(std::vector<bim_song_info> &songs, const std::ranges::subrange<std::span<char>::iterator> entry_input)
{
	/* Some versions of `descent.sng` are not a simple list of songs, but
	 * instead have a song, whitespace, and then other text on the line.  The
	 * original parser used sscanf to extract the filename, which caused the
	 * extraction to stop on the first whitespace.  Implement an equivalent
	 * early stop here, by scanning the input range for whitespace.  When no
	 * whitespace is found, `input == entry_input`.
	 */
	const std::ranges::subrange<std::span<char>::iterator> input{
		entry_input.begin(),
		std::ranges::find_if(entry_input, [](const char c) { return std::isspace(static_cast<unsigned>(c)); })
	};
	if (input.size() >= std::tuple_size<decltype(bim_song_info::filename)>::value)
		/* Drop excessively long filenames. */
		return;
	auto &&reverse_input{std::ranges::views::reverse(input)};
	if (const auto &&ri_dot{std::ranges::find(reverse_input, '.')}; ri_dot == reverse_input.end())
		/* Drop filenames that lack a dot. */
		return;
	/* Due to `std::reverse_iterator` rules, `reverse_iterator::operator*()`
	 * returns `*std::prev(reverse_iterator::base())`, so `find` returns an
	 * iterator one forward of the '.'.  Therefore, `.base()` points to the
	 * first character in the extension, not to the dot.
	 */
	else if (!is_valid_song_extension({ri_dot.base(), input.end()}))
		/* Drop filenames that lack a valid song extension. */
		return;
	/* The input is not a `bim_song_info`, so a direct use of
	 * `emplace_back(input)` is not well-formed.
	 *
	 * Using `emplace_back(bim_song_info(...))` is well-formed, but constructs
	 * a temporary to pass to emplace_back, which then copy-constructs the
	 * vector element.
	 *
	 * Pass an instance of an object that is convertible to the element_type,
	 * and rely on copy elision to construct the converted value directly into
	 * the vector element.
	 */
	struct emplace_subrange
	{
		std::ranges::subrange<std::span<char>::iterator> input;
		operator bim_song_info() const
		{
			/* `input` does not include a terminating null, so zero initialize
			 * `r` before copying `input` into it.  Zeroing the entire buffer
			 * is cheap, and avoids a lookup of the length of the input buffer.
			 */
			bim_song_info r{};
			std::ranges::copy(input, std::ranges::begin(r.filename));
			return r;
		}
	};
	songs.emplace_back(emplace_subrange{input});
}

}
}

namespace dsx {
namespace {

// Set up everything for our music
// NOTE: you might think this is done once per runtime but it's not! It's done for EACH song so that each mission can have it's own descent.sng structure. We COULD optimize that by only doing this once per mission.
static void songs_init()
{
	using namespace std::string_view_literals;

	BIMSongs.reset();
	BIMSecretSongs.reset();

	int canUseExtensions = 0;
	// try dxx-r.sng - a songfile specifically for dxx which level authors CAN use (dxx does not care if descent.sng contains MP3/OGG/etc. as well) besides the normal descent.sng containing files other versions of the game cannot play. this way a mission can contain a DOS-Descent compatible OST (hmp files) as well as a OST using MP3, OGG, etc.
	auto fp = PHYSFSX_openReadBuffered("dxx-r.sng").first;

	if (!fp) // try to open regular descent.sng
		fp = PHYSFSX_openReadBuffered("descent.sng").first;
	else
		canUseExtensions = 1; // can use extensions ONLY if dxx-r.sng

	if (!fp) // No descent.sng available. Define a default song-set
	{
		constexpr std::size_t predef{30}; // define 30 songs - period

		user_configured_level_songs builtin_songs{predef};

		assign_builtin_song(builtin_songs[SONG_TITLE], "descent.hmp");
		assign_builtin_song(builtin_songs[SONG_BRIEFING], "briefing.hmp");
		assign_builtin_song(builtin_songs[SONG_CREDITS], "credits.hmp");
		assign_builtin_song(builtin_songs[SONG_ENDLEVEL], "endlevel.hmp");	// can't find it? give a warning
		assign_builtin_song(builtin_songs[SONG_ENDGAME], "endgame.hmp");	// ditto

		for (const auto i : constant_xrange<unsigned, SONG_FIRST_LEVEL_SONG, predef>{})
		{
			auto &s = builtin_songs[i];
			const auto d = std::data(s.filename);
			const auto fs = std::size(s.filename);
			if (!PHYSFSX_exists((snprintf(d, fs, "game%02u.hmp", i - SONG_FIRST_LEVEL_SONG + 1), d), 1) &&
				!PHYSFSX_exists((snprintf(d, fs, "game%u.hmp", i - SONG_FIRST_LEVEL_SONG), d), 1))
			{
				s = {};	// music not available
				break;
			}
		}
		BIMSongs = std::move(builtin_songs);
		/* BIMSecretSongs remains unset.  There is no built-in default song
		 * list for secret levels.
		 */
	}
	else
	{
		std::vector<bim_song_info> main_songs, secret_songs;
		for (PHYSFSX_gets_line_t<81> inputline; const auto result{PHYSFSX_fgets(inputline, fp)};)
		{
			if (result.empty())
				continue;
			{
				if (canUseExtensions)
				{
					const std::string_view secret_label{"!Rebirth.secret "sv};
					// extension stuffs
					if (result.size() > secret_label.size() &&
						std::ranges::equal(std::span(result.begin(), secret_label.size()), secret_label))
					{
						add_song(secret_songs, result.next(secret_label.size()));
						continue;
					}
				}
				add_song(main_songs, result);
			}
		}
#if defined(DXX_BUILD_DESCENT_I)
		// HACK: If Descent.hog is patched from 1.0 to 1.5, descent.sng is turncated. So let's patch it up here
		constexpr std::size_t truncated_song_count = 12;
		if (!canUseExtensions &&
			main_songs.size() == truncated_song_count &&
			PHYSFS_fileLength(fp) == 422)
		{
			main_songs.resize(truncated_song_count + 15);
			for (const auto i : constant_xrange<unsigned, truncated_song_count, 27>{})
			{
				auto &s = main_songs[i];
				snprintf(std::data(s.filename), std::size(s.filename), "game%02u.hmp", i - 4);
			}
		}
#endif
		BIMSongs = std::move(main_songs);
		BIMSecretSongs = std::move(secret_songs);
	}

	fp.reset();

	if (CGameArg.SndNoMusic)
		CGameCfg.MusicType = music_type::None;

	// If SDL_Mixer is not supported (or deactivated), switch to no-music type if SDL_mixer-related music type was selected
	else if (CGameArg.SndDisableSdlMixer)
	{
#ifndef _WIN32
		if (CGameCfg.MusicType == music_type::Builtin)
			CGameCfg.MusicType = music_type::None;
#endif
		if (CGameCfg.MusicType == music_type::Custom)
			CGameCfg.MusicType = music_type::None;
	}

	switch (CGameCfg.MusicType)
	{
		case music_type::None:
		case music_type::Builtin:
			break;
#if DXX_USE_SDL_REDBOOK_AUDIO
		case music_type::Redbook:
			RBAInit();
			break;
#endif
		case music_type::Custom:
			jukebox_load();
			break;
	}

	songs_set_volume(CGameCfg.MusicVolume);
}
}
}

void songs_uninit()
{
	songs_stop_all();
#if DXX_USE_SDLMIXER
	jukebox_unload();
#endif
	BIMSecretSongs.reset();
	BIMSongs.reset();
}

//stop any songs - builtin, redbook or jukebox - that are currently playing
void songs_stop_all(void)
{
#ifdef _WIN32
	digi_win32_stop_midi_song();	// Stop midi song, if playing
#endif
#if DXX_USE_SDL_REDBOOK_AUDIO
	RBAStop();
#endif
#if DXX_USE_SDLMIXER
	mix_stop_music();
#endif

	Song_playing = -1;
}

void songs_pause(void)
{
#ifdef _WIN32
	digi_win32_pause_midi_song();
#endif
#if DXX_USE_SDL_REDBOOK_AUDIO
	if (CGameCfg.MusicType == music_type::Redbook)
		RBAPause();
#endif
#if DXX_USE_SDLMIXER
	mix_pause_music();
#endif
}

void songs_resume(void)
{
#ifdef _WIN32
	digi_win32_resume_midi_song();
#endif
#if DXX_USE_SDL_REDBOOK_AUDIO
	if (CGameCfg.MusicType == music_type::Redbook)
		RBAResume();
#endif
#if DXX_USE_SDLMIXER
	mix_resume_music();
#endif
}

void songs_pause_resume(void)
{
#if DXX_USE_SDL_REDBOOK_AUDIO
	if (CGameCfg.MusicType == music_type::Redbook)
		RBAPauseResume();
#endif
#if DXX_USE_SDLMIXER
	mix_pause_resume_music();
#endif
}

#if defined(DXX_BUILD_DESCENT_I)
/*
 * This list may not be exhaustive!!
 */
#define D1_MAC_OEM_DISCID       0xde0feb0e // Descent CD that came with the Mac Performa 6400, hope mine isn't scratched [too much]

#if DXX_USE_SDL_REDBOOK_AUDIO
#define REDBOOK_ENDLEVEL_TRACK		4
#define REDBOOK_ENDGAME_TRACK		(RBAGetNumberOfTracks())
#define REDBOOK_FIRST_LEVEL_TRACK	(songs_have_cd() ? 6 : 1)
#endif
#elif defined(DXX_BUILD_DESCENT_II)
/*
 * Some of these have different Track listings!
 * Which one is the "correct" order?
 */
#define D2_1_DISCID         0x7d0ff809 // Descent II
#define D2_2_DISCID         0xe010a30e // Descent II
#define D2_3_DISCID         0xd410070d // Descent II
#define D2_4_DISCID         0xc610080d // Descent II
#define D2_DEF_DISCID       0x87102209 // Definitive collection Disc 2
#define D2_OEM_DISCID       0xac0bc30d // Destination: Quartzon
#define D2_OEM2_DISCID      0xc40c0a0d // Destination: Quartzon
#define D2_VERTIGO_DISCID   0x53078208 // Vertigo
#define D2_VERTIGO2_DISCID  0x64071408 // Vertigo + DMB
#define D2_MAC_DISCID       0xb70ee40e // Macintosh
#define D2_IPLAY_DISCID     0x22115710 // iPlay for Macintosh

#if DXX_USE_SDL_REDBOOK_AUDIO
#define REDBOOK_TITLE_TRACK         2
#define REDBOOK_CREDITS_TRACK       3
#define REDBOOK_FIRST_LEVEL_TRACK   (songs_have_cd() ? 4 : 1)
#endif
#endif

// 0 otherwise
namespace dsx {
namespace {
#if DXX_USE_SDL_REDBOOK_AUDIO
static int songs_have_cd()
{
	unsigned long discid;

	if (CGameCfg.OrigTrackOrder)
		return 1;

	if (!(CGameCfg.MusicType == music_type::Redbook))
		return 0;

	discid = RBAGetDiscID();
	con_printf(CON_DEBUG, "CD-ROM disc ID is 0x%08lx", discid);

	switch (discid) {
#if defined(DXX_BUILD_DESCENT_I)
		case D1_MAC_OEM_DISCID:	// Doesn't work with your Mac Descent CD? Please tell!
			return 1;
#elif defined(DXX_BUILD_DESCENT_II)
	case D2_1_DISCID:
	case D2_2_DISCID:
	case D2_3_DISCID:
	case D2_4_DISCID:
	case D2_DEF_DISCID:
	case D2_OEM_DISCID:
	case D2_OEM2_DISCID:
	case D2_VERTIGO_DISCID:
	case D2_VERTIGO2_DISCID:
	case D2_MAC_DISCID:
	case D2_IPLAY_DISCID:
		return 1;
#endif
		default:
			return 0;
	}
}
#endif

#if defined(DXX_BUILD_DESCENT_I)
#if DXX_USE_SDL_REDBOOK_AUDIO
static void redbook_repeat_func()
{
	pause_game_world_time p;
	RBAPlayTracks(Redbook_playing, 0, redbook_repeat_func);
}
#endif
#elif defined(DXX_BUILD_DESCENT_II)
#if DXX_USE_SDL_REDBOOK_AUDIO || DXX_USE_SDLMIXER
static void play_credits_track()
{
	pause_game_world_time p;
	songs_play_song(SONG_CREDITS, 1);
}
#endif
#endif
}
}

// play a filename as music, depending on filename extension.
int songs_play_file(const char *filename, int repeat, void (*hook_finished_track)())
{
	songs_stop_all();
#if defined(_WIN32) || DXX_USE_SDLMIXER
	const char *fptr = strrchr(filename, '.');
	if (fptr == NULL)
		return 0;

	++ fptr;
	if (!d_stricmp(fptr, SONG_EXT_HMP))
	{
#if defined(_WIN32)
		return digi_win32_play_midi_song( filename, repeat );
#elif DXX_USE_SDLMIXER
		return mix_play_file( filename, repeat, hook_finished_track );
#else
		return 0;
#endif
	}
#if DXX_USE_SDLMIXER
	else if ( !d_stricmp(fptr, SONG_EXT_MID) ||
			!d_stricmp(fptr, SONG_EXT_OGG) ||
			!d_stricmp(fptr, SONG_EXT_FLAC) ||
			!d_stricmp(fptr, SONG_EXT_MP3) )
	{
		return mix_play_file( filename, repeat, hook_finished_track );
	}
#endif
#endif
	return 0;
}

namespace dsx {
int songs_play_song( int songnum, int repeat )
{
	songs_init();

	switch (CGameCfg.MusicType)
	{
		case music_type::Builtin:
		{
			if (!BIMSongs)
				break;
			// EXCEPTION: If SONG_ENDLEVEL is not available, continue playing level song.
			auto &s = BIMSongs[songnum];
			if (Song_playing >= SONG_FIRST_LEVEL_SONG && songnum == SONG_ENDLEVEL && !PHYSFSX_exists(std::data(s.filename), 1))
				return Song_playing;

			Song_playing = -1;
			if (songs_play_file(std::data(s.filename), repeat, nullptr))
				Song_playing = songnum;
			break;
		}
#if DXX_USE_SDL_REDBOOK_AUDIO
		case music_type::Redbook:
		{
			int num_tracks = RBAGetNumberOfTracks();

#if defined(DXX_BUILD_DESCENT_I)
			Song_playing = -1;
			if ((songnum < SONG_ENDGAME) && (songnum + 2 <= num_tracks))
			{
				if (RBAPlayTracks(songnum + 2, songnum + 2, repeat ? redbook_repeat_func : NULL))
				{
					Redbook_playing = songnum + 2;
					Song_playing = songnum;
				}
			}
			else if ((songnum == SONG_ENDGAME) && (REDBOOK_ENDGAME_TRACK <= num_tracks)) // The endgame track is the last track
			{
				if (RBAPlayTracks(REDBOOK_ENDGAME_TRACK, REDBOOK_ENDGAME_TRACK, repeat ? redbook_repeat_func : NULL))
				{
					Redbook_playing = REDBOOK_ENDGAME_TRACK;
					Song_playing = songnum;
				}
			}
			else if ((songnum > SONG_ENDGAME) && (songnum + 1 <= num_tracks))
			{
				if (RBAPlayTracks(songnum + 1, songnum + 1, repeat ? redbook_repeat_func : NULL))
				{
					Redbook_playing = songnum + 1;
					Song_playing = songnum;
				}
			}
#elif defined(DXX_BUILD_DESCENT_II)
			//Song_playing = -1;		// keep playing current music if chosen song is unavailable (e.g. SONG_ENDLEVEL)
			if ((songnum == SONG_TITLE) && (REDBOOK_TITLE_TRACK <= num_tracks))
			{
				if (RBAPlayTracks(REDBOOK_TITLE_TRACK, REDBOOK_TITLE_TRACK, repeat ? play_credits_track : NULL))
				{
					Redbook_playing = REDBOOK_TITLE_TRACK;
					Song_playing = songnum;
				}
			}
			else if ((songnum == SONG_CREDITS) && (REDBOOK_CREDITS_TRACK <= num_tracks))
			{
				if (RBAPlayTracks(REDBOOK_CREDITS_TRACK, REDBOOK_CREDITS_TRACK, repeat ? play_credits_track : NULL))
				{
					Redbook_playing = REDBOOK_CREDITS_TRACK;
					Song_playing = songnum;
				}
			}
#endif
			break;
		}
#endif
#if DXX_USE_SDLMIXER
		case music_type::Custom:
		{
			// EXCEPTION: If SONG_ENDLEVEL is undefined, continue playing level song.
			if (Song_playing >= SONG_FIRST_LEVEL_SONG && songnum == SONG_ENDLEVEL && !CGameCfg.CMMiscMusic[songnum].front())
				return Song_playing;

			Song_playing = -1;
#if defined(DXX_BUILD_DESCENT_I)
			int play = songs_play_file(CGameCfg.CMMiscMusic[songnum].data(), repeat, NULL);
#elif defined(DXX_BUILD_DESCENT_II)
			int use_credits_track = (songnum == SONG_TITLE && CGameCfg.OrigTrackOrder);
			int play = songs_play_file(CGameCfg.CMMiscMusic[songnum].data(),
							  // Play the credits track after the title track and loop the credits track if original CD track order was chosen
							  use_credits_track ? 0 : repeat,
							  use_credits_track ? play_credits_track : NULL);
#endif
			if (play)
				Song_playing = songnum;
			break;
		}
#endif
		default:
			Song_playing = -1;
			break;
	}

	return Song_playing;
}
}

#if DXX_USE_SDL_REDBOOK_AUDIO
namespace {
static void redbook_first_song_func()
{
	pause_game_world_time p;
	Song_playing = -1; // Playing Redbook tracks will not modify Song_playing. To repeat we must reset this so songs_play_level_song does not think we want to re-play the same song again.
	songs_play_level_song(1, 0);
}
}
#endif

// play track given by levelnum (depending on the music type and it's playing behaviour) or increment/decrement current track number via offset value
namespace dsx {
int songs_play_level_song( int levelnum, int offset )
{
	int songnum;

	Assert( levelnum != 0 );

	songs_init();
	songnum = (levelnum>0)?(levelnum-1):(-levelnum);

	switch (CGameCfg.MusicType)
	{
		case music_type::Builtin:
		{
			if (offset)
				return Song_playing;

			Song_playing = -1;
			/* count_level_songs excludes songs assigned to non-levels,
			 * such as SONG_TITLE, SONG_BRIEFING, etc.
			 */
			int count_level_songs;
			if (levelnum < 0 && BIMSecretSongs)
			{
				/* Secret songs are processed separately and do not need
				 * to exclude non-levels.
				 */
				const int secretsongnum = (-levelnum - 1) % BIMSecretSongs.size();
				if (songs_play_file(std::data(BIMSecretSongs[secretsongnum].filename), 1, nullptr))
					Song_playing = secretsongnum;
			}
			else if (BIMSongs && (count_level_songs = BIMSongs.size() - SONG_FIRST_LEVEL_SONG) > 0)
			{
				songnum = SONG_FIRST_LEVEL_SONG + (songnum % count_level_songs);
				if (songs_play_file(std::data(BIMSongs[songnum].filename), 1, nullptr))
					Song_playing = songnum;
			}
			break;
		}
#if DXX_USE_SDL_REDBOOK_AUDIO
		case music_type::Redbook:
		{
			int n_tracks = RBAGetNumberOfTracks();
			int tracknum;

			if (!offset)
			{
				// we have just been told to play the same as we do already -> ignore
				if (Song_playing >= SONG_FIRST_LEVEL_SONG && songnum + SONG_FIRST_LEVEL_SONG == Song_playing)
					return Song_playing;

#if defined(DXX_BUILD_DESCENT_I)
				int bn_tracks = n_tracks;
#elif defined(DXX_BUILD_DESCENT_II)
				int bn_tracks = n_tracks + 1;
#endif
				tracknum = REDBOOK_FIRST_LEVEL_TRACK + ((bn_tracks<=REDBOOK_FIRST_LEVEL_TRACK) ? 0 : (songnum % (bn_tracks-REDBOOK_FIRST_LEVEL_TRACK)));
			}
			else
			{
				tracknum = Redbook_playing+offset;
				if (tracknum < REDBOOK_FIRST_LEVEL_TRACK)
					tracknum = n_tracks - (REDBOOK_FIRST_LEVEL_TRACK - tracknum) + 1;
				else if (tracknum > n_tracks)
					tracknum = REDBOOK_FIRST_LEVEL_TRACK + (tracknum - n_tracks) - 1;
			}

			Song_playing = -1;
			if (RBAEnabled() && (tracknum <= n_tracks))
			{
#if defined(DXX_BUILD_DESCENT_I)
				int have_cd = songs_have_cd();
				int play = RBAPlayTracks(tracknum, !have_cd?n_tracks:tracknum, have_cd ? redbook_repeat_func : redbook_first_song_func);
#elif defined(DXX_BUILD_DESCENT_II)
				int play = RBAPlayTracks(tracknum, n_tracks, redbook_first_song_func);
#endif
				if (play)
				{
					Song_playing = songnum + SONG_FIRST_LEVEL_SONG;
					Redbook_playing = tracknum;
				}
			}
			break;
		}
#endif
#if DXX_USE_SDLMIXER
		case music_type::Custom:
		{
			if (CGameCfg.CMLevelMusicPlayOrder == LevelMusicPlayOrder::Random)
				CGameCfg.CMLevelMusicTrack[0] = d_rand() % CGameCfg.CMLevelMusicTrack[1]; // simply a random selection - no check if this song has already been played. But that's how I roll!
			else if (!offset)
			{
				if (CGameCfg.CMLevelMusicPlayOrder == LevelMusicPlayOrder::Continuous)
				{
					static int last_songnum = -1;

					if (Song_playing >= SONG_FIRST_LEVEL_SONG)
						return Song_playing;

					// As soon as we start a new level, go to next track
					if (last_songnum != -1 && songnum != last_songnum)
						((CGameCfg.CMLevelMusicTrack[0] + 1 >= CGameCfg.CMLevelMusicTrack[1])
							? CGameCfg.CMLevelMusicTrack[0] = 0
							: CGameCfg.CMLevelMusicTrack[0]++);
					last_songnum = songnum;
				}
				else if (CGameCfg.CMLevelMusicPlayOrder == LevelMusicPlayOrder::Level)
					CGameCfg.CMLevelMusicTrack[0] = (songnum % CGameCfg.CMLevelMusicTrack[1]);
			}
			else
			{
				CGameCfg.CMLevelMusicTrack[0] += offset;
				if (CGameCfg.CMLevelMusicTrack[0] < 0)
					CGameCfg.CMLevelMusicTrack[0] = CGameCfg.CMLevelMusicTrack[1] + CGameCfg.CMLevelMusicTrack[0];
				if (CGameCfg.CMLevelMusicTrack[0] + 1 > CGameCfg.CMLevelMusicTrack[1])
					CGameCfg.CMLevelMusicTrack[0] = CGameCfg.CMLevelMusicTrack[0] - CGameCfg.CMLevelMusicTrack[1];
			}

			Song_playing = -1;
			if (jukebox_play())
				Song_playing = songnum + SONG_FIRST_LEVEL_SONG;

			break;
		}
#endif
		default:
			Song_playing = -1;
			break;
	}

#if defined(DXX_BUILD_DESCENT_I)
	// If we couldn't play the song, most likely because it wasn't specified, play no music.
	if (Song_playing == -1)
		songs_stop_all();
#endif
	return Song_playing;
}
}

// check which song is playing, or -1 if not playing anything
int songs_is_playing()
{
	return Song_playing;
}

