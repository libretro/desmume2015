/*
	Copyright 2008-2015 DeSmuME team

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

#include "movie.h"

#include <assert.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#include "MMU.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "debug.h"
#include "driver.h"
#include "rtc.h"
#include "common.h"
#include "mic.h"
#include "version.h"
#include "GPU_osd.h"
#include "path.h"
#include "emufile.h"

int currFrameCounter;
