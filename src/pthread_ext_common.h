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

/** @file pthread_ext_common.h
 * @brief pthread extension common functions
 */

#ifndef PTHREAD_EXT_COMMON_H
#define PTHREAD_EXT_COMMON_H

#include <stdint.h>
#include <time.h>

/** Timeout identifiers */
#define PTHREAD_WAIT	(-1)
#define PTHREAD_NOWAIT	(0)

/** Convert relative time (from now) to absolute time
 *
 * @param[in]  ms			number of milliseconds relative to current time
 * @param[out] abstime		pointer to timespec structure which holds absolute time
 */
void pthread_ext_ms2abs_time(long ms, struct timespec * abstime);

#endif  /* PTHREAD_EXT_COMMON_H */
