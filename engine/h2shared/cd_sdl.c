/*
	cd_sdl.c
	$Id$

	Copyright (C) 1996-1997  Id Software, Inc.
	Taken from the Twilight project with modifications
	to make it work with Hexen II: Hammer of Thyrion.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		51 Franklin St, Fifth Floor,
		Boston, MA  02110-1301  USA
*/


#include "cd_unix.h"

#ifdef	__USE_SDL_CDROM__

#include "sdl_inc.h"

#ifndef	SDL_INIT_CDROM

/* SDL dropped support for
   cd audio since v1.3.0 */
#warning SDL CDAudio support disabled
#include "cd_null.c"

#else	/* SDL_INIT_CDROM */

#include "quakedef.h"
#include "cdaudio.h"

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	enabled = true;
static qboolean playLooping = false;
static byte	remap[100];
static byte	playTrack;
static SDL_CD	*cd_handle;
static int	cd_dev = -1;
static float	old_cdvolume;
static qboolean	hw_vol_works = true;


static void CDAudio_Eject(void)
{
	if (!cd_handle || !enabled)
		return;

	if (SDL_CDEject(cd_handle) == -1)
		Con_Printf ("Unable to eject CD-ROM: %s\n", SDL_GetError ());
}

static int CDAudio_GetAudioDiskInfo(void)
{
	cdValid = false;

	if (!cd_handle)
		return -1;

	if ( ! CD_INDRIVE(SDL_CDStatus(cd_handle)) )
		return -1;

	cdValid = true;

	return 0;
}

int CDAudio_Play(byte track, qboolean looping)
{
	if (!cd_handle || !enabled)
		return -1;

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return -1;
	}

	track = remap[track];

	if (track < 1 || track > cd_handle->numtracks)
	{
		Con_Printf ("%s: Bad track number %d.\n", __thisfunc__, track);
		return -1;
	}

	if (cd_handle->track[track-1].type == SDL_DATA_TRACK)
	{
		Con_Printf ("%s: track %d is not audio\n", __thisfunc__, track);
		return -1;
	}

	if (playing)
	{
		if (playTrack == track)
			return 0;
		CDAudio_Stop();
	}

	if (SDL_CDPlay(cd_handle, cd_handle->track[track-1].offset, cd_handle->track[track-1].length) == -1)
	{
		int cd_status = SDL_CDStatus(cd_handle);
		if (cd_status > 0)
			Con_Printf ("%s: Unable to play %d: %s\n", __thisfunc__, track, SDL_GetError ());
		return -1;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (bgmvolume.value == 0) /* don't bother advancing */
		CDAudio_Pause ();

	return 0;
}

void CDAudio_Stop(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDStop(cd_handle) == -1)
		Con_Printf ("%s: Unable to stop CD-ROM (%s)\n", __thisfunc__, SDL_GetError());

	wasPlaying = false;
	playing = false;
}

void CDAudio_Pause(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDPause(cd_handle) == -1)
		Con_Printf ("Unable to pause CD-ROM: %s\n", SDL_GetError());

	wasPlaying = playing;
	playing = false;
}

void CDAudio_Resume(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (SDL_CDResume(cd_handle) == -1)
		Con_Printf ("Unable to resume CD-ROM: %s\n", SDL_GetError());
	playing = true;
}

static void CD_f (void)
{
	const char	*command;
	int		ret, n;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands:");
		Con_Printf("on, off, reset, remap, \n");
		Con_Printf("play, stop, loop, pause, resume\n");
		Con_Printf("eject, info\n");
		return;
	}

	command = Cmd_Argv (1);

	if (q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv (n + 1));
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), false);
		return;
	}

	if (q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), true);
		return;
	}

	if (q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (q_strcasecmp(command, "info") == 0)
	{
		int	current_min, current_sec, current_frame;
		int	length_min, length_sec, length_frame;

		Con_Printf ("%u tracks\n", cd_handle->numtracks);

		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);

		if (playing || wasPlaying)
		{
			SDL_CDStatus(cd_handle);
			FRAMES_TO_MSF(cd_handle->cur_frame, &current_min, &current_sec, &current_frame);
			FRAMES_TO_MSF(cd_handle->track[playTrack-1].length, &length_min, &length_sec, &length_frame);

			Con_Printf ("Current position: %d:%02d.%02d (of %d:%02d.%02d)\n",
						current_min, current_sec, current_frame * 60 / CD_FPS,
						length_min, length_sec, length_frame * 60 / CD_FPS);
		}
		Con_Printf("Volume is %f\n", bgmvolume.value);

		return;
	}
}

static qboolean CD_GetVolume (void *unused)
{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
	return false;
}

static qboolean CD_SetVolume (void *unused)
{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
	return false;
}

