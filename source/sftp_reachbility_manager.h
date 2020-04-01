//
// Created by 向尉 on 2020/3/13.
//

#ifndef FUSETEST_SFTP_REACHBILITY_MANAGER_H
#define FUSETEST_SFTP_REACHBILITY_MANAGER_H

#include <pthread.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <string>

using std::string;

namespace BCFS {
    class SFTPReachbilityMananer {
    public:
        enum class ConnectionHandleType {
            Lstat,
            Read,
            Write,
        };

    private:
        SFTPReachbilityMananer() = default;

        static void partial_shut_down(LIBSSH2_SESSION *libssh_session, int socked_id);

        struct ServerInfo {
            string ip;
            string username;
            string password;
        };

        struct ConnectHandle {
            int socked_id;
            LIBSSH2_SESSION *session;
            LIBSSH2_SFTP *sftp_session;

            ConnectHandle() : socked_id(-1), session(nullptr), sftp_session(nullptr) {}
        };



        static ServerInfo server_info;

        static ConnectHandle lstat_handle;

        static ConnectHandle read_handle;

        static ConnectHandle write_handle;

        static int initialize(ServerInfo &info, ConnectHandle &handle);

    public:

        static LIBSSH2_SFTP *get_lstat_sftp_session();

        static LIBSSH2_SFTP *get_read_sftp_session();

        static LIBSSH2_SFTP *get_write_sftp_session();

        static int link(const string &ip_address, const string &username, const string &password);

        static void re_connet(ConnectionHandleType type);

        static void shut_down();

    };
}







#endif //FUSETEST_SFTP_REACHBILITY_MANAGER_H
