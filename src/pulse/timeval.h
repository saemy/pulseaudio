#ifndef footimevalhfoo
#define footimevalhfoo

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <pulse/cdecl.h>
#include <pulse/gccmacro.h>
#include <pulse/sample.h>
#include <pulse/version.h>

/** \file
 * Utility functions for handling timeval calculations */

PA_C_DECL_BEGIN

/** The number of milliseconds in a second */
#define PA_MSEC_PER_SEC ((pa_usec_t) 1000ULL)

/** The number of microseconds in a second */
#define PA_USEC_PER_SEC ((pa_usec_t) 1000000ULL)

/** The number of nanoseconds in a second */
#define PA_NSEC_PER_SEC ((pa_usec_t) 1000000000ULL)

/** The number of microseconds in a millisecond */
#define PA_USEC_PER_MSEC ((pa_usec_t) 1000ULL)

/** The number of nanoseconds in a millisecond */
#define PA_NSEC_PER_MSEC ((pa_usec_t) 1000000ULL)

/** The number of nanoseconds in a microsecond */
#define PA_NSEC_PER_USEC ((pa_usec_t) 1000ULL)

struct timeval;

/** Return the current timestamp, just like UNIX gettimeofday() */
struct timeval *pa_gettimeofday(struct timeval *tv);

/** Calculate the difference between the two specified timeval
 * structs. */
pa_usec_t pa_timeval_diff(const struct timeval *a, const struct timeval *b) PA_GCC_PURE;

/** Compare the two timeval structs and return 0 when equal, negative when a < b, positive otherwse */
int pa_timeval_cmp(const struct timeval *a, const struct timeval *b) PA_GCC_PURE;

/** Return the time difference between now and the specified timestamp */
pa_usec_t pa_timeval_age(const struct timeval *tv);

/** Add the specified time inmicroseconds to the specified timeval structure */
struct timeval* pa_timeval_add(struct timeval *tv, pa_usec_t v);

/** Subtract the specified time inmicroseconds to the specified timeval structure. \since 0.9.11 */
struct timeval* pa_timeval_sub(struct timeval *tv, pa_usec_t v);

/** Store the specified uec value in the timeval struct. \since 0.9.7 */
struct timeval* pa_timeval_store(struct timeval *tv, pa_usec_t v);

/** Load the specified tv value and return it in usec. \since 0.9.7 */
pa_usec_t pa_timeval_load(const struct timeval *tv);

PA_C_DECL_END

#endif
