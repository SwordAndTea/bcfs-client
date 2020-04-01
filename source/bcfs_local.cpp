//
// Created by 向尉 on 2020/3/3.
//


#define _GNU_SOURCE

#include <fuse.h>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cerrno>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <string>
#if __APPLE__
    #define HAVE_FSETATTR_X
#endif


using std::string;

static const char *info_dir = "/.UserInfos";

static string dir_prefix;

#define TEST_RES(res) if ((res) == -1) return -errno

#define REAL_PATH(path) (dir_prefix + string((path))).data()

#define G_PREFIX                       "org"
#define G_KAUTH_FILESEC_XATTR G_PREFIX ".apple.system.Security"
#define A_PREFIX                       "com"
#define A_KAUTH_FILESEC_XATTR A_PREFIX ".apple.system.Security"
#define XATTR_APPLE_PREFIX             "com.apple."


static int bcfs_local_getattr(const char *path, struct stat *stbuf) {
    if (strcmp(path, "/") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 1;
    } else if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        int res = lstat(REAL_PATH(path), stbuf);
        TEST_RES(res);
    } else {
        return -ENOENT;
    }
    return 0;
}


struct DirPointer {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

/**
 * @code directory operation
 * */

static int bcfs_local_opendir(const char *path, struct fuse_file_info *fi) {
    if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        DirPointer *d = new(std::nothrow) DirPointer;
        if (d == nullptr) {
            return -ENOMEM;
        }

        d->dp = opendir(REAL_PATH(path));
        if (d->dp == nullptr) {
            delete d;
            return -errno;
        }

        d->offset = 0;
        d->entry = nullptr;
        fi->fh = (unsigned long) d;
    }
    return 0;

}

static inline struct DirPointer * get_dir_pointer(struct fuse_file_info *fi) {
    return (DirPointer *) (uintptr_t) fi->fh;
}

static int bcfs_local_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") == 0) {
        filler(buf, info_dir + 1, nullptr, 0);
    } else if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        struct DirPointer *d = get_dir_pointer(fi);
        if (offset == 0) {
            rewinddir(d->dp);
            d->entry = nullptr;
            d->offset = 0;
        } else if (offset != d->offset) {
            seekdir(d->dp, offset);
            d->entry = nullptr;
            d->offset = offset;
        }

        while (true) {
            struct stat st;
            off_t nextoff;

            if (!d->entry) {
                d->entry = readdir(d->dp);
                if (!d->entry) {
                    break;
                }
            }
            memset(&st, 0, sizeof(st));
            st.st_ino = d->entry->d_ino;
            st.st_mode = d->entry->d_type << 12;
            nextoff = telldir(d->dp);
            if (filler(buf, d->entry->d_name, &st, nextoff)) {
                break;
            }

            d->entry = nullptr;
            d->offset = nextoff;
        }
    }
    return 0;
}

static int bcfs_local_releasedir(const char *path, struct fuse_file_info *fi) {
    if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        struct DirPointer *d = get_dir_pointer(fi);
        closedir(d->dp);
        free(d);
    }
    return 0;
}

static int bcfs_local_mkdir(const char *path, mode_t mode) {
    int res = mkdir(REAL_PATH(path), mode);
    TEST_RES(res);
    return 0;
}



static int bcfs_local_rmdir(const char *path) {
    int res = rmdir(REAL_PATH(path));
    TEST_RES(res);
    return 0;
}

/**
 * @endcode directory operation
 * */

/**
 * @code file operation
 * */

static int bcfs_local_create_file(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int fd = open(REAL_PATH(path), fi->flags, mode);
    TEST_RES(fd);
    fi->fh = fd;
    return 0;
}

static int bcfs_local_open_file(const char *path, struct fuse_file_info *fi) {
    int fd = open(REAL_PATH(path), fi->flags);
    TEST_RES(fd);

    fi->fh = fd;
    fi->direct_io = true;
    return 0;
}

static int bcfs_local_read_file(const char *path, char *buf, size_t size, off_t offset,
                                struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    TEST_RES(res);
    return res;
}

static int bcfs_local_write_file(const char *path, const char *buf, size_t size,
                                 off_t offset, struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);
    TEST_RES(res);
    return res;
}

static int bcfs_local_unlink_file(const char *path) {
    int res = unlink(REAL_PATH(path));
    TEST_RES(res);
    return 0;
}

static int bcfs_local_flush_file(const char *path, struct fuse_file_info *fi) {
    int res = close(dup(fi->fh));
    TEST_RES(res);
    return 0;
}

static int bcfs_local_release_file(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    return 0;
}

static int bcfs_local_fsync_file(const char *path, int isdatasync, struct fuse_file_info *fi) {
    int res = fsync(fi->fh);
    TEST_RES(res);
    return 0;
}


static int bcfs_local_fallocate_file(const char *path, int mode, off_t offset, off_t length,
                                     struct fuse_file_info *fi) {
    fstore_t fstore;

    if (!(mode & PREALLOCATE)) {
        return -ENOTSUP;
    }

    fstore.fst_flags = 0;
    if (mode & ALLOCATECONTIG) {
        fstore.fst_flags |= F_ALLOCATECONTIG;
    }
    if (mode & ALLOCATEALL) {
        fstore.fst_flags |= F_ALLOCATEALL;
    }

    if (mode & ALLOCATEFROMPEOF) {
        fstore.fst_posmode = F_PEOFPOSMODE;
    } else if (mode & ALLOCATEFROMVOL) {
        fstore.fst_posmode = F_VOLPOSMODE;
    }

    fstore.fst_offset = offset;
    fstore.fst_length = length;

    if (fcntl(fi->fh, F_PREALLOCATE, &fstore) == -1) {
        return -errno;
    } else {
        return 0;
    }
}

/**
 * @endcode file operation
 * */

static int bcfs_local_rename(const char *from, const char *to) {
    int res = rename(REAL_PATH(from), REAL_PATH(to));
    TEST_RES(res);
    return 0;
}


void *bcfs_local_init(struct fuse_conn_info *conn) {
    return nullptr;
}

static struct fuse_operations bcfs_local_oper = {
        .init        = bcfs_local_init,
        .getattr     = bcfs_local_getattr,

        .opendir     = bcfs_local_opendir,
        .readdir     = bcfs_local_readdir,
        .releasedir  = bcfs_local_releasedir,
        .mkdir       = bcfs_local_mkdir,
        .rmdir       = bcfs_local_rmdir,

        .create      = bcfs_local_create_file,
        .open        = bcfs_local_open_file,
        .read        = bcfs_local_read_file,
        .write       = bcfs_local_write_file,
        .unlink      = bcfs_local_unlink_file,
        .flush       = bcfs_local_flush_file,
        .release     = bcfs_local_release_file,
        .fsync       = bcfs_local_fsync_file,
        .fallocate   = bcfs_local_fallocate_file,

        .rename      = bcfs_local_rename,
};


int main(int argc, char *argv[]) {
    dir_prefix = getenv("HOME");
    return fuse_main(argc, argv, &bcfs_local_oper, nullptr);
}
