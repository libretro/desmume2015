/*  Copyright 2009-2015 DeSmuME team

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

//-------------------------
//this file contains the METASPU system
//which is designed to handle the task of audio synchronization
//and is designed to be as portable between multiple emulators
//-------------------------


#ifndef _METASPU_H_
#define _METASPU_H_

#include <algorithm>
#include <queue>
#include <assert.h>

#include "types.h"

class ISynchronizingAudioBuffer
{
public:
	ISynchronizingAudioBuffer()
		: mixqueue_go(false), adjustobuf()
	{

	}

	bool mixqueue_go;

	void enqueue_samples(s16* buf, int samples_provided)
	{
		for(int i=0;i<samples_provided;i++) {
			s16 left = *buf++;
			s16 right = *buf++;
			adjustobuf.enqueue(left,right);
		}
	}

	//returns the number of samples actually supplied, which may not match the number requested
	int output_samples(s16* buf, int samples_requested)
	{
		int done = 0;
      for(int i=0;i<samples_requested;i++)
      {
         done++;
         s16 left, right;
         adjustobuf.dequeue(left,right);
         *buf++ = left;
         *buf++ = right;
      }
		
		return done;
	}

private:
	class Adjustobuf
	{
	public:
		Adjustobuf()
			: size(0)
		{
		}

		std::queue<s16> buffer;
		int size;

		void enqueue(s16 left, s16 right) 
		{
			buffer.push(left);
			buffer.push(right);
			size++;
		}

		void dequeue(s16& left, s16& right)
		{
			left = right = 0; 
			if(size==0)
            return;
         left = buffer.front(); buffer.pop();
         right = buffer.front(); buffer.pop();
         size--;
		}
	} adjustobuf;
};

enum ESynchMethod
{
	ESynchMethod_N, //nitsuja's
	ESynchMethod_Z, //zero's
};

ISynchronizingAudioBuffer* metaspu_construct(ESynchMethod method);

#endif
