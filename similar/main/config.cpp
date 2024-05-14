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
 * contains routine(s) to read in the configuration file which contains
 * game configuration stuff like detail level, sound card, etc
 *
 */

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "pstypes.h"
#include "game.h"
#include "songs.h"
#include "kconfig.h"
#include "palette.h"
#include "digi.h"
#include "mission.h"
#include "u_mem.h"
#include "physfsx.h"
#include "nvparse.h"
#if DXX_USE_OGL
#include "ogl_init.h"
#endif
#include <memory>

namespace dcx {
CCfg CGameCfg;
}

namespace dsx {
Cfg GameCfg;

#define DigiVolumeStr "DigiVolume"
#define MusicVolumeStr "MusicVolume"
#define ReverseStereoStr "ReverseStereo"
#define OrigTrackOrderStr "OrigTrackOrder"
#define MusicTypeStr "MusicType"
#define CMLevelMusicPlayOrderStr "CMLevelMusicPlayOrder"
#define CMLevelMusicTrack0Str "CMLevelMusicTrack0"
#define CMLevelMusicTrack1Str "CMLevelMusicTrack1"
#define CMLevelMusicPathStr "CMLevelMusicPath"
#define CMMiscMusic0Str "CMMiscMusic0"
#define CMMiscMusic1Str "CMMiscMusic1"
#define CMMiscMusic2Str "CMMiscMusic2"
#define CMMiscMusic3Str "CMMiscMusic3"
#define CMMiscMusic4Str "CMMiscMusic4"
#define GammaLevelStr "GammaLevel"
#define LastPlayerStr "LastPlayer"
#define LastMissionStr "LastMission"
#define ResolutionXStr "ResolutionX"
#define ResolutionYStr "ResolutionY"
#define AspectXStr "AspectX"
#define AspectYStr "AspectY"
#define WindowModeStr "WindowMode"
#define TexFiltStr "TexFilt"
#define TexAnisStr "TexAnisotropy"
#if defined(DXX_BUILD_DESCENT_II)
#define MovieTexFiltStr "MovieTexFilt"
#define MovieSubtitlesStr "MovieSubtitles"
#endif
#if DXX_USE_ADLMIDI
#define ADLMIDINumChipsStr	"ADLMIDI_NumberOfChips"
#define ADLMIDIBankStr	"ADLMIDI_Bank"
#define ADLMIDIEnabledStr	"ADLMIDI_Enabled"
#endif
#define VSyncStr "VSync"
#define MultisampleStr "Multisample"
#define FPSIndicatorStr "FPSIndicator"
#define GrabinputStr "GrabInput"

int ReadConfigFile()
{
	// set defaults
	CGameCfg.ADLMIDI_enabled = 1;
	CGameCfg.DigiVolume = 8;
	CGameCfg.MusicVolume = 8;
	CGameCfg.ReverseStereo = false;
	CGameCfg.OrigTrackOrder = false;
#if DXX_USE_SDL_REDBOOK_AUDIO && defined(__APPLE__) && defined(__MACH__)
	CGameCfg.MusicType = music_type::Redbook;
#else
	CGameCfg.MusicType = music_type::Builtin;
#endif
	CGameCfg.CMLevelMusicPlayOrder = LevelMusicPlayOrder::Continuous;
	CGameCfg.CMLevelMusicTrack[0] = -1;
	CGameCfg.CMLevelMusicTrack[1] = -1;
	CGameCfg.CMLevelMusicPath = {};
	CGameCfg.CMMiscMusic = {};
#if defined(__APPLE__) && defined(__MACH__)
	CGameCfg.OrigTrackOrder = true;
	const auto userdir = PHYSFS_getUserDir();
#if defined(DXX_BUILD_DESCENT_I)
	CGameCfg.CMLevelMusicPlayOrder = LevelMusicPlayOrder::Level;
	CGameCfg.CMLevelMusicPath = "descent.m3u";
	snprintf(CGameCfg.CMMiscMusic[SONG_TITLE].data(), CGameCfg.CMMiscMusic[SONG_TITLE].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Insanity/Descent/02 Primitive Rage.mp3");
	snprintf(CGameCfg.CMMiscMusic[SONG_CREDITS].data(), CGameCfg.CMMiscMusic[SONG_CREDITS].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Insanity/Descent/05 The Darkness Of Space.mp3");
#elif defined(DXX_BUILD_DESCENT_II)
	CGameCfg.CMLevelMusicPath = "descent2.m3u";
	snprintf(CGameCfg.CMMiscMusic[SONG_TITLE].data(), CGameCfg.CMMiscMusic[SONG_TITLE].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Redbook Soundtrack/Descent II, Macintosh CD-ROM/02 Title.mp3");
	snprintf(CGameCfg.CMMiscMusic[SONG_CREDITS].data(), CGameCfg.CMMiscMusic[SONG_CREDITS].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Redbook Soundtrack/Descent II, Macintosh CD-ROM/03 Crawl.mp3");
#endif
	snprintf(CGameCfg.CMMiscMusic[SONG_BRIEFING].data(), CGameCfg.CMMiscMusic[SONG_BRIEFING].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Insanity/Descent/03 Outerlimits.mp3");
	snprintf(CGameCfg.CMMiscMusic[SONG_ENDLEVEL].data(), CGameCfg.CMMiscMusic[SONG_ENDLEVEL].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Insanity/Descent/04 Close Call.mp3");
	snprintf(CGameCfg.CMMiscMusic[SONG_ENDGAME].data(), CGameCfg.CMMiscMusic[SONG_ENDGAME].size(), "%s%s", userdir, "Music/iTunes/iTunes Music/Insanity/Descent/14 Insanity.mp3");
#endif
	CGameCfg.GammaLevel = 0;
	GameCfg.LastPlayer = {};
	CGameCfg.LastMission = "";
	CGameCfg.ResolutionX = 1024;
	CGameCfg.ResolutionY = 768;
	CGameCfg.AspectX = 3;
	CGameCfg.AspectY = 4;
	CGameCfg.WindowMode = false;
#if DXX_USE_OGL
	CGameCfg.TexFilt = opengl_texture_filter::classic;
#endif
	CGameCfg.TexAnisotropy = 0;
#if defined(DXX_BUILD_DESCENT_II)
	GameCfg.MovieTexFilt = 0;
	GameCfg.MovieSubtitles = 0;
#endif
	CGameCfg.VSync = false;
	CGameCfg.Multisample = 0;
	CGameCfg.FPSIndicator = 0;
	CGameCfg.Grabinput = true;


	auto infile = PHYSFSX_openReadBuffered("descent.cfg").first;
	if (!infile)
	{
		return 1;
	}

	// to be fully safe, assume the whole cfg consists of one big line
	for (PHYSFSX_gets_line_t<0> line(PHYSFS_fileLength(infile) + 1); const auto rc{PHYSFSX_fgets(line, infile)};)
	{
		const auto eol{rc.end()};
		const auto lb{rc.begin()};
		const auto eq{std::ranges::find(lb, eol, '=')};
		if (eq == eol)
			continue;
		const auto value{std::next(eq)};
		if (const std::ranges::subrange name{lb, eq}; compare_nonterminated_name(name, DigiVolumeStr))
		{
			if (const auto r = convert_integer<uint8_t>(value))
				if (const auto v = *r; v < 8)
					CGameCfg.DigiVolume = v;
		}
		else if (compare_nonterminated_name(name, MusicVolumeStr))
		{
			if (const auto r = convert_integer<uint8_t>(value))
				if (const auto v = *r; v < 8)
					CGameCfg.MusicVolume = v;
		}
		else if (compare_nonterminated_name(name, ReverseStereoStr))
			convert_integer(CGameCfg.ReverseStereo, value);
		else if (compare_nonterminated_name(name, OrigTrackOrderStr))
			convert_integer(CGameCfg.OrigTrackOrder, value);
		else if (compare_nonterminated_name(name, MusicTypeStr))
		{
			if (const auto r = convert_integer<uint8_t>(value))
				switch (const music_type v{*r})
				{
					case music_type::None:
					case music_type::Builtin:
#if DXX_USE_SDL_REDBOOK_AUDIO
					case music_type::Redbook:
#endif
					case music_type::Custom:
						CGameCfg.MusicType = v;
						break;
				}
		}
		else if (compare_nonterminated_name(name, CMLevelMusicPlayOrderStr))
		{
			if (auto r = convert_integer<uint8_t>(value))
				if (auto CMLevelMusicPlayOrder = *r; CMLevelMusicPlayOrder <= static_cast<uint8_t>(LevelMusicPlayOrder::Random))
					CGameCfg.CMLevelMusicPlayOrder = LevelMusicPlayOrder{CMLevelMusicPlayOrder};
		}
		else if (compare_nonterminated_name(name, CMLevelMusicTrack0Str))
			convert_integer(CGameCfg.CMLevelMusicTrack[0], value);
		else if (compare_nonterminated_name(name, CMLevelMusicTrack1Str))
			convert_integer(CGameCfg.CMLevelMusicTrack[1], value);
		else if (compare_nonterminated_name(name, CMLevelMusicPathStr))
			convert_string(CGameCfg.CMLevelMusicPath, value, eol);
		else if (compare_nonterminated_name(name, CMMiscMusic0Str))
			convert_string(CGameCfg.CMMiscMusic[SONG_TITLE], value, eol);
		else if (compare_nonterminated_name(name, CMMiscMusic1Str))
			convert_string(CGameCfg.CMMiscMusic[SONG_BRIEFING], value, eol);
		else if (compare_nonterminated_name(name, CMMiscMusic2Str))
			convert_string(CGameCfg.CMMiscMusic[SONG_ENDLEVEL], value, eol);
		else if (compare_nonterminated_name(name, CMMiscMusic3Str))
			convert_string(CGameCfg.CMMiscMusic[SONG_ENDGAME], value, eol);
		else if (compare_nonterminated_name(name, CMMiscMusic4Str))
			convert_string(CGameCfg.CMMiscMusic[SONG_CREDITS], value, eol);
		else if (compare_nonterminated_name(name, GammaLevelStr))
		{
			convert_integer(CGameCfg.GammaLevel, value);
			gr_palette_set_gamma(CGameCfg.GammaLevel);
		}
		else if (compare_nonterminated_name(name, LastPlayerStr))
			GameCfg.LastPlayer.copy_lower(std::span(value, std::distance(value, eol)));
		else if (compare_nonterminated_name(name, LastMissionStr))
			convert_string(CGameCfg.LastMission, value, eol);
		else if (compare_nonterminated_name(name, ResolutionXStr))
			convert_integer(CGameCfg.ResolutionX, value);
		else if (compare_nonterminated_name(name, ResolutionYStr))
			convert_integer(CGameCfg.ResolutionY, value);
		else if (compare_nonterminated_name(name, AspectXStr))
			convert_integer(CGameCfg.AspectX, value);
		else if (compare_nonterminated_name(name, AspectYStr))
			convert_integer(CGameCfg.AspectY, value);
		else if (compare_nonterminated_name(name, WindowModeStr))
			convert_integer(CGameCfg.WindowMode, value);
		else if (compare_nonterminated_name(name, TexFiltStr))
		{
			if (auto r = convert_integer<uint8_t>(value))
			{
				switch (const auto TexFilt = *r)
				{
#if DXX_USE_OGL
					case static_cast<unsigned>(opengl_texture_filter::classic):
					case static_cast<unsigned>(opengl_texture_filter::upscale):
					case static_cast<unsigned>(opengl_texture_filter::trilinear):
#else
					default:
						/* In SDL-only builds, accept any value and save it.
						 * The value will not be used, but it will be written
						 * back to the configuration file, to avoid deleting
						 * settings for players who use both SDL-only and
						 * OpenGL-enabled builds.
						 */
#endif
						CGameCfg.TexFilt = opengl_texture_filter{TexFilt};
						break;
				}
			}
		}
		else if (compare_nonterminated_name(name, TexAnisStr))
			convert_integer(CGameCfg.TexAnisotropy, value);
#if defined(DXX_BUILD_DESCENT_II)
		else if (compare_nonterminated_name(name, MovieTexFiltStr))
			convert_integer(GameCfg.MovieTexFilt, value);
		else if (compare_nonterminated_name(name, MovieSubtitlesStr))
			convert_integer(GameCfg.MovieSubtitles, value);
#endif
#if DXX_USE_ADLMIDI
		else if (compare_nonterminated_name(name, ADLMIDINumChipsStr))
			convert_integer(CGameCfg.ADLMIDI_num_chips, value);
		else if (compare_nonterminated_name(name, ADLMIDIBankStr))
			convert_integer(CGameCfg.ADLMIDI_bank, value);
		else if (compare_nonterminated_name(name, ADLMIDIEnabledStr))
			convert_integer(CGameCfg.ADLMIDI_enabled, value);
#endif
		else if (compare_nonterminated_name(name, VSyncStr))
			convert_integer(CGameCfg.VSync, value);
		else if (compare_nonterminated_name(name, MultisampleStr))
			convert_integer(CGameCfg.Multisample, value);
		else if (compare_nonterminated_name(name, FPSIndicatorStr))
			convert_integer(CGameCfg.FPSIndicator, value);
		else if (compare_nonterminated_name(name, GrabinputStr))
			convert_integer(CGameCfg.Grabinput, value);
	}

	if (CGameCfg.ResolutionX >= 320 && CGameCfg.ResolutionY >= 200)
	{
		Game_screen_mode.width = CGameCfg.ResolutionX;
		Game_screen_mode.height = CGameCfg.ResolutionY;
	}

	return 0;
}

int WriteConfigFile()
{
	CGameCfg.GammaLevel = gr_palette_get_gamma();

	auto infile = PHYSFSX_openWriteBuffered("descent.cfg").first;
	if (!infile)
	{
		return 1;
	}
	PHYSFSX_printf(infile, "%s=%d\n", DigiVolumeStr, CGameCfg.DigiVolume);
	PHYSFSX_printf(infile, "%s=%d\n", MusicVolumeStr, CGameCfg.MusicVolume);
	PHYSFSX_printf(infile, "%s=%d\n", ReverseStereoStr, CGameCfg.ReverseStereo);
	PHYSFSX_printf(infile, "%s=%d\n", OrigTrackOrderStr, CGameCfg.OrigTrackOrder);
	PHYSFSX_printf(infile, "%s=%d\n", MusicTypeStr, underlying_value(CGameCfg.MusicType));
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicPlayOrderStr, static_cast<int>(CGameCfg.CMLevelMusicPlayOrder));
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicTrack0Str, CGameCfg.CMLevelMusicTrack[0]);
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicTrack1Str, CGameCfg.CMLevelMusicTrack[1]);
	PHYSFSX_printf(infile, "%s=%s\n", CMLevelMusicPathStr, CGameCfg.CMLevelMusicPath.data());
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic0Str, CGameCfg.CMMiscMusic[SONG_TITLE].data());
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic1Str, CGameCfg.CMMiscMusic[SONG_BRIEFING].data());
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic2Str, CGameCfg.CMMiscMusic[SONG_ENDLEVEL].data());
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic3Str, CGameCfg.CMMiscMusic[SONG_ENDGAME].data());
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic4Str, CGameCfg.CMMiscMusic[SONG_CREDITS].data());
	PHYSFSX_printf(infile, "%s=%d\n", GammaLevelStr, CGameCfg.GammaLevel);
	PHYSFSX_printf(infile, "%s=%s\n", LastPlayerStr, static_cast<const char *>(InterfaceUniqueState.PilotName));
	PHYSFSX_printf(infile, "%s=%s\n", LastMissionStr, static_cast<const char *>(CGameCfg.LastMission));
	PHYSFSX_printf(infile, "%s=%i\n", ResolutionXStr, SM_W(Game_screen_mode));
	PHYSFSX_printf(infile, "%s=%i\n", ResolutionYStr, SM_H(Game_screen_mode));
	PHYSFSX_printf(infile, "%s=%i\n", AspectXStr, CGameCfg.AspectX);
	PHYSFSX_printf(infile, "%s=%i\n", AspectYStr, CGameCfg.AspectY);
	PHYSFSX_printf(infile, "%s=%i\n", WindowModeStr, CGameCfg.WindowMode);
	PHYSFSX_printf(infile, "%s=%i\n", TexFiltStr, underlying_value(CGameCfg.TexFilt));
	PHYSFSX_printf(infile, "%s=%i\n", TexAnisStr, CGameCfg.TexAnisotropy);
