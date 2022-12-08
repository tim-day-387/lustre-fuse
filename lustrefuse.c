/*
    liblustre-over-FUSE

    Derived in part from the example FUSE module, which is copyright
    2001-2006 Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL
    version 2.  See the file COPYING.

    IMPORTANT NOTES
    ---------------

    libsysio doesn't properly track network and domain sockets, so
    FUSE's attempts to communicate with the kernel will return -EBADF.
    To avoid this, we build libsysio and lustrefuse with SYSIO_LABEL_NAMES,
    which places the sysio calls in their own namespace.  If sysio is taught
    to handle non-file FDs properly, this requirement could be removed.

    FUSE requires FILE_OFFSET_BITS=64, and sysio by default is not built
    with that option.  If you don't provide it, they'll have differently
    sized "struct stat" buffers, and nothing will work.

    Lastly, as of this writing, libsysio does not have a statvfs()
    function, so a minimal patch is provided.  To enable it, build with
    -D_HAVE_STATVFS

    To accomplish all this, run the following command at the top of your
    Lustre tree:

    make CFLAGS='-DSYSIO_LABEL_NAMES="fuse_" -D_FILE_OFFSET_BITS=64 -D_HAVE_STATVFS'

    To run lustrefuse, try lustrefuse -h
*/

#define FUSE_USE_VERSION 26

#include <config.h>

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#define _PREPEND_HELPER(p, x) \
        p ## x
#define PREPEND(p, x) \
        _PREPEND_HELPER(p, x)

/*
 * SYSIO name label macros
 */
#ifdef SYSIO_LABEL_NAMES
#define SYSIO_INTERFACE_NAME(x) \
        PREPEND(SYSIO_LABEL_NAMES, x)
#else
#define SYSIO_INTERFACE_NAME(x) x
#endif

static char *lustre_mountpoint;

static char *real_path(const char *path)
{
    char *real = malloc(strlen(path) + strlen(lustre_mountpoint) + 1);

    memcpy(real, lustre_mountpoint, strlen(lustre_mountpoint));
    memcpy(real + strlen(lustre_mountpoint), path, strlen(path));
    real[strlen(lustre_mountpoint) + strlen(path)] = '\0';

    return real;
}

