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
		: mixqueue_go(false)
		,
		#ifdef NDEBUG
		adjustobuf(200,1000)
		#else
		adjustobuf(22000,44000)
		#endif
	{

	}

	bool mixqueue_go;

	virtual void enqueue_samples(s16* buf, int samples_provided)
	{
		for(int i=0;i<samples_provided;i++) {
			s16 left = *buf++;
			s16 right = *buf++;
			adjustobuf.enqueue(left,right);
		}
	}

	//returns the number of samples actually supplied, which may not match the number requested
	virtual int output_samples(s16* buf, int samples_requested)
	{
		int done = 0;
		if(!mixqueue_go) {
			if(adjustobuf.size > 200)
				mixqueue_go = true;
		}
		else
		{
			for(int i=0;i<samples_requested;i++) {
				if(adjustobuf.size==0) {
					mixqueue_go = false;
					break;
				}
				done++;
				s16 left, right;
				adjustobuf.dequeue(left,right);
				*buf++ = left;
				*buf++ = right;
			}
		}
		
		return done;
	}

private:
	class Adjustobuf
	{
	public:
		Adjustobuf(int _minLatency, int _maxLatency)
			: size(0)
			, minLatency(_minLatency)
			, maxLatency(_maxLatency)
		{
			rollingTotalSize = 0;
			targetLatency = (maxLatency + minLatency)/2;
			rate = 1.0f;
			cursor = 0.0f;
			curr[0] = curr[1] = 0;
			kAverageSize = 80000;
		}

		float rate, cursor;
		int minLatency, targetLatency, maxLatency;
		std::queue<s16> buffer;
		int size;
		s16 curr[2];

		std::queue<int> statsHistory;

		void enqueue(s16 left, s16 right) 
		{
			buffer.push(left);
			buffer.push(right);
			size++;
		}

		s64 rollingTotalSize;

		u32 kAverageSize;

		void addStatistic()
		{
			statsHistory.push(size);
			rollingTotalSize += size;
			if(statsHistory.size()>kAverageSize)
			{
				rollingTotalSize -= statsHistory.front();
				statsHistory.pop();

				float averageSize = (float)(rollingTotalSize / kAverageSize);
				//static int ctr=0;  ctr++; if((ctr&127)==0) printf("avg size: %f curr size: %d rate: %f\n",averageSize,size,rate);
				{
					float targetRate;
					if(averageSize < targetLatency)
					{
						targetRate = 1.0f - (targetLatency-averageSize)/kAverageSize;
					}
					else if(averageSize > targetLatency) {
						targetRate = 1.0f + (averageSize-targetLatency)/kAverageSize;
					} else targetRate = 1.0f;
				
					rate = targetRate;
				}

			}


		}

		void dequeue(s16& left, s16& right)
		{
			left = right = 0; 
			addStatistic();
			if(size==0) { return; }
			cursor += rate;
			while(cursor>1.0f) {
				cursor -= 1.0f;
				if(size>0) {
					curr[0] = buffer.front(); buffer.pop();
					curr[1] = buffer.front(); buffer.pop();
					size--;
				}
			}
			left = curr[0]; 
			right = curr[1];
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
