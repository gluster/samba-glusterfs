/*
   Unix SMB/CIFS implementation.

   Wrap GlusterFS GFAPI calls in vfs functions.

   Copyright (c) 2013 Anand Avati <avati@redhat.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file   vfs_glusterfs.c
 * @author Anand Avati <avati@redhat.com>
 * @date   May 2013
 * @brief  Samba VFS module for glusterfs
 *
 * @todo
 *   - AIO support\n
 *     See, for example \c vfs_aio_linux.c in the \c sourc3/modules directory
 *   - sendfile/recvfile support
 *
 * A Samba VFS module for GlusterFS, based on Gluster's libgfapi.
 * This is a "bottom" vfs module (not something to be stacked on top of
 * another module), and translates (most) calls to the closest actions
 * available in libgfapi.
 *
 */

#include "includes.h"
#include "smbd/smbd.h"
#include <stdio.h>
#include "api/glfs.h"

#define DEFAULT_VOLFILE_SERVER "localhost"

/**
 * Helper to convert struct stat to struct stat_ex.
 */
static void smb_stat_ex_from_stat(struct stat_ex *dst, const struct stat *src)
{
	ZERO_STRUCTP(dst);

	dst->st_ex_dev = src->st_dev;
	dst->st_ex_ino = src->st_ino;
	dst->st_ex_mode = src->st_mode;
	dst->st_ex_nlink = src->st_nlink;
	dst->st_ex_uid = src->st_uid;
	dst->st_ex_gid = src->st_gid;
	dst->st_ex_rdev = src->st_rdev;
	dst->st_ex_size = src->st_size;
	dst->st_ex_atime.tv_sec = src->st_atime;
	dst->st_ex_mtime.tv_sec = src->st_mtime;
	dst->st_ex_ctime.tv_sec = src->st_ctime;
	dst->st_ex_btime.tv_sec = src->st_mtime;
	dst->st_ex_blksize = src->st_blksize;
	dst->st_ex_blocks = src->st_blocks;
#ifdef STAT_HAVE_NSEC
	dst->st_ex_atime.tv_nsec = src->st_atime_nsec;
	dst->st_ex_mtime.tv_nsec = src->st_mtime_nsec;
	dst->st_ex_ctime.tv_nsec = src->st_ctime_nsec;
	dst->st_ex_btime.tv_nsec = src->st_mtime_nsec;
#endif
}

/* pre-opened glfs_t */

static struct glfs_preopened {
	char *volume;
	char *connectpath;
	glfs_t *fs;
	int ref;
	struct glfs_preopened *next, *prev;
} *glfs_preopened;


static int glfs_set_preopened(const char *volume, const char *connectpath, glfs_t *fs)
{
	struct glfs_preopened *entry = NULL;

	entry = talloc_zero(NULL, struct glfs_preopened);
	if (!entry) {
		errno = ENOMEM;
		return -1;
	}

	entry->volume = talloc_strdup(entry, volume);
	if (!entry->volume) {
		talloc_free(entry);
		errno = ENOMEM;
		return -1;
	}

	entry->connectpath = talloc_strdup(entry, connectpath);
	if (entry->connectpath == NULL) {
		talloc_free(entry);
		errno = ENOMEM;
		return -1;
	}

	entry->fs = fs;
	entry->ref = 1;

	DLIST_ADD(glfs_preopened, entry);

	return 0;
}

static glfs_t *glfs_find_preopened(const char *volume, const char *connectpath)
{
	struct glfs_preopened *entry = NULL;

	for (entry = glfs_preopened; entry; entry = entry->next) {
		if (strcmp(entry->volume, volume) == 0 &&
		    strcmp(entry->connectpath, connectpath) == 0)
		{
			entry->ref++;
			return entry->fs;
		}
	}

	return NULL;
}

static void glfs_clear_preopened(glfs_t *fs)
{
	struct glfs_preopened *entry = NULL;

	for (entry = glfs_preopened; entry; entry = entry->next) {
		if (entry->fs == fs) {
			if (--entry->ref)
				return;

			DLIST_REMOVE(glfs_preopened, entry);

			glfs_fini(entry->fs);
			talloc_free(entry);
		}
	}
}

/* Disk Operations */

static int vfs_gluster_connect(struct vfs_handle_struct *handle,
			       const char *service,
			       const char *user)
{
	const char *volfile_server;
	const char *volume;
	char *logfile;
	int loglevel;
	glfs_t *fs = NULL;
	int ret = 0;

	logfile = lp_parm_talloc_string(SNUM(handle->conn), "glusterfs",
				       "logfile", NULL);

	loglevel = lp_parm_int(SNUM(handle->conn), "glusterfs", "loglevel", -1);

	volfile_server = lp_parm_const_string(SNUM(handle->conn), "glusterfs",
					       "volfile_server", NULL);
	if (volfile_server == NULL) {
		volfile_server = DEFAULT_VOLFILE_SERVER;
	}

	volume = lp_parm_const_string(SNUM(handle->conn), "glusterfs", "volume",
				      NULL);
	if (volume == NULL) {
		volume = service;
	}

	fs = glfs_find_preopened(volume, handle->conn->connectpath);
	if (fs) {
		goto done;
	}

	fs = glfs_new(volume);
	if (fs == NULL) {
		ret = -1;
		goto done;
	}

	ret = glfs_set_volfile_server(fs, "tcp", volfile_server, 0);
	if (ret < 0) {
		DEBUG(0, ("Failed to set volfile_server %s\n", volfile_server));
		goto done;
	}

	ret = glfs_set_xlator_option(fs, "*-md-cache", "cache-posix-acl",
				     "true");
	if (ret < 0) {
		DEBUG(0, ("%s: Failed to set xlator options\n", volume));
		goto done;
	}

	ret = glfs_set_logging(fs, logfile, loglevel);
	if (ret < 0) {
		DEBUG(0, ("%s: Failed to set logfile %s loglevel %d\n",
			  volume, logfile, loglevel));
		goto done;
	}

	ret = glfs_init(fs);
	if (ret < 0) {
		DEBUG(0, ("%s: Failed to initialize volume (%s)\n",
			  volume, strerror(errno)));
		goto done;
	}

	ret = glfs_set_preopened(volume, handle->conn->connectpath, fs);
	if (ret < 0) {
		DEBUG(0, ("%s: Failed to register volume (%s)\n",
			  volume, strerror(errno)));
		goto done;
	}
done:
	talloc_free(logfile);
	if (ret < 0) {
		if (fs)
			glfs_fini(fs);
		return -1;
	} else {
		DEBUG(0, ("%s: Initialized volume from server %s\n",
                         volume, volfile_server));
		handle->data = fs;
		return 0;
	}
}