#if defined(DXX_BUILD_DESCENT_II)
	PHYSFSX_printf(infile, "%s=%i\n", MovieTexFiltStr, GameCfg.MovieTexFilt);
	PHYSFSX_printf(infile, "%s=%i\n", MovieSubtitlesStr, GameCfg.MovieSubtitles);
#endif
#if DXX_USE_ADLMIDI
	PHYSFSX_printf(infile, "%s=%i\n", ADLMIDINumChipsStr, CGameCfg.ADLMIDI_num_chips);
	PHYSFSX_printf(infile, "%s=%i\n", ADLMIDIBankStr, CGameCfg.ADLMIDI_bank);
	PHYSFSX_printf(infile, "%s=%i\n", ADLMIDIEnabledStr, CGameCfg.ADLMIDI_enabled);
#endif
	PHYSFSX_printf(infile, "%s=%i\n", VSyncStr, CGameCfg.VSync);
	PHYSFSX_printf(infile, "%s=%i\n", MultisampleStr, CGameCfg.Multisample);
	PHYSFSX_printf(infile, "%s=%i\n", FPSIndicatorStr, CGameCfg.FPSIndicator);
	PHYSFSX_printf(infile, "%s=%i\n", GrabinputStr, CGameCfg.Grabinput);
	return 0;
}

}
