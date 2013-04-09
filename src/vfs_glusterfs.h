#ifndef VFS_GLUSTERFS_H
#define VFS_GLUSTERFS_H
/* ========================================================================== **
 *                              vfs_glusterfs.h
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
 *  A direct connection from Samba to GlusterFS.
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
 *  + For each direct-connect Gluster share defined in your smb.conf file,
 *    add the following parameter to the share declaration:
 *      vfs objects = glusterfs
 *
 *  + The GlusterFS VFS module communicates directly with Gluster via
 *    libgfapi.  There is no mounted file system underlying the share, so
 *    no other VFS modules can be stacked below this one (unless they also
 *    communicate with libgfapi).
 *
 * ========================================================================== **
 */

#include "includes.h"     /* The Samba global includes file.    */
#include "smbd/smbd.h"    /* The Samba smbd daemon header file. */


/* -------------------------------------------------------------------------- **
 * Function Declarations:
 */

NTSTATUS vfs_glusterfs_init( void );
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
   *          Since this module implements a direct connection between Samba
   *          and Gluster (via libgfapi), it never talks to an actual mount
   *          point.  As a result, all of Samba's vfs functions MUST be
   *          implemented, or must fall through to a function in the
   *          <vfs_default> module that returns an error code indicating
   *          that the requested behavior isn't implemented.
   *
   * ------------------------------------------------------------------------ **
   */

#endif /* VFS_GLUSTERFS_H */