static void vfs_gluster_disconnect(struct vfs_handle_struct *handle)
{
	glfs_t *fs = NULL;

	fs = handle->data;

	glfs_clear_preopened(fs);
}

static uint64_t vfs_gluster_disk_free(struct vfs_handle_struct *handle,
				      const char *path, bool small_query,
				      uint64_t *bsize_p, uint64_t *dfree_p,
				      uint64_t *dsize_p)
{
	struct statvfs statvfs = { 0, };
	int ret;

	ret = glfs_statvfs(handle->data, path, &statvfs);
	if (ret < 0) {
		DEBUG(0, ("glfs_statvfs(%s) failed: %s\n",
			  path, strerror(errno)));
		return -1;
	}

	if (bsize_p != NULL) {
		*bsize_p = (uint64_t)statvfs.f_bsize; /* Block size */
	}
	if (dfree_p != NULL) {
		*dfree_p = (uint64_t)statvfs.f_bavail; /* Available Block units */
	}
	if (dsize_p != NULL) {
		*dsize_p = (uint64_t)statvfs.f_blocks; /* Total Block units */
	}

	return (uint64_t)statvfs.f_bavail;
}

static int vfs_gluster_get_quota(struct vfs_handle_struct *handle,
				 enum SMB_QUOTA_TYPE qtype, unid_t id,
				 SMB_DISK_QUOTA *qt)
{
	errno = ENOSYS;
	return -1;
}

static int
vfs_gluster_set_quota(struct vfs_handle_struct *handle,
		      enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *qt)
{
	errno = ENOSYS;
	return -1;
}

static int vfs_gluster_statvfs(struct vfs_handle_struct *handle,
			       const char *path,
			       struct vfs_statvfs_struct *vfs_statvfs)
{
	struct statvfs statvfs = { 0, };
	int ret;

	ret = glfs_statvfs(handle->data, path, &statvfs);
	if (ret < 0) {
		DEBUG(0, ("glfs_statvfs(%s) failed: %s\n",
			  path, strerror(errno)));
		return -1;
	}

	ZERO_STRUCTP(vfs_statvfs);

	vfs_statvfs->OptimalTransferSize = statvfs.f_frsize;
	vfs_statvfs->BlockSize = statvfs.f_bsize;
	vfs_statvfs->TotalBlocks = statvfs.f_blocks;
	vfs_statvfs->BlocksAvail = statvfs.f_bfree;
	vfs_statvfs->UserBlocksAvail = statvfs.f_bavail;
	vfs_statvfs->TotalFileNodes = statvfs.f_files;
	vfs_statvfs->FreeFileNodes = statvfs.f_ffree;
	vfs_statvfs->FsIdentifier = statvfs.f_fsid;
	vfs_statvfs->FsCapabilities =
	    FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES;

	return ret;
}

static uint32_t vfs_gluster_fs_capabilities(struct vfs_handle_struct *handle,
					    enum timestamp_set_resolution *p_ts_res)
{
	uint32_t caps = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES;

#ifdef STAT_HAVE_NSEC
	*p_ts_res = TIMESTAMP_SET_NT_OR_BETTER;
#endif

	return caps;
}

static DIR *vfs_gluster_opendir(struct vfs_handle_struct *handle,
				const char *path, const char *mask,
				uint32 attributes)
{
	glfs_fd_t *fd;

	fd = glfs_opendir(handle->data, path);
	if (fd == NULL) {
		DEBUG(0, ("glfs_opendir(%s) failed: %s\n",
			  path, strerror(errno)));
	}

	return (DIR *) fd;
}

static DIR *vfs_gluster_fdopendir(struct vfs_handle_struct *handle,
				  files_struct *fsp, const char *mask,
				  uint32 attributes)
{
	return (DIR *) *(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp);
}

static int vfs_gluster_closedir(struct vfs_handle_struct *handle, DIR *dirp)
{
	return glfs_closedir((void *)dirp);
}

static SMB_STRUCT_DIRENT *vfs_gluster_readdir(struct vfs_handle_struct *handle,
					      SMB_STRUCT_DIR *dirp,
					      SMB_STRUCT_STAT *sbuf)
{
	static char direntbuf[512];
	int ret;
	struct stat stat;
	struct dirent *dirent = 0;
	static SMB_STRUCT_DIRENT result;

	if (sbuf != NULL) {
		ret = glfs_readdirplus_r((void *)dirp, &stat, (void *)direntbuf,
					 &dirent);
	} else {
		ret = glfs_readdir_r((void *)dirp, (void *)direntbuf, &dirent);
	}

	if ((ret < 0) || (dirent == NULL)) {
		return NULL;
	}

	if (sbuf != NULL) {
		smb_stat_ex_from_stat(sbuf, &stat);
	}

	result.d_ino = dirent->d_ino;
	result.d_off = dirent->d_off;
	result.d_reclen = dirent->d_reclen;
	result.d_type = dirent->d_type;
	strncpy(result.d_name, dirent->d_name, 256);

	return &result;
}

