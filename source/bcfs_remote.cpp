//
// Created by 向尉 on 2020/3/3.
//
#include <iostream>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <fuse.h>
#include <cerrno>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <unordered_map>
#include <set>
#include "sftp_reachbility_manager.h"

using std::string;
using std::unordered_map;
using std::set;
using BCFS::SFTPReachbilityMananer;


#define REAL_PATH(path) (dir_prefix + string((path))).data()

#define Lstat_Handle SFTPReachbilityMananer::get_lstat_sftp_session()

#define Write_Handle SFTPReachbilityMananer::get_read_sftp_session()

#define Read_Handle SFTPReachbilityMananer::get_write_sftp_session()

static const char *info_dir = "/.UserInfos";

static string dir_prefix;

static unordered_map<string, LIBSSH2_SFTP_ATTRIBUTES> caches;

static set<string> cached_files;

static set<string> actual_files;

static bool exit_flag = false;

static pthread_mutex_t cache_mutex;

static pthread_mutex_t update_mutex;

//static int update_count = 0;
//
//static pthread_mutex_t count_mutex;

static pthread_mutex_t sftp_mutex;

#define GET_ALL_LOCK do {\
    pthread_mutex_lock(&cache_mutex); \
    pthread_mutex_lock(&update_mutex); \
    pthread_mutex_lock(&sftp_mutex); \
} while(0)

#define RELEASE_ALL_LOCK do { \
    pthread_mutex_unlock(&sftp_mutex); \
    pthread_mutex_unlock(&update_mutex); \
    pthread_mutex_unlock(&cache_mutex); \
} while(0)

//#define INCREASE_UPDATE_COUNT \
//do {\
//    pthread_mutex_lock(&count_mutex);\
//    ++update_count; \
//    if (update_count == 1) {\
//        pthread_mutex_lock(&update_mutex);\
//    }\
//    pthread_mutex_unlock(&count_mutex);\
//} while(0)

//#define DECREASE_UPDATE_COUNT \
//do {\
//    pthread_mutex_lock(&count_mutex);\
//    --update_count; \
//    if (update_count == 0) {\
//        pthread_mutex_unlock(&update_mutex);\
//    }\
//    pthread_mutex_unlock(&count_mutex);\
//} while(0)

static int bcfs_remote_get_attribute(const char *path, struct stat *file_state) {
    if (cached_files.find(path) == cached_files.end()) {
        return -ENOENT;
    }
    if (strcmp(path, "/") == 0) { /* The root directory of our file system. */
        memset(file_state, 0, sizeof(struct stat));
        file_state->st_mode = S_IFDIR | 0777; //rwx-rwx-rwx
        file_state->st_nlink = 1;
    } else if (strncmp(path, info_dir, strlen(info_dir)) == 0) { //the info_dir and it's files in it
        LIBSSH2_SFTP_ATTRIBUTES attribute;
        GET_ALL_LOCK;
        if (caches.find(REAL_PATH(path)) != caches.end()) {
            attribute = caches[REAL_PATH(path)];
        } else {
            int res = libssh2_sftp_lstat(Lstat_Handle, REAL_PATH(path), &attribute);
            //fprintf(stderr, "%s %s", "real path is" , REAL_PATH(path));
            if (res != 0) {
                RELEASE_ALL_LOCK;
                return -ENOENT;
            }
            caches[REAL_PATH(path)] = attribute;
        }
        RELEASE_ALL_LOCK;
        if (attribute.permissions & S_IFDIR) {
            file_state->st_mode = attribute.permissions | 0777;
        } else if (attribute.permissions & S_IFREG) {
            file_state->st_mode = attribute.permissions | 0666;
        }
        file_state->st_gid = attribute.gid;
        file_state->st_uid = attribute.uid;
        file_state->st_size = attribute.filesize;
        file_state->st_atimespec.tv_sec = attribute.atime;
        file_state->st_mtimespec.tv_sec = attribute.mtime;
    } else {/*reject other file. */
        return -ENOENT;
    }
    return 0;
}


/**
 * @code directory operation
 * */


//create a directory
static int bcfs_remote_mkdir(const char *path, mode_t mode) {
    GET_ALL_LOCK;
    int res = libssh2_sftp_mkdir(Lstat_Handle, REAL_PATH(path), mode);
    if (res != 0) {
        RELEASE_ALL_LOCK;
        return EEXIST;
    }
    RELEASE_ALL_LOCK;
    return 0;
}

