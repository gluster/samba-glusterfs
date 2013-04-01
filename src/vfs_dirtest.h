#ifndef VFS_DIRTEST_H
#define VFS_DIRTEST_H
/* ========================================================================== **
 *                               vfs_dirtest.h
 *
 * Copyright:
 *  Copyright (C) 2013 by Christopher R. Hertel
 *
 * Email: crh@redhat.com
 *
 * $Id$
 *
 * -------------------------------------------------------------------------- **
 *
 * Description:
 *  Test VFS for catching directory queries.
 *
 * -------------------------------------------------------------------------- **
 *
 * License:
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 3 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- **
 *
 * Notes:
 *  + For each FUSE mounted Gluster share defined in your smb.conf file,
 *    add the following parameterr to the share declaration:
 *      vfs objects = dirtest
 *
 *  + The name "dirtest" looks like "dirtiest".
 *    Not completely inappropriate.
 *
 *  + This module catches and reports on directory queries, so that we can
 *    inspect (and, potentially, replace) Samba's handling of name lookups.
 *
 *  + There used to be a directory cache, but that seems to have gone away.
 *    We might need to replicate some of that old behavior here.
 *
 * ========================================================================== **
 */

#include "includes.h"     /* The Samba global includes file.    */
#include "smbd/smbd.h"    /* The Samba smbd daemon header file. */


/* -------------------------------------------------------------------------- **
 * Function Declarations:
 */

NTSTATUS vfs_dirtest_init( void );
  /* ------------------------------------------------------------------------ **
   * Module initialization.
   *
   *  Input:  <none>
   *
   *  Output: An NTSTATUS value, which is a 32-bit error code
   *
   *  Notes:  This function is called when Samba starts up.  It pushes an
   *          array of functions onto the VFS call stack.
   *
   *          This is a testing module, so we'll use the initialization as
   *          an opportunity to create a log file that we'll use to collect
   *          the information we're capturing.
   *
   * ------------------------------------------------------------------------ **
   */

#endif /* VFS_DIRTEST_H */