static long vfs_gluster_telldir(struct vfs_handle_struct *handle, DIR *dirp)
{
	return glfs_telldir((void *)dirp);
}

static void vfs_gluster_seekdir(struct vfs_handle_struct *handle, DIR *dirp,
				long offset)
{
	glfs_seekdir((void *)dirp, offset);
}

static void vfs_gluster_rewinddir(struct vfs_handle_struct *handle, DIR *dirp)
{
	glfs_seekdir((void *)dirp, 0);
}

static void vfs_gluster_init_search_op(struct vfs_handle_struct *handle,
				       DIR *dirp)
{
	return;
}

static int vfs_gluster_mkdir(struct vfs_handle_struct *handle, const char *path,
			     mode_t mode)
{
	return glfs_mkdir(handle->data, path, mode);
}

static int vfs_gluster_rmdir(struct vfs_handle_struct *handle, const char *path)
{
	return glfs_rmdir(handle->data, path);
}

static int vfs_gluster_open(struct vfs_handle_struct *handle,
			    struct smb_filename *smb_fname, files_struct *fsp,
			    int flags, mode_t mode)
{
	glfs_fd_t *glfd;
	glfs_fd_t **p_tmp;

	if (flags & O_DIRECTORY) {
		glfd = glfs_opendir(handle->data, smb_fname->base_name);
	} else if (flags & O_CREAT) {
		glfd = glfs_creat(handle->data, smb_fname->base_name, flags,
				  mode);
	} else {
		glfd = glfs_open(handle->data, smb_fname->base_name, flags);
	}

	if (glfd == NULL) {
		return -1;
	}
	p_tmp = (glfs_fd_t **)VFS_ADD_FSP_EXTENSION(handle, fsp,
							  glfs_fd_t *, NULL);
	*p_tmp = glfd;
	/* An arbitrary value for error reporting, so you know its us. */
	return 13371337;
}

static int vfs_gluster_close(struct vfs_handle_struct *handle,
			     files_struct *fsp)
{
	glfs_fd_t *glfd;
	glfd = *(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp);
	VFS_REMOVE_FSP_EXTENSION(handle, fsp);
	return glfs_close(glfd);
}

static ssize_t vfs_gluster_read(struct vfs_handle_struct *handle,
				files_struct *fsp, void *data, size_t n)
{
	return glfs_read(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), data, n, 0);
}

static ssize_t vfs_gluster_pread(struct vfs_handle_struct *handle,
				 files_struct *fsp, void *data,
				 size_t n, off_t offset)
{
	return glfs_pread(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), data, n, offset, 0);
}

static ssize_t vfs_gluster_write(struct vfs_handle_struct *handle,
				 files_struct *fsp, const void *data, size_t n)
{
	return glfs_write(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), data, n, 0);
}

static ssize_t vfs_gluster_pwrite(struct vfs_handle_struct *handle,
				  files_struct *fsp, const void *data,
				  size_t n, off_t offset)
{
	return glfs_pwrite(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), data, n, offset, 0);
}

static off_t vfs_gluster_lseek(struct vfs_handle_struct *handle,
			       files_struct *fsp, off_t offset, int whence)
{
	return glfs_lseek(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), offset, whence);
}

static ssize_t vfs_gluster_sendfile(struct vfs_handle_struct *handle, int tofd,
				    files_struct *fromfsp,
				    const DATA_BLOB *hdr,
				    off_t offset, size_t n)
{
	errno = ENOTSUP;
	return -1;
}

static ssize_t vfs_gluster_recvfile(struct vfs_handle_struct *handle,
				    int fromfd, files_struct *tofsp,
				    off_t offset, size_t n)
{
	errno = ENOTSUP;
	return -1;
}

static int vfs_gluster_rename(struct vfs_handle_struct *handle,
			      const struct smb_filename *smb_fname_src,
			      const struct smb_filename *smb_fname_dst)
{
	return glfs_rename(handle->data, smb_fname_src->base_name,
			   smb_fname_dst->base_name);
}

static int vfs_gluster_fsync(struct vfs_handle_struct *handle,
			     files_struct *fsp)
{
	return glfs_fsync(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp));
}

static int vfs_gluster_stat(struct vfs_handle_struct *handle,
			    struct smb_filename *smb_fname)
{
	struct stat st;
	int ret;

	ret = glfs_stat(handle->data, smb_fname->base_name, &st);
	if (ret == 0) {
		smb_stat_ex_from_stat(&smb_fname->st, &st);
	}
	if (ret < 0 && errno != ENOENT) {
		DEBUG(0, ("glfs_stat(%s) failed: %s\n",
			  smb_fname->base_name, strerror(errno)));
	}
	return ret;
}

static int vfs_gluster_fstat(struct vfs_handle_struct *handle,
			     files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	struct stat st;
	int ret;

	ret = glfs_fstat(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), &st);
	if (ret == 0) {
		smb_stat_ex_from_stat(sbuf, &st);
	}
	if (ret < 0) {
		DEBUG(0, ("glfs_fstat(%d) failed: %s\n",
			  fsp->fh->fd, strerror(errno)));
	}
	return ret;
}

static int vfs_gluster_lstat(struct vfs_handle_struct *handle,
			     struct smb_filename *smb_fname)
{
	struct stat st;
	int ret;

