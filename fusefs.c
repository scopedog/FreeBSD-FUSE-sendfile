/***************************************************************************

	Author: Hiroshi Nishida, nishida@asusa.net

	fusefs.c
	Interface for FUSE

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include <sys/mount.h>
#define	FUSE_USE_VERSION	30 // Must be defined before fuse.h
#include <fuse.h>
#include <curl/curl.h>
#include "fusetest.h"
#include "log.h"
#ifdef EXTERN
#undef EXTERN
#endif
#include "util.h"

/**************************************************************************
	Definitions
**************************************************************************/

/**************************************************************************
	Functions
**************************************************************************/

// Read dummy stat file
static int
ReadDummyStatF(struct stat *sb)
{
	int	err = 0;
	FILE	*fp = NULL;

	// Open dummy stat file
	if ((fp = fopen(DUMMY_STAT_FILE, "rb")) == NULL) {
		Log("Error: %s: fopen %s: %s",
			__func__, DUMMY_STAT_FILE, strerror(errno));
		err = errno;
		goto END;
	}

	// Read
	if (fread(sb, 1, sizeof(struct stat), fp) < sizeof(struct stat)) {
		int	e = ferror(fp);

		if (e) {
			Log("Error: %s: fread %s: %s",
				__func__, DUMMY_STAT_FILE, strerror(e));
			err = errno = e;
		}
		else {
			Log("Error: %s: fread %s: Unknown error",
				__func__, DUMMY_STAT_FILE);
			err = errno = EPERM;
		}
		goto END;
	}

END:	// Finalize
	if (fp != NULL) {
		fclose(fp);
	}

	return err ? -1 : 0;
}

// Get attributes
static int
FuseGetattr(const char *path, struct stat *sb)
{
	char	*ppath;
	int	err = 0;

	DebugMsg("%s: path: %s\n", __func__, path);

	// Skip leading / in path
	for (ppath = (char *)path; *ppath == '/'; ppath++);

	// Check path
	if (strcmp(ppath, VIDEO_FILE) == 0) { // VIDEO_FILE
		// Read dummy stat file
		if (ReadDummyStatF(sb) == -1) {
			Log("Error: ReadDummyStatF: %s: %s",
				path, strerror(errno));
			err = errno;
		}
	}

	return -err;
}

// Statfs
static int
FuseStatfs(const char *path, struct statvfs *sf)
{
	int	err = 0;

	DebugMsg("%s: path: %s\n", __func__, path);

	// Statfs
	errno = 0;
	if (statvfs("/", sf)) {
		Log("Error: statvfs: %s: %s", path, strerror(errno));
		err = errno;
	}

	return -err;
}

// Open dir
static int 
FuseOpendir(const char *path, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s\n", __func__, path);

	return 0;
}

// Read dir
static int 
FuseReaddir(const char *path, void *fuse_buf, fuse_fill_dir_t filler,
	    off_t offset, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s\n", __func__, path);

	return 0;
}

// Close dir
static int 
FuseReleasedir(const char *path, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s\n", __func__, path);

	return 0;
}

// Create
static int 
FuseCreate(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s, mode: %d\n", __func__, path, mode);

	return 0;
}

// Open
static int 
FuseOpen(const char *path, struct fuse_file_info *fi)
{
	int	err = 0, mode;

	DebugMsg("%s: path: %s, flags: %d\n", __func__, path, fi->flags);

	// Initialize
	fi->direct_io = 1;

	// Get mode and check
	mode = fi->flags & O_ACCMODE;
	if (mode != O_RDONLY) {
		Log("Error: %s accepts O_RDONLY only", __func__);
		err = errno = EPERM;
		goto END;
	}

END:	// Finalize

	return -err;
}

// Write downloaded data
static size_t
WriteDownloadedData(void *data, size_t size, size_t nmemb, void *fp)
{
	return fwrite(data, size, nmemb, (FILE *)fp);
}

// Read
static int 
FuseRead(const char *path, char *data, size_t size, off_t offset,
	 struct fuse_file_info *fi)
{
	char		buf[BUFSIZ];
	int		err = 0;
	ssize_t		ssize = 0;
	FILE		*fp = NULL;
	CURL		*curl = NULL;
	CURLcode	res;

	DebugMsg("%s: path: %s, fi->fh: %p, size: %d, offset: %d\n",
		 __func__, path, fi->fh, size, offset);

	// Initialize curl
	if ((curl = curl_easy_init()) == NULL) {
		Log("Error: %s: curl_easy_init: %s", __func__, strerror(errno));
		err = errno;
		goto END;
	}

	// Open file
	remove(CACHE_FILE);
	if ((fp = fopen(CACHE_FILE, "w+")) == NULL) {
		Log("Error: %s: fopen %s: %s",
			__func__, CACHE_FILE, strerror(errno));
		err = errno;
		goto END;
	}