static int bcfs_remote_open_dir(const char *path, fuse_file_info *fi) {
    return 0;
}

static int bcfs_remote_read_dir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") == 0) {//如果是根目录
        filler(buf, info_dir + 1, nullptr, 0);
    } else if (strcmp(path, info_dir) == 0) {
        for (auto & file : actual_files) {
            filler(buf, file.data(), nullptr, 0);
        }
    }
    return 0;
}

static int bcfs_remote_release_dir(const char *path, fuse_file_info *fi) {
    return 0;
}

static int bcfs_remote_rm_dir(const char *path) {
    GET_ALL_LOCK;
    int res = libssh2_sftp_rmdir(Lstat_Handle, REAL_PATH(path));
    if (res != 0) {
        RELEASE_ALL_LOCK;
        return -ENOENT;
    }
    caches.erase(string(REAL_PATH(path)));
    RELEASE_ALL_LOCK;
    return 0;
}

/**
 * @endcode directory operation
 * */

/**
 * @code file operation
 * */

static int bcfs_remote_create_file(const char *path, mode_t mode, struct fuse_file_info *fi) {
    GET_ALL_LOCK;
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(Write_Handle, REAL_PATH(path), LIBSSH2_FXF_CREAT,
                                                    LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                                                    LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                                                    LIBSSH2_SFTP_S_IROTH);
    if (handle == nullptr) {
        SFTPReachbilityMananer::re_connet(SFTPReachbilityMananer::ConnectionHandleType::Write);
        handle = libssh2_sftp_open(Write_Handle, REAL_PATH(path), LIBSSH2_FXF_READ,
                                   LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                                   LIBSSH2_SFTP_S_IROTH);
    }
    if (handle != nullptr) {
        libssh2_sftp_close_handle(handle);
    } else {
        RELEASE_ALL_LOCK;
        return -EEXIST;
    }
    RELEASE_ALL_LOCK;
    return 0;
}

static int bcfs_remote_open_file(const char *path, fuse_file_info *fi) {
    if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        fi->fh = 1;
        //fi->direct_io = true;
        fi->flags = LIBSSH2_FXF_READ;
    }
    return 0;
}

static int bcfs_remote_read_file(const char *path, char *buf, size_t size, off_t offset, fuse_file_info *fi) {
    GET_ALL_LOCK;
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(Read_Handle, REAL_PATH(path), LIBSSH2_FXF_READ,
                                                    LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                                                    LIBSSH2_SFTP_S_IROTH);

    if (handle == nullptr) {
        SFTPReachbilityMananer::re_connet(SFTPReachbilityMananer::ConnectionHandleType::Read);
        handle = libssh2_sftp_open(Read_Handle, REAL_PATH(path), LIBSSH2_FXF_READ,
                                                        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                                                        LIBSSH2_SFTP_S_IROTH);
    }

    if (handle == nullptr) {
        RELEASE_ALL_LOCK;
        return -ENOENT;
    }

    int res = libssh2_sftp_read(handle, buf, size);
    libssh2_sftp_close_handle(handle);
    RELEASE_ALL_LOCK;
    return res;
}

static int bcfs_remote_write_file(const char *path, const char *buf,
                         size_t size, off_t offset, fuse_file_info *fi) {
    GET_ALL_LOCK;
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(Write_Handle, REAL_PATH(path), LIBSSH2_FXF_WRITE,
                                                    LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                                                    LIBSSH2_SFTP_S_IROTH);
    if (handle == nullptr) {
        SFTPReachbilityMananer::re_connet(SFTPReachbilityMananer::ConnectionHandleType::Write);
        handle = libssh2_sftp_open(Write_Handle, REAL_PATH(path), LIBSSH2_FXF_READ,
                                                        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                                                        LIBSSH2_SFTP_S_IROTH);
    }

    if (handle == nullptr) {
        RELEASE_ALL_LOCK;
        return -ENOENT;
    }
    LIBSSH2_SFTP_ATTRIBUTES &attribute = caches[string(REAL_PATH(path))];
    libssh2_sftp_seek64(handle, attribute.filesize);
    int res = libssh2_sftp_write(handle, buf + attribute.filesize, size - attribute.filesize);
    libssh2_sftp_close_handle(handle);
    attribute.filesize = size;
    RELEASE_ALL_LOCK;
    return 0;
}