	ret = glfs_lstat(handle->data, smb_fname->base_name, &st);
	if (ret == 0) {
		smb_stat_ex_from_stat(&smb_fname->st, &st);
	}
	if (ret < 0 && errno != ENOENT) {
		DEBUG(0, ("glfs_lstat(%s) failed: %s\n",
			  smb_fname->base_name, strerror(errno)));
	}
	return ret;
}

static uint64_t vfs_gluster_get_alloc_size(struct vfs_handle_struct *handle,
					   files_struct *fsp,
					   const SMB_STRUCT_STAT *sbuf)
{
	return sbuf->st_ex_blocks * 512;
}

static int vfs_gluster_unlink(struct vfs_handle_struct *handle,
			      const struct smb_filename *smb_fname)
{
	return glfs_unlink(handle->data, smb_fname->base_name);
}

static int vfs_gluster_chmod(struct vfs_handle_struct *handle,
			     const char *path, mode_t mode)
{
	return glfs_chmod(handle->data, path, mode);
}

static int vfs_gluster_fchmod(struct vfs_handle_struct *handle,
			      files_struct *fsp, mode_t mode)
{
	return glfs_fchmod(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), mode);
}

static int vfs_gluster_chown(struct vfs_handle_struct *handle,
			     const char *path, uid_t uid, gid_t gid)
{
	return glfs_chown(handle->data, path, uid, gid);
}

static int vfs_gluster_fchown(struct vfs_handle_struct *handle,
			      files_struct *fsp, uid_t uid, gid_t gid)
{
	return glfs_fchown(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), uid, gid);
}

static int vfs_gluster_lchown(struct vfs_handle_struct *handle,
			      const char *path, uid_t uid, gid_t gid)
{
	return glfs_lchown(handle->data, path, uid, gid);
}

static int vfs_gluster_chdir(struct vfs_handle_struct *handle, const char *path)
{
	return glfs_chdir(handle->data, path);
}

static char *vfs_gluster_getwd(struct vfs_handle_struct *handle, char *path)
{
	return glfs_getcwd(handle->data, path, PATH_MAX);
}

static int vfs_gluster_ntimes(struct vfs_handle_struct *handle,
			      const struct smb_filename *smb_fname,
			      struct smb_file_time *ft)
{
	struct timespec times[2];

	if (null_timespec(ft->atime)) {
		times[0].tv_sec = smb_fname->st.st_ex_atime.tv_sec;
		times[0].tv_nsec = smb_fname->st.st_ex_atime.tv_nsec;
	} else {
		times[0].tv_sec = ft->atime.tv_sec;
		times[0].tv_nsec = ft->atime.tv_nsec;
	}

	if (null_timespec(ft->mtime)) {
		times[1].tv_sec = smb_fname->st.st_ex_mtime.tv_sec;
		times[1].tv_nsec = smb_fname->st.st_ex_mtime.tv_nsec;
	} else {
		times[1].tv_sec = ft->mtime.tv_sec;
		times[1].tv_nsec = ft->mtime.tv_nsec;
	}

	if ((timespec_compare(&times[0],
			      &smb_fname->st.st_ex_atime) == 0) &&
	    (timespec_compare(&times[1],
			      &smb_fname->st.st_ex_mtime) == 0)) {
		return 0;
	}

	return glfs_utimens(handle->data, smb_fname->base_name, times);
}

static int vfs_gluster_ftruncate(struct vfs_handle_struct *handle,
				 files_struct *fsp, off_t offset)
{
	return glfs_ftruncate(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), offset);
}

static int vfs_gluster_fallocate(struct vfs_handle_struct *handle,
				 struct files_struct *fsp,
				 enum vfs_fallocate_mode mode,
				 off_t offset, off_t len)
{
	errno = ENOTSUP;
	return -1;
}

static char *vfs_gluster_realpath(struct vfs_handle_struct *handle,
				  const char *path)
{
	return glfs_realpath(handle->data, path, 0);
}

static bool vfs_gluster_lock(struct vfs_handle_struct *handle,
			     files_struct *fsp, int op, off_t offset,
			     off_t count, int type)
{
	struct flock flock = { 0, };
	int ret;

	flock.l_type = type;
	flock.l_whence = SEEK_SET;
	flock.l_start = offset;
	flock.l_len = count;
	flock.l_pid = 0;

	ret = glfs_posix_lock(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), op, &flock);

	if (op == F_GETLK) {
		/* lock query, true if someone else has locked */
		if ((ret != -1) &&
		    (flock.l_type != F_UNLCK) &&
		    (flock.l_pid != 0) && (flock.l_pid != getpid()))
			return true;
		/* not me */
		return false;
	}

	if (ret == -1) {
		return false;
	}

	return true;
}

static int vfs_gluster_kernel_flock(struct vfs_handle_struct *handle,
				    files_struct *fsp, uint32 share_mode,
				    uint32_t access_mask)
{
	return 0;
}

static int vfs_gluster_linux_setlease(struct vfs_handle_struct *handle,
				      files_struct *fsp, int leasetype)
{
	errno = ENOSYS;
	return -1;
}

static bool vfs_gluster_getlock(struct vfs_handle_struct *handle,
				files_struct *fsp, off_t *poffset,
				off_t *pcount, int *ptype, pid_t *ppid)
{
	struct flock flock = { 0, };
	int ret;

	flock.l_type = *ptype;
	flock.l_whence = SEEK_SET;
	flock.l_start = *poffset;
	flock.l_len = *pcount;
	flock.l_pid = 0;

	ret = glfs_posix_lock(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), F_GETLK, &flock);

	if (ret == -1) {
		return false;
	}

	*ptype = flock.l_type;
	*poffset = flock.l_start;
	*pcount = flock.l_len;
	*ppid = flock.l_pid;

	return true;
}

