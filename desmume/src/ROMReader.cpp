/*
	Copyright 2007 Guillaume Duhamel
	Copyright 2007-2012 DeSmuME team

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

#include "ROMReader.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef WIN32
#define stat(...) _stat(__VA_ARGS__)
#define S_IFMT _S_IFMT
#define S_IFREG _S_IFREG
#endif

ROMReader_struct * ROMReaderInit(char ** filename)
{
	return &STDROMReader;
}

void * STDROMReaderInit(const char * filename);
void STDROMReaderDeInit(void *);
u32 STDROMReaderSize(void *);
int STDROMReaderSeek(void *, int, int);
int STDROMReaderRead(void *, void *, u32);

ROMReader_struct STDROMReader =
{
	ROMREADER_STD,
	"Standard ROM Reader",
	STDROMReaderInit,
	STDROMReaderDeInit,
	STDROMReaderSize,
	STDROMReaderSeek,
	STDROMReaderRead
};

void * STDROMReaderInit(const char * filename)
{
#ifdef WIN32
	struct _stat sb;
#else
	struct stat sb;
#endif
	if (stat(filename, &sb) == -1)
		return 0;

 	if ((sb.st_mode & S_IFMT) != S_IFREG)
		return 0;

	return (void *) fopen(filename, "rb");
}

void STDROMReaderDeInit(void * file)
{
	if (!file) return ;
	fclose((FILE*)file);
}

u32 STDROMReaderSize(void * file)
{
	u32 size;

	if (!file) return 0 ;

	fseek((FILE*)file, 0, SEEK_END);
	size = ftell((FILE*)file);
	fseek((FILE*)file, 0, SEEK_SET);

	return size;
}

int STDROMReaderSeek(void * file, int offset, int whence)
{
	if (!file) return 0 ;
	return fseek((FILE*)file, offset, whence);
}

int STDROMReaderRead(void * file, void * buffer, u32 size)
{
	if (!file) return 0 ;
	return fread(buffer, 1, size, (FILE*)file);
}