	// Set opt
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	snprintf(buf, sizeof(buf), "%ld-%ld", offset, offset + size - 1);
	curl_easy_setopt(curl, CURLOPT_RANGE, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteDownloadedData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	// Get
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		Log("Error: %s: curl_easy_perform: %s",
			__func__, curl_easy_strerror(res));
		err = errno = EPERM;
		goto END;
	}

	// Read
	rewind(fp);
	clearerr(fp);
	if ((ssize = fread(data, 1, size, fp)) < size) {
		int	e = ferror(fp);

		if (e) {
			Log("Error: %s: fread %s: %s",
				__func__, CACHE_FILE, strerror(e));
			err = errno = e;
			goto END;
		}
	}

END:	// Finalize
	if (fp != NULL) {
		fclose(fp);
	}
	if (curl != NULL) {
		curl_easy_cleanup(curl);
	}

	return err ? -err : (int)ssize;
}

/** Release an open file
         *
         * Release is called when there are no more references to an open
         * file: all file descriptors are closed and all memory mappings
         * are unmapped.
         *
         * For every open() call there will be exactly one release() call
         * with the same flags and file descriptor.      It is possible to
         * have a file opened more than once, in which case only the last
         * release will mean, that no more reads/writes will happen on the
         * file.  The return value of release is ignored.  */
static int
FuseRelease(const char *path, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s, fi->fh: %p\n", __func__, path, fi->fh);

	return 0;
}

// Truncate
static int
FuseTruncate(const char *path, off_t size)
{
	DebugMsg("%s: path: %s, size: %d\n", __func__, path, size);

	return 0;
}

// Ftruncate
static int
FuseFtruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s, size: %d\n", __func__, path, size);

	return 0;
}

// Remove
static int
FuseUnlink(const char *path)
{
	DebugMsg("%s: path: %s\n", __func__, path);

	return 0;
}

// Rename
static int
FuseRename(const char *from, const char *to)
{
	DebugMsg("%s: from: %s, to: %s\n", __func__, from, to);

	return 0;
}

// Mkdir
static int
FuseMkdir(const char *path, mode_t mode)
{
	int			err = 0;
	struct fuse_context	*fc;

	DebugMsg("%s: path: %s\n", __func__, path);

	// Get uid and gid
	fc = fuse_get_context(); 

	// Create at local cache
	if (mkdir(path, mode) < 0) {
		Log("Error: FuseMkdir: mkdir %s: %s", path, strerror(errno));
		err = errno;
		goto END;
	}

	// Chown cache
	if (chown(path, fc->uid, fc->gid) < 0) {
		Log("Error: FuseMkdir: chown %s: %s", path, strerror(errno));
		err = errno; // Let's just ignore...
	}

END:	// Finalize
	return -err;
}

// Chown
static int 
FuseChown(const char *path, uid_t uid, gid_t gid)
{
	int	err = 0;

	DebugMsg("%s: path: %s, uid: %u, gid: %u\n", __func__, path, uid, gid);

	// Chown
	if (chown(path, uid, gid) < 0) {
		Log("Error: FuseChown: chown %s: %s", path, strerror(errno));
		err = errno; // Let's just ignore...
	}

	return 0;
}

// chmod -- Change mode
static int
FuseChmod(const char *path, mode_t mode)
{
	int	err = 0;

	DebugMsg("%s: path: %s, mode: %u\n", __func__, path, mode);

	// Chmod
	chmod(path, mode);
	if (chmod(path, mode) < 0) {
		Log("Error: : chown %s: %s", path, strerror(errno));
		err = errno; // Let's just ignore...
	}

	return -errno;
}

// Flush
static int
FuseFlush(const char *path, struct fuse_file_info *fi)
{
	DebugMsg("%s: path: %s, fi: %p\n", __func__, path, fi);

	// Do nothing
	return 0;
}

// Fsync
static int
FuseFsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	DebugMsg("%s: isdatasync: %d, path: %s, fi: %p\n",
		__func__, isdatasync, path, fi);

	// Do nothing
	return 0;
}

/* FUSE file system loop */
int 
FuseLoop(int argc, char *argv[])
{
	static struct fuse_operations fuse_op = {
		.getattr = FuseGetattr,
		.statfs = FuseStatfs,
		.opendir = FuseOpendir,
		.readdir = FuseReaddir,
		.releasedir = FuseReleasedir,
		.create = FuseCreate,
		.open = FuseOpen,
		.read = FuseRead,
		//.write = FuseWrite,
		.release = FuseRelease,
		.truncate = FuseTruncate,
		.ftruncate = FuseFtruncate,
		.unlink = FuseUnlink,
		.rename = FuseRename,
		.mkdir = FuseMkdir,
		.chown = FuseChown,
		.chmod = FuseChmod,
		.flush = FuseFlush,
		.fsync = FuseFsync,
		//.readlink = FuseReadlink,
	};

	return fuse_main(argc, argv, &fuse_op, NULL);
}
