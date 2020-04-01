//
// Created by 向尉 on 2020/3/13.
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "sftp_reachbility_manager.h"


using namespace BCFS;

SFTPReachbilityMananer::ConnectHandle SFTPReachbilityMananer::lstat_handle = ConnectHandle{};

SFTPReachbilityMananer::ConnectHandle SFTPReachbilityMananer::read_handle = ConnectHandle{};

SFTPReachbilityMananer::ConnectHandle SFTPReachbilityMananer::write_handle = ConnectHandle{};

SFTPReachbilityMananer::ServerInfo SFTPReachbilityMananer::server_info = ServerInfo{};


LIBSSH2_SFTP *SFTPReachbilityMananer::get_lstat_sftp_session() {
    return lstat_handle.sftp_session;
}

LIBSSH2_SFTP *SFTPReachbilityMananer::get_read_sftp_session() {
    return read_handle.sftp_session;
}

LIBSSH2_SFTP *SFTPReachbilityMananer::get_write_sftp_session() {
    return write_handle.sftp_session;
}


void SFTPReachbilityMananer::partial_shut_down(LIBSSH2_SESSION *libssh_session, int socked_id) {
    libssh2_session_disconnect(libssh_session, "Normal Shutdown");
    libssh2_session_free(libssh_session);
    close(socked_id);
    libssh2_exit();
}

int SFTPReachbilityMananer::initialize(SFTPReachbilityMananer::ServerInfo &info,
                                       SFTPReachbilityMananer::ConnectHandle &handle) {
    int rc = libssh2_init(0);
    if (rc != 0) {
        std::cerr << "libssh2 initialization failed " << rc << std::endl;
    }

    /* create socket instance */
    handle.socked_id = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    sockaddr_in server_addr;//IPv4地址
    server_addr.sin_family = AF_INET;//使用IPv4
    server_addr.sin_port = htons(22);
    server_addr.sin_addr.s_addr = inet_addr(info.ip.data());

    if (connect(handle.socked_id, (sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "failed to connect" << std::endl;
        return -1;
    }

    /* create sesssion instance */
    handle.session = libssh2_session_init();
    if (handle.session == nullptr) {
        std::cerr << "libssh2 session create fail" << std::endl;
        return -1;
    }
    rc = libssh2_session_handshake(handle.session, handle.socked_id);
    if (rc != 0) {
        std::cerr << "Failure establishing SSH session: " << rc << std::endl;
        return -1;
    }

    /* check finger print */
    //const char *fingerprint = libssh2_hostkey_hash(global_handle.session, LIBSSH2_HOSTKEY_HASH_SHA1);

    if (libssh2_userauth_password(handle.session, info.username.data(),
            info.password.data())) {
        std::cerr << "authentication by password failed" << std::endl;
        partial_shut_down(handle.session, handle.socked_id);
        return -1;
    }

    handle.sftp_session = libssh2_sftp_init(handle.session);

    if (handle.sftp_session == nullptr) {
        std::cerr << "unable to init sftp session" << std::endl;
        partial_shut_down(handle.session, handle.socked_id);
        return -1;
    }

    libssh2_session_set_blocking(handle.session, 1);

    return 0;
}

int SFTPReachbilityMananer::link(const string &ip_address, const string &username, const string &password) {
    server_info = ServerInfo{ip_address, username, password};
    int res = initialize(server_info, lstat_handle);
    if (res != 0) {
        return res;
    }
    res = initialize(server_info, read_handle);
    if (res != 0) {
        return res;
    }
    return initialize(server_info, write_handle);
}



void SFTPReachbilityMananer::re_connet(SFTPReachbilityMananer::ConnectionHandleType type) {
    //shut_down();
    switch (type) {
        case ConnectionHandleType::Lstat :
            initialize(server_info, lstat_handle);
            break;
        case ConnectionHandleType::Read :
            initialize(server_info, read_handle);
        case ConnectionHandleType::Write :
            initialize(server_info, write_handle);
    }
    link(server_info.ip, server_info.username, server_info.password);
}

void SFTPReachbilityMananer::shut_down() {
    libssh2_sftp_shutdown(lstat_handle.sftp_session);
    partial_shut_down(lstat_handle.session, lstat_handle.socked_id);
}