static int vfs_gluster_symlink(struct vfs_handle_struct *handle,
			       const char *oldpath, const char *newpath)
{
	return glfs_symlink(handle->data, oldpath, newpath);
}

static int vfs_gluster_readlink(struct vfs_handle_struct *handle,
				const char *path, char *buf, size_t bufsiz)
{
	return glfs_readlink(handle->data, path, buf, bufsiz);
}

static int vfs_gluster_link(struct vfs_handle_struct *handle,
			    const char *oldpath, const char *newpath)
{
	return glfs_link(handle->data, oldpath, newpath);
}

static int vfs_gluster_mknod(struct vfs_handle_struct *handle, const char *path,
			     mode_t mode, SMB_DEV_T dev)
{
	return glfs_mknod(handle->data, path, mode, dev);
}

static NTSTATUS vfs_gluster_notify_watch(struct vfs_handle_struct *handle,
					 struct sys_notify_context *ctx,
					 struct notify_entry *e,
					 void (*callback) (struct sys_notify_context *ctx,
							   void *private_data,
							   struct notify_event *ev),
					 void *private_data, void *handle_p)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static int vfs_gluster_chflags(struct vfs_handle_struct *handle,
			       const char *path, unsigned int flags)
{
	errno = ENOSYS;
	return -1;
}

static int vfs_gluster_get_real_filename(struct vfs_handle_struct *handle,
					 const char *path, const char *name,
					 TALLOC_CTX *mem_ctx, char **found_name)
{
	int ret;
	char key_buf[NAME_MAX + 64];
	char val_buf[NAME_MAX + 1];

	if (strlen(name) >= NAME_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	snprintf(key_buf, NAME_MAX + 64,
		 "user.glusterfs.get_real_filename:%s", name);

	ret = glfs_getxattr(handle->data, path, key_buf, val_buf, NAME_MAX + 1);
	if (ret == -1) {
		if (errno == ENODATA) {
			errno = EOPNOTSUPP;
		}
		return -1;
	}

	*found_name = talloc_strdup(mem_ctx, val_buf);
	if (found_name[0] == NULL) {
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

static const char *vfs_gluster_connectpath(struct vfs_handle_struct *handle,
					   const char *filename)
{
	return handle->conn->connectpath;
}

/* EA Operations */

static ssize_t vfs_gluster_getxattr(struct vfs_handle_struct *handle,
				    const char *path, const char *name,
				    void *value, size_t size)
{
	return glfs_getxattr(handle->data, path, name, value, size);
}

static ssize_t vfs_gluster_lgetxattr(struct vfs_handle_struct *handle,
				     const char *path, const char *name,
				     void *value, size_t size)
{
	return glfs_lgetxattr(handle->data, path, name, value, size);
}

static ssize_t vfs_gluster_fgetxattr(struct vfs_handle_struct *handle,
				     files_struct *fsp, const char *name,
				     void *value, size_t size)
{
	return glfs_fgetxattr(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), name, value, size);
}

static ssize_t vfs_gluster_listxattr(struct vfs_handle_struct *handle,
				     const char *path, char *list, size_t size)
{
	return glfs_listxattr(handle->data, path, list, size);
}

static ssize_t vfs_gluster_llistxattr(struct vfs_handle_struct *handle,
				      const char *path, char *list, size_t size)
{
	return glfs_llistxattr(handle->data, path, list, size);
}

static ssize_t vfs_gluster_flistxattr(struct vfs_handle_struct *handle,
				      files_struct *fsp, char *list,
				      size_t size)
{
	return glfs_flistxattr(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), list, size);
}

static int vfs_gluster_removexattr(struct vfs_handle_struct *handle,
				   const char *path, const char *name)
{
	return glfs_removexattr(handle->data, path, name);
}

static int vfs_gluster_lremovexattr(struct vfs_handle_struct *handle,
				    const char *path, const char *name)
{
	return glfs_lremovexattr(handle->data, path, name);
}

static int vfs_gluster_fremovexattr(struct vfs_handle_struct *handle,
				    files_struct *fsp, const char *name)
{
	return glfs_fremovexattr(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), name);
}

static int vfs_gluster_setxattr(struct vfs_handle_struct *handle,
				const char *path, const char *name,
				const void *value, size_t size, int flags)
{
	return glfs_setxattr(handle->data, path, name, value, size, flags);
}

static int vfs_gluster_lsetxattr(struct vfs_handle_struct *handle,
				 const char *path, const char *name,
				 const void *value, size_t size, int flags)
{
	return glfs_lsetxattr(handle->data, path, name, value, size, flags);
}

static int vfs_gluster_fsetxattr(struct vfs_handle_struct *handle,
				 files_struct *fsp, const char *name,
				 const void *value, size_t size, int flags)
{
	return glfs_fsetxattr(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp), name, value, size,
			      flags);
}

/* AIO Operations */

static bool vfs_gluster_aio_force(struct vfs_handle_struct *handle,
				  files_struct *fsp)
{
	return false;
}

/* Offline Operations */

static bool vfs_gluster_is_offline(struct vfs_handle_struct *handle,
				   const struct smb_filename *fname,
				   SMB_STRUCT_STAT *sbuf)
{
	return false;
}

static int vfs_gluster_set_offline(struct vfs_handle_struct *handle,
				   const struct smb_filename *fname)
{
	errno = ENOTSUP;
	return -1;
}

/* Posix ACL Operations */

