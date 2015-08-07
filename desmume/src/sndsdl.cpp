/*
	Copyright (C) 2005-2006 Theo Berkau
	Copyright (C) 2006-2010 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libretro.h>

#include "types.h"
#include "SPU.h"
#include "sndsdl.h"
#include "debug.h"

int SNDRetroInit(int buffersize);
void SNDRetroDeInit();
void SNDRetroUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDRetroGetAudioSpace();
void SNDRetroMuteAudio();
void SNDRetroUnMuteAudio();
void SNDRetroSetVolume(int volume);

extern retro_audio_sample_batch_t audio_batch_cb;

SoundInterface_struct SNDRetro = {
SNDCORE_RETRO,
"SDL Sound Interface",
SNDRetroInit,
SNDRetroDeInit,
SNDRetroUpdateAudio,
SNDRetroGetAudioSpace,
SNDRetroMuteAudio,
SNDRetroUnMuteAudio,
SNDRetroSetVolume,
};

unsigned retro_audio_frames;

int SNDRetroInit(int buffersize) { return 0; }

void SNDRetroDeInit() {}

void SNDRetroUpdateAudio(s16 *buffer, u32 num_samples)
{
   if (audio_batch_cb)
      audio_batch_cb(buffer, num_samples);
   retro_audio_frames += num_samples;
}

u32 SNDRetroGetAudioSpace() { return 735 - retro_audio_frames; }
void SNDRetroMuteAudio() {}
void SNDRetroUnMuteAudio() {}
void SNDRetroSetVolume(int volume) {}