static qboolean CDAudio_SetVolume (float value)
{
	if (!cd_handle || !enabled)
		return false;

	old_cdvolume = value;

	if (value == 0.0f)
		CDAudio_Pause ();
	else
		CDAudio_Resume();

	if (!hw_vol_works)
	{
		return false;
	}
	else
	{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
		return CD_SetVolume (NULL);
	}
}

void CDAudio_Update(void)
{
	CDstatus	curstat;
	Uint32		timechk;
	static Uint32	lastchk;

	if (!cd_handle || !enabled)
		return;

	if (old_cdvolume != bgmvolume.value)
		CDAudio_SetVolume (bgmvolume.value);

	timechk = SDL_GetTicks ();
	if (playing && lastchk < timechk)
	{
		lastchk = timechk + 2000; /* two seconds between chks */
		curstat = SDL_CDStatus(cd_handle);
		if (curstat != CD_PLAYING && curstat != CD_PAUSED)
		{
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
	}
}

static const char *get_cddev_arg (const char *arg)
{
#if defined(_WIN32)
/* arg should be like "D:\", make sure it is so,
 * but tolerate args like "D" or "D:", as well. */
	static char drive[4];
	if (!arg || ! *arg)
		return NULL;
	if (arg[1] != '\0')
	{
		if (arg[1] != ':')
			return NULL;
		if (arg[2] != '\0')
		{
			if (arg[2] != '\\' &&
			    arg[2] != '/')
				return NULL;
			if (arg[3] != '\0')
				return NULL;
		}
	}
	if (*arg >= 'A' && *arg <= 'Z')
	{
		drive[0] = *arg;
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
		return drive;
	}
	else if (*arg >= 'a' && *arg <= 'z')
	{
	/* make it uppercase for SDL */
		drive[0] = *arg - ('a' - 'A');
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
		return drive;
	}
	return NULL;
#else
	if (!arg || ! *arg)
		return NULL;
	return arg;
#endif
}

static void export_cddev_arg (void)
{
/* Bad ugly hack to workaround SDL's cdrom device detection.
 * not needed for windows due to the way SDL_cdrom works. */
#if !defined(_WIN32)
	int i = COM_CheckParm("-cddev");
	if (i != 0 && i < com_argc - 1 && com_argv[i+1][0] != '\0')
	{
		static char arg[64];
		q_snprintf(arg, sizeof(arg), "SDL_CDROM=%s", com_argv[i+1]);
		putenv(arg);
	}
#endif
}

int CDAudio_Init(void)
{
	int	i, sdl_num_drives;

	if (safemode || COM_CheckParm("-nocdaudio"))
		return -1;

	export_cddev_arg();

	if (SDL_InitSubSystem(SDL_INIT_CDROM) == -1)
	{
		Con_Printf("Couldn't init SDL cdrom: %s\n", SDL_GetError());
		return -1;
	}

	sdl_num_drives = SDL_CDNumDrives ();
	Con_Printf ("SDL detected %d CD-ROM drive%c\n", sdl_num_drives,
					sdl_num_drives == 1 ? ' ' : 's');

	if (sdl_num_drives < 1)
		return -1;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
	{
		const char *userdev = get_cddev_arg(com_argv[i+1]);
		if (!userdev)
		{
			Con_Printf("Invalid argument to -cddev\n");
			return -1;
		}
		for (i = 0; i < sdl_num_drives; i++)
		{
			if (!q_strcasecmp(SDL_CDName(i), userdev))
			{
				cd_dev = i;
				break;
			}
		}
		if (cd_dev == -1)
		{
			Con_Printf("SDL couldn't find cdrom device %s\n", userdev);
			return -1;
		}
	}

	if (cd_dev == -1)
		cd_dev = 0;	/* default drive */

	cd_handle = SDL_CDOpen(cd_dev);
	if (!cd_handle)
	{
		Con_Printf ("%s: Unable to open CD-ROM drive %s (%s)\n",
				__thisfunc__, SDL_CDName(cd_dev), SDL_GetError());
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	enabled = true;
	old_cdvolume = bgmvolume.value;

	Con_Printf("CDAudio initialized (SDL, using %s)\n", SDL_CDName(cd_dev));

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("%s: No CD in drive\n", __thisfunc__);
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	hw_vol_works = CD_GetVolume (NULL); /* no SDL support at present. */
	if (hw_vol_works)
		hw_vol_works = CDAudio_SetVolume (bgmvolume.value);

	return 0;
}

void CDAudio_Shutdown(void)
{
	if (!cd_handle)
		return;
	CDAudio_Stop();
	if (hw_vol_works)
		CD_SetVolume (NULL); /* no SDL support at present. */
	SDL_CDClose(cd_handle);
	cd_handle = NULL;
	cd_dev = -1;
	SDL_QuitSubSystem(SDL_INIT_CDROM);
}

#endif	/* SDL_INIT_CDROM */

#endif	/* __USE_SDL_CDROM__ */