#define GLUSTER_ACL_VERSION 2
#define GLUSTER_ACL_READ    0x04
#define GLUSTER_ACL_WRITE   0x02
#define GLUSTER_ACL_EXECUTE 0x01

#define GLUSTER_ACL_UNDEFINED_TAG  0x00
#define GLUSTER_ACL_USER_OBJ       0x01
#define GLUSTER_ACL_USER           0x02
#define GLUSTER_ACL_GROUP_OBJ      0x04
#define GLUSTER_ACL_GROUP          0x08
#define GLUSTER_ACL_MASK           0x10
#define GLUSTER_ACL_OTHER          0x20

#define GLUSTER_ACL_UNDEFINED_ID  (-1)

struct gluster_ace {
	uint16_t tag;
	uint16_t perm;
	uint32_t id;
};

struct gluster_acl_header {
	uint32_t version;
	struct gluster_ace entries[];
};

static int gluster_ace_cmp (const void *val1, const void *val2)
{
	const struct gluster_ace *ace1 = NULL;
	const struct gluster_ace *ace2 = NULL;
	int                        ret = 0;

	ace1 = val1;
	ace2 = val2;

	ret = (ace1->tag - ace2->tag);
	if (!ret)
		ret = (ace1->id - ace2->id);

	return ret;
}

static void gluster_acl_normalize (struct gluster_acl_header *acl, int count)
{
	qsort (acl->entries, count, sizeof (struct gluster_ace *),
	       gluster_ace_cmp);
}

static void gluster_acl_to_xattr (struct gluster_acl_header *acl, int count)
{
	struct gluster_ace *ace;
	int i;

	ace = acl->entries;

	SIVAL(&acl->version, 0, acl->version);

	for (i = 0; i < count; i++) {
		SSVAL(&ace->tag, 0, ace->tag);
		SSVAL(&ace->perm, 0, ace->perm);
		SIVAL(&ace->id, 0, ace->id);
		ace++;
	}
}

static SMB_ACL_T gluster_to_smb_acl(const char *buf, size_t xattr_size)
{
	int count;
	size_t size;
	struct gluster_ace *ace;
	struct smb_acl_entry *smb_ace;
	struct gluster_acl_header *hdr;
	struct smb_acl_t *result;
	int i;
	uint16_t tag;
	uint16_t perm;
	uint32_t id;

	size = xattr_size;

	if (size < sizeof(*hdr)) {
		/* ACL should be at least as big as the header */
		errno = EINVAL;
		return NULL;
	}

	size -= sizeof(*hdr);

	if (size % sizeof(*ace)) {
		/* Size of entries must strictly be a multiple of
		   size of an ACE
		*/
		errno = EINVAL;
		return NULL;
	}

	count = size / sizeof(*ace);

	hdr = (void *)buf;

	if (GLUSTER_ACL_VERSION != IVAL(&hdr->version, 0)) {
		DEBUG(0, ("Unknown gluster ACL version: %d\n",
			  IVAL(&hdr->version, 0)));
		return NULL;
	}

	result = SMB_MALLOC(sizeof(struct smb_acl_t) + (sizeof(struct smb_acl_entry) * count));
	if (!result) {
		errno = ENOMEM;
		return NULL;
	}

	result->count = count;

	smb_ace = result->acl;
	ace = hdr->entries;

	for (i = 0; i < count; i++) {
		tag = SVAL(&ace->tag, 0);

		switch(tag) {
		case GLUSTER_ACL_USER:
			smb_ace->a_type = SMB_ACL_USER;
			break;
		case GLUSTER_ACL_USER_OBJ:
			smb_ace->a_type = SMB_ACL_USER_OBJ;
			break;
		case GLUSTER_ACL_GROUP:
			smb_ace->a_type = SMB_ACL_GROUP;
			break;
		case GLUSTER_ACL_GROUP_OBJ:
			smb_ace->a_type = SMB_ACL_GROUP_OBJ;
			break;
		case GLUSTER_ACL_OTHER:
			smb_ace->a_type = SMB_ACL_OTHER;
			break;
		case GLUSTER_ACL_MASK:
			smb_ace->a_type = SMB_ACL_MASK;
			break;
		default:
			DEBUG(0, ("unknown tag type %d\n", (unsigned int) tag));
			return NULL;
		}

		id = IVAL(&ace->id, 0);

		switch(smb_ace->a_type) {
		case SMB_ACL_USER:
			smb_ace->uid = id;
			break;
		case SMB_ACL_GROUP:
			smb_ace->gid = id;
			break;
		default:
			break;
		}

		perm = SVAL(&ace->perm, 0);

		smb_ace->a_perm = 0;
		smb_ace->a_perm |=
			((perm & GLUSTER_ACL_READ) ? SMB_ACL_READ : 0);
		smb_ace->a_perm |=
			((perm & GLUSTER_ACL_WRITE) ? SMB_ACL_WRITE : 0);
		smb_ace->a_perm |=
			((perm & GLUSTER_ACL_EXECUTE) ? SMB_ACL_EXECUTE : 0);

		ace++;
		smb_ace++;
	}

	return result;
}

