#ifndef foopulsecdeclhfoo
#define foopulsecdeclhfoo

/* $Id: cdecl.h 1426 2007-02-13 15:35:19Z ossman $ */

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

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

/** \file
 * C++ compatibility support */

#ifdef __cplusplus
/** If using C++ this macro enables C mode, otherwise does nothing */
#define PA_C_DECL_BEGIN extern "C" {
/** If using C++ this macros switches back to C++ mode, otherwise does nothing */
#define PA_C_DECL_END }

#else
/** If using C++ this macro enables C mode, otherwise does nothing */
#define PA_C_DECL_BEGIN
/** If using C++ this macros switches back to C++ mode, otherwise does nothing */
#define PA_C_DECL_END

#endif

#endif
