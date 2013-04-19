/* ========================================================================== **
 *                              vfs_glusterfs.c
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

#include "vfs_glusterfs.h"  /* Module header.  Lots of important stuff here.  */


/* -------------------------------------------------------------------------- **
 * Static Functions:
 */

/* Volume Operations */

static int glu_connect( struct vfs_handle_struct *handle,
                        const char               *service,
                        const char               *user )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_connect */

static void glu_disconnect( struct vfs_handle_struct *handle )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_disconnect */

static uint64_t glu_disk_free( struct vfs_handle_struct *handle,
                               const char               *path,
                               bool                      small_query,
                               uint64_t                 *bsize,
                               uint64_t                 *dfree,
                               uint64_t                 *dsize )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_disk_free */

static int glu_get_quota( struct vfs_handle_struct *handle,
                          enum SMB_QUOTA_TYPE       qtype,
                          unid_t                    id,
                          SMB_DISK_QUOTA           *qt )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_get_quota */

static int glu_set_quota( struct vfs_handle_struct *handle,
                          enum SMB_QUOTA_TYPE       qtype,
                          unid_t                    id,
                          SMB_DISK_QUOTA           *qt )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_set_quota */

static int glu_statvfs( struct vfs_handle_struct  *handle,
                        const char                *path,
                        struct vfs_statvfs_struct *statbuf )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_statvfs */


/* Directory Operations */

static SMB_STRUCT_DIR *glu_opendir( struct vfs_handle_struct *handle,
                                    const char               *fname,
                                    const char               *mask,
                                    uint32                    attributes )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_opendir */

static int glu_closedir( struct vfs_handle_struct *handle,
                         SMB_STRUCT_DIR           *dir )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_closedir */

static SMB_STRUCT_DIRENT *glu_readdir( struct vfs_handle_struct *handle,
                                       SMB_STRUCT_DIR           *dirp,
                                       SMB_STRUCT_STAT          *sbuf )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_readdir */

static void glu_seekdir( struct vfs_handle_struct *handle,
                         SMB_STRUCT_DIR           *dirp,
                         long                      offset )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_seekdir */

static long glu_telldir( struct vfs_handle_struct *handle,
                         SMB_STRUCT_DIR           *dirp )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_telldir */

static void glu_rewinddir( struct vfs_handle_struct *handle,
                           SMB_STRUCT_DIR           *dirp )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_rewinddir */

static int glu_mkdir( struct vfs_handle_struct *handle,
                      const char               *path,
                      mode_t                    mode )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_mkdir */


static int glu_rmdir( struct vfs_handle_struct *handle, const char *path )
  /* ------------------------------------------------------------------------ **
   *
   *
   * ------------------------------------------------------------------------ **
   */
  {
  } /* glu_rmdir */


/* File operations. */
/* Extended Attribute (EA) operations.  */
/* Async I/O operations.  */

static bool glu_aio_force( struct vfs_handle_struct *handle,
                           struct files_struct      *fsp )
  /* ------------------------------------------------------------------------ **
   *
   *  Notes:  AsyncIO is not yet supported, so we set <errno> to ENOTSUP
   *          and return -1 to indicate an error.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  errno = ENOTSUP;
  return( -1 );
  } /* glu_aio_force */


/* Offline operations.  */

static bool glu_is_offline( struct vfs_handle_struct  *handle,
                            const struct smb_filename *fname,
                            SMB_STRUCT_STAT           *sbuf )
  /* ------------------------------------------------------------------------ **
   *
   * ------------------------------------------------------------------------ **
   */
  {
  return( false );
  } /* glu_is_offline */

static int glu_set_offline( struct vfs_handle_struct  *handle,
                            const struct smb_filename *fname )
  /* ------------------------------------------------------------------------ **
   * Set a
   *
   *  Input:  handle  - Pointer to a ???
   *          fname   - Pointer to a ???
   *
   *  Output: An integer.  A negative return
   *
   *  Notes:  This feature is not yet implemented, so this function sets the
   *          global <errno> to ENOTSUP to indicate that it's not supported.
   *          The function returns -1 to indicate an error.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  errno = ENOTSUP;
  return( -1 );
  } /* glu_set_offline */