static int bcfs_remote_release_file(const char *path, fuse_file_info *fi) {
    if (strncmp(path, info_dir, strlen(info_dir)) == 0) {
        close(dup(fi->fh));
    }
    return 0;
}


//remove a file
static int bcfs_remote_unlink_file(const char *path) {
    GET_ALL_LOCK;
    int res = libssh2_sftp_unlink(Write_Handle, REAL_PATH(path));
    if (res != 0) {
        RELEASE_ALL_LOCK;
        return -ENOENT;
    }
    caches.erase(string(REAL_PATH(path)));
    RELEASE_ALL_LOCK;
    return 0;
}


/**
 * @endcode file operation
 * */

static int bcfs_remote_statfs(const char *path, struct statvfs *stbuf)
{
    int res = statvfs(path, stbuf);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static void *update_caches(void *) {
    while (true) {
        sleep(4);
        GET_ALL_LOCK;
        if (exit_flag) {
            RELEASE_ALL_LOCK;
            break;
        }
        const char *paths[] = {"/.UserInfos/.balance_result", "/.UserInfos/.account_result", "/.UserInfos/.transaction_result", "/.UserInfos/.receive"};
        for (int i = 0; i < 4; ++i) {
            LIBSSH2_SFTP_ATTRIBUTES attribute;
            int res = libssh2_sftp_lstat(Lstat_Handle, REAL_PATH(paths[i]), &attribute);
            if (res == 0) {
                caches[REAL_PATH(paths[i])] = attribute;
            } else {
                SFTPReachbilityMananer::re_connet(SFTPReachbilityMananer::ConnectionHandleType::Lstat);
            }
        }
        RELEASE_ALL_LOCK;
    }
    return nullptr;
}

static pthread_t t1;

static void *bcfs_remote_init(struct fuse_conn_info *conn) {
    actual_files = {".transaction", ".transaction_result",
                    ".balance", ".balance_result", ".account",
                    ".account_result", ".receive",
                    ".private_keys", ".private_keys", ".coins", ".add_coin", ".add_coin_result"};

    cached_files = {"/", "/.UserInfos", "/.UserInfos/.transaction", "/.UserInfos/.transaction_result",
                    "/.UserInfos/.balance", "/.UserInfos/.balance_result", "/.UserInfos/.account",
                    "/.UserInfos/.account_result", "/.UserInfos/.receive",
                    "/.UserInfos/.private_keys", "/.UserInfos/.private_keys",
                    "/.UserInfos/.coins", "/.UserInfos/.add_coin", "/.UserInfos/.add_coin_result"};
    pthread_create(&t1, nullptr, update_caches, nullptr);
    return nullptr;
}

static void destroy(void *) {
    GET_ALL_LOCK;
    exit_flag = true;
    RELEASE_ALL_LOCK;
    //pthread_join(t1, nullptr);
}

static struct fuse_operations bcfs_remote_fs_operations = {
        .init = bcfs_remote_init,
        .getattr = bcfs_remote_get_attribute,
        /** @begin  dir */
        .opendir = bcfs_remote_open_dir,
        .readdir = bcfs_remote_read_dir, /* To provide directory listing.      */
        .releasedir = bcfs_remote_release_dir,
        //.mkdir = bcfs_remote_mkdir,
        //.rmdir = bcfs_remote_rm_dir,
        /** @end dir */

        /** @begin file */
        //.create = bcfs_remote_create_file,
        .open = bcfs_remote_open_file,
        .read = bcfs_remote_read_file,
        .write = bcfs_remote_write_file,
        //.release = bcfs_remote_release_file,
        //.unlink = bcfs_remote_unlink_file,
        /** @end file */

        .statfs = bcfs_remote_statfs,
        .destroy = destroy
};

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "parameter error, use " << argv[0] << "<mount dir> <ip address> <username> <password>" << std::endl;
        exit(1);
    }
    char *new_argv[] = {argv[0], argv[1]};//"-f", "-d",
    int res = SFTPReachbilityMananer::link(argv[2], argv[3], argv[4]);
    if (res != 0) {
        std::cerr << "remote connect fail" << std::endl;
        exit(1);
    }

    dir_prefix = "/home/" + string(argv[3]);// + "/" + string(argv[5]);
//    libssh2_sftp_mkdir(SFTPReachbilityMananer::get_instance().global_handle.sftp_session, dir_prefix.data(),
//            LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
//            LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
    return fuse_main(argc - 3, new_argv, &bcfs_remote_fs_operations, nullptr);
}