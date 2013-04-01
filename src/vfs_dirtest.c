/* ========================================================================== **
 *                               vfs_dirtest.c
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

#include "vfs_dirtest.h"    /* Module header.  Lots of important stuff here.  */


/* -------------------------------------------------------------------------- **
 * Static Functions:
 */

static SMB_STRUCT_DIRENT *dt_readdir( vfs_handle_struct *handle,
                                      SMB_STRUCT_DIR    *dirp,
                                      SMB_STRUCT_STAT   *sbufr )
  /* ------------------------------------------------------------------------ **
   * Retrieve one directory entry and (optional) stat buffer.
   *
   *  Input:  handle  - A pointer to the VFS handle for the current VFS
   *                    context.  [Needs better documentation.]
   *          dirp    - A pointer to the directory handle representing an
   *                    already opened directory.
   *          sbufr   - If NULL, then it is ignored.  If non-NULL, then it
   *                    is a pointer to a buffer large enough to receive an
   *                    SMB_STRUCT_STAT structure.
   *
   *  Output: A pointer to an SMB_STRUCT_DIRENT structure, or NULL.
   *          NULL is returned on error, or if there are no more entries
   *          available to return from the directory indicated by <dirp>.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  SMB_STRUCT_DIRENT *result = SMB_VFS_NEXT_READDIR( handle, dirp, sbufr );

  if( result )
    {
    DEBUG( 0, ("[dirtest] readdir: %s\n", result->d_name) );
    }
  return( result );
  } /* dt_readdir */

static int dt_open( vfs_handle_struct   *handle,
                    struct smb_filename *smb_fname,
                    files_struct        *fsp,
                    int                  flags,
                    mode_t               mode )
  /* ------------------------------------------------------------------------ **
   *
   * ------------------------------------------------------------------------ **
   */
  {
  DEBUG( 0, ("[dirtest] open: %s\n", smb_fname_str_dbg( smb_fname )) );
  return( SMB_VFS_NEXT_OPEN( handle, smb_fname, fsp, flags, mode ) );
  } /* dt_open */

static int dt_stat( vfs_handle_struct   *handle,
                    struct smb_filename *smb_fname )
  /* ------------------------------------------------------------------------ **
   *
   * ------------------------------------------------------------------------ **
   */
  {
  DEBUG( 0, ("[dirtest] stat: %s\n", smb_fname_str_dbg( smb_fname )) );
  return( SMB_VFS_NEXT_STAT( handle, smb_fname ) );
  } /* dt_stat */

static int dt_grfn( struct vfs_handle_struct *handle,
                    const char               *path,
                    const char               *name,
                    TALLOC_CTX               *mem_ctx,
                    char                    **found_name )
  /* ------------------------------------------------------------------------ **
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int result;

  DEBUG( 0, ("[dirtest] grfn: %s\n", name ) );
  result = SMB_VFS_NEXT_GET_REAL_FILENAME( handle,
                                           path,
                                           name,
                                           mem_ctx,
                                           found_name );
  return( result );
  } /* dt_grfn */


/* -------------------------------------------------------------------------- **
 * Static Globals:
 *
 *  vfs_dirtest_fns - A list of all of the VFS functions defined by this
 *                    module.  The initialization uses C89/C99 syntax.
 *                    Pointer fields not listed are initialized to NULL
 *                    (because the structure is statically allocated; see
 *                    the C language specification).  Any function not
 *                    explicitly assigned here will fall through to
 *                    functions lower on the VFS stack.
 *                    See <source3/include/vfs.h> in the Samba v3 source
 *                    tree.  Not sure where to look in the Samba v4 tree.
 */

static struct vfs_fn_pointers vfs_dirtest_fns =
  {
  /* Directory operations.  */
  .readdir           = dt_readdir,
  /* File operations. */
  .open_fn           = dt_open,
  .stat              = dt_stat,
  .get_real_filename = dt_grfn
  };


/* -------------------------------------------------------------------------- **
 * Exported Functions:
 */

NTSTATUS vfs_dirtest_init( void )
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
   * ------------------------------------------------------------------------ **
   */
  {
  NTSTATUS result;

  result = smb_register_vfs( SMB_VFS_INTERFACE_VERSION,
                             "dirtest",
                             &(vfs_dirtest_fns) );
  return( result );
  } /* vfs_dirtest_init */

/* ========================================================================== */