static int lfuse_getattr(const char *path, struct stat *stbuf)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(lstat)(path, stbuf);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_access(const char *path, int mask)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(access)(path, mask);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_readlink(const char *path, char *buf, size_t size)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(readlink)(path, buf, size - 1);
    free(path);

    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int lfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;

    path = real_path(path);
    dp = (DIR *)SYSIO_INTERFACE_NAME(opendir)(path);
    free(path);
    if (dp == NULL)
        return -errno;

    while ((de = (struct dirent *)SYSIO_INTERFACE_NAME(readdir)(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    SYSIO_INTERFACE_NAME(closedir)(dp);
    return 0;
}

static int lfuse_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    path = real_path(path);
    if (S_ISREG(mode)) {
        res = SYSIO_INTERFACE_NAME(open)(path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = SYSIO_INTERFACE_NAME(close)(res);
    } else if (S_ISFIFO(mode)) {
        return -ENOSYS;
        // res = SYSIO_INTERFACE_NAME(mkfifo)(path, mode);
    } else {
        res = SYSIO_INTERFACE_NAME(mknod)(path, mode, rdev);
    }
    free(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_mkdir(const char *path, mode_t mode)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(mkdir)(path, mode);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_unlink(const char *path)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(unlink)(path);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_rmdir(const char *path)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(rmdir)(path);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_symlink(const char *from, const char *to)
{
    int res;

    from = real_path(from);
    to = real_path(to);
    res = SYSIO_INTERFACE_NAME(symlink)(from, to);
    free(from);
    free(to);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_rename(const char *from, const char *to)
{
    int res;

    from = real_path(from);
    to = real_path(to);
    res = SYSIO_INTERFACE_NAME(rename)(from, to);
    free(from);
    free(to);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_link(const char *from, const char *to)
{
    int res;

    from = real_path(from);
    to = real_path(to);
    res = SYSIO_INTERFACE_NAME(link)(from, to);
    free(from);
    free(to);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_chmod(const char *path, mode_t mode)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(chmod)(path, mode);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(chown)(path, uid, gid); // should be lchown, but sysio doesn't support that
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_truncate(const char *path, off_t size)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(truncate)(path, size);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    struct utimbuf timbuf;
    struct timeval tv[2];

    timbuf.actime = ts[0].tv_sec;
    timbuf.modtime = ts[1].tv_sec;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(utime)(path, &timbuf);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_open(const char *path, struct fuse_file_info *fi)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(open)(path, fi->flags);
    free(path);

    if (res == -1)
        return -errno;

    fi->fh = res;

    return 0;
}

static int lfuse_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int fd = fi->fh;
    int res;

    if (fd == 0)
        return -EBADF;

    res = SYSIO_INTERFACE_NAME(pread)(fd, buf, size, offset);

    if (res == -1)
        res = -errno;

    return res;
}

static int lfuse_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int fd = fi->fh;
    int res;

    if (fd == 0)
        return -EBADF;

    res = SYSIO_INTERFACE_NAME(pwrite)(fd, buf, size, offset);

    if (res == -1)
        res = -errno;

    return res;
}

static int lfuse_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    path = real_path(path);
    res = SYSIO_INTERFACE_NAME(statvfs)(path, stbuf);
    free(path);

    if (res == -1)
        return -errno;

    return 0;
}

static int lfuse_release(const char *path, struct fuse_file_info *fi)
{
    int res;

    if (fi->fh == 0)
	return -EBADF;

    res = SYSIO_INTERFACE_NAME(close)(fi->fh);

    if (res < 0)
	return -errno;

    return 0;
}

static int lfuse_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    int res;

    if (fi->fh == 0)
        return -EBADF;

    res = SYSIO_INTERFACE_NAME(fsync)(fi->fh);
    if (res < 0)
        return -errno;

    return 0;
}

// Leaving this in, in case liblustre gets xattrs some day
#undef HAVE_SETXATTR

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int lfuse_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    int res = SYSIO_INTERFACE_NAME(lsetxattr)(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int lfuse_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    int res;

    path = real_path(path);
    res = lgetxattr(path, name, value, size);
    free(path);

    if (res == -1)
        return -errno;
    return res;
}

static int lfuse_listxattr(const char *path, char *list, size_t size)
{
    int res;

    path = real_path(path);
    res = llistxattr(path, list, size);
    free(path);

    if (res == -1)
        return -errno;
    return res;
}

static int lfuse_removexattr(const char *path, const char *name)
{
    int res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations lfuse_oper = {
    .getattr	= lfuse_getattr,
    .access	= lfuse_access,
    .readlink	= lfuse_readlink,
    .readdir	= lfuse_readdir,
    .mknod	= lfuse_mknod,
    .mkdir	= lfuse_mkdir,
    .symlink	= lfuse_symlink,
    .unlink	= lfuse_unlink,
    .rmdir	= lfuse_rmdir,
    .rename	= lfuse_rename,
    .link	= lfuse_link,
    .chmod	= lfuse_chmod,
    .chown	= lfuse_chown,
    .truncate	= lfuse_truncate,
    .utimens	= lfuse_utimens,
    .open	= lfuse_open,
    .read	= lfuse_read,
    .write	= lfuse_write,
    .statfs	= lfuse_statfs,
    .release	= lfuse_release,
    .fsync	= lfuse_fsync,
#ifdef HAVE_SETXATTR
    .setxattr	= lfuse_setxattr,
    .getxattr	= lfuse_getxattr,
    .listxattr	= lfuse_listxattr,
    .removexattr= lfuse_removexattr,
#endif
};

int main(int argc, char *argv[])
{
    struct fuse *fuse;
    char *mountpoint, *home;
    int rc;

    umask(0);
    home = getenv("HOME");
    if (home == NULL) {
        printf("lustrefuse: fatal error, HOME environment variable not set\n");
        return 1;
    }
    lustre_mountpoint = malloc(strlen(home) + strlen("/.lustrefuse") + 1);
    if (lustre_mountpoint == NULL) {
        printf("lustrefuse: fatal error, malloc failed\n");
        return 1;
    }
    memcpy(lustre_mountpoint, home, strlen(home));
    memcpy(lustre_mountpoint + strlen(home), "/.lustrefuse",
           strlen("/.lustrefuse") + 1);
    rc = mkdir(lustre_mountpoint, 0700);
    if (rc != 0 && errno != EEXIST) {
        printf("lustrefuse: couldn't create temporary directory %s: %s\n",
               lustre_mountpoint, strerror(errno));
        return 1;
    }

    setenv("LIBLUSTRE_MOUNT_POINT", lustre_mountpoint, 1);

    fuse = fuse_setup(argc, argv, &lfuse_oper, sizeof(lfuse_oper),
                      &mountpoint, NULL, NULL);
    if (fuse == NULL)
      return 1;

    __liblustre_setup_();

    fuse_loop(fuse);
    fuse_teardown(fuse, mountpoint);
}
