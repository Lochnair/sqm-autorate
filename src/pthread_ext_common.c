/*
The MIT License (MIT)

Copyright (c) 2014, Stephen Scott
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

/* 
 * pthread_queue implementation
 */

#include "pthread_ext_common.h"

/**************************************************************************************************/
void pthread_ext_ms2abs_time(long ms, struct timespec * abstime)
{
	clock_gettime(CLOCK_REALTIME, abstime);

	if (ms >= 1000)
	{
		abstime->tv_sec += ms/1000;
		abstime->tv_nsec += (ms % 1000) * 1000000l;
	}

	else
		abstime->tv_nsec += ms * 1000000l;

	if (abstime->tv_nsec > 1000000000l)
	{
		abstime->tv_nsec -= 1000000000l;
		abstime->tv_sec += 1;
	}

}
