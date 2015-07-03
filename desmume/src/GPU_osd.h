/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2012 DeSmuME team

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

#ifndef __GPU_OSD_
#define __GPU_OSD_

#include "types.h"

void DrawHUD();

class OSDCLASS {
public:
  OSDCLASS(u8 core);
  ~OSDCLASS();
  void    update();
  void    clear();
  void    setLineColor(u8 r, u8 b, u8 g);
  void    addLine(const char *fmt, ...);
};

extern OSDCLASS        *osd;
#endif