static ssize_t smb_to_gluster_acl(SMB_ACL_T theacl, char *buf, size_t len)
{
	ssize_t size;
	struct gluster_ace *ace;
	struct smb_acl_entry *smb_ace;
	struct gluster_acl_header *hdr;
	int i;
	int count;
	uint16_t tag;
	uint16_t perm;
	uint32_t id;

	count = theacl->count;

	size = sizeof(*hdr) + (count * sizeof(*ace));
	if (!buf) {
		return size;
	}

	if (len < size) {
		errno = ERANGE;
		return -1;
	}

	hdr = (void *)buf;
	ace = hdr->entries;
	smb_ace = theacl->acl;

	hdr->version = GLUSTER_ACL_VERSION;

	for (i = 0; i < count; i++) {
		switch(smb_ace->a_type) {
		case SMB_ACL_USER:
			tag = GLUSTER_ACL_USER;
			break;
		case SMB_ACL_USER_OBJ:
			tag = GLUSTER_ACL_USER_OBJ;
			break;
		case SMB_ACL_GROUP:
			tag = GLUSTER_ACL_GROUP;
			break;
		case SMB_ACL_GROUP_OBJ:
			tag = GLUSTER_ACL_GROUP_OBJ;
			break;
		case SMB_ACL_OTHER:
			tag = GLUSTER_ACL_OTHER;
			break;
		case SMB_ACL_MASK:
			tag = GLUSTER_ACL_MASK;
			break;
		default:
			DEBUG(0, ("Unknown tag value %d\n",
				  smb_ace->a_type));
			errno = EINVAL;
			return -1;
		}

		ace->tag = tag;

		switch(smb_ace->a_type) {
		case SMB_ACL_USER:
			id = smb_ace->uid;
			break;
		case SMB_ACL_GROUP:
			id = smb_ace->gid;
			break;
		default:
			id = GLUSTER_ACL_UNDEFINED_ID;
			break;
		}

		ace->id = id;

		ace->perm = 0;
		ace->perm |=
			((smb_ace->a_perm & SMB_ACL_READ) ? GLUSTER_ACL_READ : 0);
		ace->perm |=
			((smb_ace->a_perm & SMB_ACL_WRITE) ? GLUSTER_ACL_WRITE : 0);
		ace->perm |=
			((smb_ace->a_perm & SMB_ACL_EXECUTE) ? GLUSTER_ACL_EXECUTE : 0);

		ace++;
		smb_ace++;
	}

	gluster_acl_normalize (hdr, count);
	gluster_acl_to_xattr (hdr, count);

	return size;
}


