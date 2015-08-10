/*
	Copyright (C) 2007 Guillaume Duhamel
	Copyright (C) 2007 DeSmuME team

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
#ifndef _NDS_ROMREADER_H
#define _NDS_ROMREADER_H

#include <stdint.h>
#include <string.h>

#define ROMREADER_DEFAULT -1
#define ROMREADER_STD	0
#define ROMREADER_GZIP	1
#define ROMREADER_ZIP	2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int id;
	const char * Name;
	void * (*Init)(const char * filename);
	void (*DeInit)(void * file);
	uint32_t (*Size)(void * file);
	int (*Seek)(void * file, int offset, int whence);
	int (*Read)(void * file, void * buffer, uint32_t size);
} ROMReader_struct;

extern ROMReader_struct STDROMReader;

ROMReader_struct * ROMReaderInit(char ** filename);

#ifdef __cplusplus
}
#endif

#endif
