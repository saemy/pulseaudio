#ifndef foomainloopsignalhfoo
#define foomainloopsignalhfoo

/* $Id: mainloop-signal.h 1033 2006-06-19 21:53:48Z lennart $ */

/***
  This file is part of PulseAudio.
 
  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.
 
  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <pulse/mainloop-api.h>
#include <pulse/cdecl.h>

PA_C_DECL_BEGIN

/** \file
 * UNIX signal support for main loops. In contrast to other
 * main loop event sources such as timer and IO events, UNIX signal
 * support requires modification of the global process
 * environment. Due to this the generic main loop abstraction layer as
 * defined in \ref mainloop-api.h doesn't have direct support for UNIX
 * signals. However, you may hook signal support into an abstract main loop via the routines defined herein.
 */

/** Initialize the UNIX signal subsystem and bind it to the specified main loop */
int pa_signal_init(pa_mainloop_api *api);

/** Cleanup the signal subsystem */
void pa_signal_done(void);

/** An opaque UNIX signal event source object */
typedef struct pa_signal_event pa_signal_event;

/** Create a new UNIX signal event source object */
pa_signal_event* pa_signal_new(int sig, void (*callback) (pa_mainloop_api *api, pa_signal_event*e, int sig, void *userdata), void *userdata);

/** Free a UNIX signal event source object */
void pa_signal_free(pa_signal_event *e);

/** Set a function that is called when the signal event source is destroyed. Use this to free the userdata argument if required */
void pa_signal_set_destroy(pa_signal_event *e, void (*callback) (pa_mainloop_api *api, pa_signal_event*e, void *userdata));

PA_C_DECL_END

#endif