static SMB_ACL_T vfs_gluster_sys_acl_get_file(struct vfs_handle_struct *handle,
					      const char *path_p,
					      SMB_ACL_TYPE_T type)
{
	struct smb_acl_t *result;
	char *buf;
	const char *key;
	ssize_t ret;

	switch (type) {
	case SMB_ACL_TYPE_ACCESS:
		key = "system.posix_acl_access";
		break;
	case SMB_ACL_TYPE_DEFAULT:
		key = "system.posix_acl_default";
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	ret = glfs_getxattr(handle->data, path_p, key, 0, 0);
	if (ret <= 0) {
		return NULL;
	}

	buf = alloca(ret);
	ret = glfs_getxattr(handle->data, path_p, key, buf, ret);
	if (ret <= 0) {
		return NULL;
	}

	result = gluster_to_smb_acl(buf, ret);

	return result;
}

static SMB_ACL_T vfs_gluster_sys_acl_get_fd(struct vfs_handle_struct *handle,
					    struct files_struct *fsp)
{
	struct smb_acl_t *result;
	int ret;
	char *buf;
	glfs_fd_t *glfd;

	glfd = *(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp);
	ret = glfs_fgetxattr(glfd, "system.posix_acl_access", 0, 0);
	if (ret <= 0) {
		return NULL;
	}

	buf = alloca(ret);
	ret = glfs_fgetxattr(glfd, "system.posix_acl_access", buf, ret);
	if (ret <= 0) {
		return NULL;
	}

	result = gluster_to_smb_acl(buf, ret);

	return result;
}

static int vfs_gluster_sys_acl_set_file(struct vfs_handle_struct *handle,
					const char *name,
					SMB_ACL_TYPE_T acltype,
					SMB_ACL_T theacl)
{
	int ret;
	const char *key;
	char *buf;
	ssize_t size;

	switch (acltype) {
	case SMB_ACL_TYPE_ACCESS:
		key = "system.posix_acl_access";
		break;
	case SMB_ACL_TYPE_DEFAULT:
		key = "system.posix_acl_default";
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	size = smb_to_gluster_acl(theacl, 0, 0);
	buf = alloca(size);

	size = smb_to_gluster_acl(theacl, buf, size);
	if (size == -1) {
		return -1;
	}

	ret = glfs_setxattr(handle->data, name, key, buf, size, 0);

	return ret;
}

static int vfs_gluster_sys_acl_set_fd(struct vfs_handle_struct *handle,
				      struct files_struct *fsp,
				      SMB_ACL_T theacl)
{
	int ret;
	char *buf;
	ssize_t size;

	size = smb_to_gluster_acl(theacl, 0, 0);
	buf = alloca(size);

	size = smb_to_gluster_acl(theacl, buf, size);
	if (size == -1) {
		return -1;
	}

	ret = glfs_fsetxattr(*(glfs_fd_t **)VFS_FETCH_FSP_EXTENSION(handle, fsp),
			     "system.posix_acl_access", buf, size, 0);
	return ret;
}

static int vfs_gluster_sys_acl_delete_def_file(struct vfs_handle_struct *handle,
					       const char *path)
{
	return glfs_removexattr(handle->data, path, "system.posix_acl_default");
}

static struct vfs_fn_pointers glusterfs_fns = {

	/* Disk Operations */

	.connect_fn = vfs_gluster_connect,
	.disconnect = vfs_gluster_disconnect,
	.disk_free = vfs_gluster_disk_free,
	.get_quota = vfs_gluster_get_quota,
	.set_quota = vfs_gluster_set_quota,
	.statvfs = vfs_gluster_statvfs,
	.fs_capabilities = vfs_gluster_fs_capabilities,

	/* Directory Operations */

	.opendir = vfs_gluster_opendir,
	.fdopendir = vfs_gluster_fdopendir,
	.readdir = vfs_gluster_readdir,
	.seekdir = vfs_gluster_seekdir,
	.telldir = vfs_gluster_telldir,
	.rewind_dir = vfs_gluster_rewinddir,
	.mkdir = vfs_gluster_mkdir,
	.rmdir = vfs_gluster_rmdir,
	.closedir = vfs_gluster_closedir,
	.init_search_op = vfs_gluster_init_search_op,

	/* File Operations */

	.open_fn = vfs_gluster_open,
	.create_file = NULL,
	.close_fn = vfs_gluster_close,
	.vfs_read = vfs_gluster_read,
	.pread = vfs_gluster_pread,
	.write = vfs_gluster_write,
	.pwrite = vfs_gluster_pwrite,
	.lseek = vfs_gluster_lseek,
	.sendfile = vfs_gluster_sendfile,
	.recvfile = vfs_gluster_recvfile,
	.rename = vfs_gluster_rename,
	.fsync = vfs_gluster_fsync,
	.stat = vfs_gluster_stat,
	.fstat = vfs_gluster_fstat,
	.lstat = vfs_gluster_lstat,
	.get_alloc_size = vfs_gluster_get_alloc_size,
	.unlink = vfs_gluster_unlink,

	.chmod = vfs_gluster_chmod,
	.fchmod = vfs_gluster_fchmod,
	.chown = vfs_gluster_chown,
	.fchown = vfs_gluster_fchown,
	.lchown = vfs_gluster_lchown,
	.chdir = vfs_gluster_chdir,
	.getwd = vfs_gluster_getwd,
	.ntimes = vfs_gluster_ntimes,
	.ftruncate = vfs_gluster_ftruncate,
	.fallocate = vfs_gluster_fallocate,
	.lock = vfs_gluster_lock,
	.kernel_flock = vfs_gluster_kernel_flock,
	.linux_setlease = vfs_gluster_linux_setlease,
	.getlock = vfs_gluster_getlock,
	.symlink = vfs_gluster_symlink,
	.vfs_readlink = vfs_gluster_readlink,
	.link = vfs_gluster_link,
	.mknod = vfs_gluster_mknod,
	.realpath = vfs_gluster_realpath,
	.notify_watch = vfs_gluster_notify_watch,
	.chflags = vfs_gluster_chflags,
	.file_id_create = NULL,
	.streaminfo = NULL,
	.get_real_filename = vfs_gluster_get_real_filename,
	.connectpath = vfs_gluster_connectpath,

	.brl_lock_windows = NULL,
	.brl_unlock_windows = NULL,
	.brl_cancel_windows = NULL,
	.strict_lock = NULL,
	.strict_unlock = NULL,
	.translate_name = NULL,

	/* NT ACL Operations */
	.fget_nt_acl = NULL,
	.get_nt_acl = NULL,
	.fset_nt_acl = NULL,

	/* Posix ACL Operations */
	.chmod_acl = NULL,	/* passthrough to default */
	.fchmod_acl = NULL,	/* passthrough to default */

	.sys_acl_get_entry = NULL,
	.sys_acl_get_tag_type = NULL,
	.sys_acl_get_permset = NULL,
	.sys_acl_get_qualifier = NULL,
	.sys_acl_get_file = vfs_gluster_sys_acl_get_file,
	.sys_acl_get_fd = vfs_gluster_sys_acl_get_fd,
	.sys_acl_clear_perms = NULL,
	.sys_acl_add_perm = NULL,
	.sys_acl_to_text = NULL,
	.sys_acl_init = NULL,
	.sys_acl_create_entry = NULL,
	.sys_acl_set_tag_type = NULL,
	.sys_acl_set_qualifier = NULL,
	.sys_acl_set_permset = NULL,
	.sys_acl_valid = NULL,
	.sys_acl_set_file = vfs_gluster_sys_acl_set_file,
	.sys_acl_set_fd = vfs_gluster_sys_acl_set_fd,
	.sys_acl_delete_def_file = vfs_gluster_sys_acl_delete_def_file,
	.sys_acl_get_perm = NULL,
	.sys_acl_free_text = NULL,
	.sys_acl_free_acl = NULL,
	.sys_acl_free_qualifier = NULL,

	/* EA Operations */
	.getxattr = vfs_gluster_getxattr,
	.lgetxattr = vfs_gluster_lgetxattr,
	.fgetxattr = vfs_gluster_fgetxattr,
	.listxattr = vfs_gluster_listxattr,
	.llistxattr = vfs_gluster_llistxattr,
	.flistxattr = vfs_gluster_flistxattr,
	.removexattr = vfs_gluster_removexattr,
	.lremovexattr = vfs_gluster_lremovexattr,
	.fremovexattr = vfs_gluster_fremovexattr,
	.setxattr = vfs_gluster_setxattr,
	.lsetxattr = vfs_gluster_lsetxattr,
	.fsetxattr = vfs_gluster_fsetxattr,

	/* AIO Operations */
	.aio_read = NULL,
	.aio_write = NULL,
	.aio_return_fn = NULL,
	.aio_cancel = NULL,
	.aio_error_fn = NULL,
	.aio_fsync = NULL,
	.aio_suspend = NULL,
	.aio_force = vfs_gluster_aio_force,

	/* Offline Operations */
	.is_offline = vfs_gluster_is_offline,
	.set_offline = vfs_gluster_set_offline,
};

NTSTATUS vfs_glusterfs_init(void);
NTSTATUS vfs_glusterfs_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"glusterfs", &glusterfs_fns);
}