/* -------------------------------------------------------------------------- **
 * Mapping of VFS Functions:
 *
 *  vfs_glusterfs_fns - A list of all of the VFS functions defined by this
 *                      module.  The initialization uses C89/C99 syntax.
 *                      Pointer fields not listed are initialized to NULL
 *                      (because the structure is statically allocated; see
 *                      the C language specification).  Any function not
 *                      explicitly assigned here will fall through to
 *                      functions lower on the VFS stack.
 *                      See <source3/include/vfs.h> in the Samba v3 source
 *                      tree.  Not sure where to look in the Samba v4 tree.
 */

static struct vfs_fn_pointers vfs_glusterfs_fns =
  {
  /* Volume operations. */
  .connect_fn = glu_connect,
  .disconnect = glu_disconnect,
  .disk_free  = glu_disk_free,
  .get_quota  = glu_get_quota,
  .set_quota  = glu_set_quota,
  .statvfs    = glu_statvfs,

  /* Directory operations.  */
  .opendir    = glu_opendir,
  .closedir   = glu_closedir,
  .readdir    = glu_readdir,
  .seekdir    = glu_seekdir,
  .telldir    = glu_telldir,
  .rewind_dir = glu_rewinddir,
  .mkdir      = glu_mkdir,
  .rmdir      = glu_rmdir,

  /* File operations. */
  .open_fn    = glu_open,
  .close_fn   = glu_close,
  .vfs_read   = glu_read,
  .pread      = glu_pread,
  .write      = glu_write,
  .pwrite     = glu_pwrite,
  .lseek      = glu_lseek,
  .sendfile   = glu_sendfile,
  .recvfile   = glu_recvfile,
  .rename     = glu_rename,
  .fsync      = glu_fsync,
  .stat       = glu_stat,
  .fstat      = glu_fstat,
  .lstat      = glu_lstat,
  .unlink     = glu_unlink,
  .chmod      = glu_chmod,
  .fchmod     = glu_fchmod,
  .chown      = glu_chown,
  .fchown     = glu_fchown,
  .lchown     = glu_lchown,
  .chdir      = glu_chdir,
  .getwd      = glu_getwd,
  .ftruncate  = glu_ftruncate,
  .lock       = glu_lock,
  .kernel_flock   = glu_kernel_flock,
  .linux_setlease = glu_linux_setlease,
  .getlock        = glu_getlock,
  .symlink        = glu_symlink,
  .vfs_readlink   = glu_readlink,
  .link           = glu_link,
  .mknod          = glu_mknod,
  .realpath       = glu_realpath,
  .notify_watch   = glu_notify_watch,
  .chflags        = glu_chflags,
  .get_real_filename  = glu_get_real_filename,
  .connectpath        = glu_connectpath,

  /* Extended Attribute (EA) operations.  */
  .getxattr     = glu_getxattr,
  .fgetxattr    = glu_fgetxattr,
  .listxattr    = glu_listxattr,
  .llistxattr   = glu_llistxattr,
  .flistxattr   = glu_flistxattr,
  .removexattr  = glu_removexattr,
  .fremovexattr = glu_fremovexattr,
  .setxattr     = glu_setxattr,
  .fsetxattr    = glu_fsetxattr,

  /* Async I/O operations.  */
  .aio_force    = glu_aio_force,

  /* Offline operations.  */
  .is_offline   = glu_is_offline,
  .set_offline  = glu_set_offline
  };


/* -------------------------------------------------------------------------- **
 * Exported Functions:
 */

NTSTATUS vfs_glusterfs_init( void )
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
  {
  int result;

  result = smb_register_vfs( SMB_VFS_INTERFACE_VERSION,
                             "glusterfs",
                             &(vfs_glusterfs_fns) );
  return( result );
  } /* vfs_glusterfs_init */

/* ========================================================================== */
