/*
	Copyright (C) 2008-2015 DeSmuME team

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

//TODO - dismantle this file

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <memalign.h>

#if defined(WIN32)
	#define CLASSNAME "DeSmuME"

	#ifdef __GNUC__
	#define sscanf_s sscanf
	#endif
#else		// non Windows
	#define sscanf_s sscanf
#endif

#ifdef __cplusplus
extern "C" {
#endif

static INLINE char *integer_to_binary(uint32_t val, size_t len)
{
   unsigned i, t;
	char buf[256] = {0};

	for (i = len * 8, t = 0;  i > 0; --i, t++)
		buf[i-1] = (val & (1<<t))?'1':'0';

	return strdup(buf);
}

extern char *removeSpecialChars(char *s);

// ===============================================================================
// Message dialogs
// ===============================================================================
#define CALL_CONVENTION
typedef struct
{
	void (CALL_CONVENTION*  info)	(const char *fmt, ...);
	bool (CALL_CONVENTION*  confirm)(const char *fmt, ...);
	void (CALL_CONVENTION*  error)	(const char *fmt, ...);
	void (CALL_CONVENTION*  warn)	(const char *fmt, ...);
} msgBoxInterface;

extern msgBoxInterface *msgbox;

// ===============================================================================
// Maker codes
// ===============================================================================

struct MAKER
{
	uint16_t code;
	const char *name;
};

const char *getDeveloperNameByID(uint16_t id);

#ifdef __cplusplus
}
#endif

#endif
