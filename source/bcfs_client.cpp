//
// Created by 向尉 on 2020/3/17.
//

#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <pthread.h>
#include <sstream>
#include <fstream>
#include <regex>
#include <sys/file.h>
#include "../include/bcfs_client.h"

static const string base_dir("/.UserInfos");

static const string balance_file(base_dir + "/.balance");

static const string balance_result_file(base_dir + "/.balance_result");

static const string transaction_file(base_dir + "/.transaction");

static const string transaction_result_file(base_dir + "/.transaction_result");

static const string account_file(base_dir + "/.account");

static const string account_result_file(base_dir + "/.account_result");

static const string receive_file(base_dir + "/.receive");

static const string private_keys_file(base_dir + "/.private_keys");

static const string add_coin_file(base_dir + "/.add_coin");

static const string add_coin_result(base_dir + "/.add_coin_result");

static const string coins_file(base_dir + "/.coins");


using namespace BCFS;

using std::vector;

//static unordered_map<string, string> private_key_cache;

#define CHECK_RE_MOUNT do { \
    struct statvfs vfs; \
    int res = statvfs(mount_dir.data(), &vfs); \
    if (res < 0 || vfs.f_files == 0) { \
        re_start(); \
    } \
} while (0)

struct MonitorInfo {
    string path;
    std::function<void(const string &data)> action;
    bool should_continue;
    bool increment;
    int time_out;
};

static pthread_mutex_t mutex;

static bool exit_flag = false;

void *monitor_file_thread(void *arg) {
    MonitorInfo *info = reinterpret_cast<MonitorInfo *>(arg);
    struct stat file_state;
    lstat(info->path.data(), &file_state);
    long initial_modify_time = file_state.st_mtimespec.tv_sec;
    int initial_size = file_state.st_size;
    int current_time = 0;
    int time_interval = 2;
    while (true) {
        pthread_mutex_lock(&mutex);
        if (exit_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        lstat(info->path.data(), &file_state);
        if (file_state.st_mtimespec.tv_sec != initial_modify_time || file_state.st_size != initial_size) {
            FILE *file = fopen(info->path.data(), "r");
            if (info->increment) {
                if (file_state.st_size > initial_size) {
                    string data(file_state.st_size - initial_size, '0');
                    fseek(file, initial_size, SEEK_SET);
                    fread(data.data(), 1, file_state.st_size - initial_size, file);
                    info->action(data);
                    initial_size = file_state.st_size;
                }
            } else {
                string data(file_state.st_size, '0');
                fread(data.data(), 1, file_state.st_size, file);
                info->action(data);
            }
            fclose(file);
            if (!info->should_continue) {
                pthread_mutex_unlock(&mutex);
                break;
            } else {
                initial_modify_time = file_state.st_mtimespec.tv_sec;
            }
        }
        if (info->time_out >= 0) {
            current_time += time_interval;
            if (current_time > info->time_out) {
                pthread_mutex_unlock(&mutex);
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(time_interval);
    }
    delete info;
    return nullptr;
}

std::vector<string> split_string(const string &str, char c) {
    int begin = 0;
    int next;
    std::vector<string> result;
    while ((next = str.find(c, begin)) != string::npos) {
        result.push_back(str.substr(begin, next - begin));
        begin = next + 1;
    }
    if (begin != str.size()) {
        result.push_back(str.substr(begin));
    }
    return result;
}

pthread_t
start_monitor_file(const string &file_path, const std::function<void(const string &)> &action, bool should_continue,
                   bool increment = false, int time_out = -1) {
    pthread_t handle;
    MonitorInfo *info = new MonitorInfo{file_path, action, should_continue, increment, time_out};
    pthread_create(&handle, nullptr, monitor_file_thread, info);
    return handle;
}

BcfsClient::BcfsClient(ServerType type, const string &ip, const string &username, const string &password) :
        server_info(type, ip, username, password) {

}

BcfsClient::BcfsClient(const ServerInfo &server_info) :
        server_info(server_info) {

}

StartStatus BcfsClient::start(const string &directory) {
    if (has_start) {
        return StartStatus::AlreadyStart;
    }
    pthread_mutex_lock(&mutex);
    exit_flag = false;
    pthread_mutex_unlock(&mutex);
    mount_dir = directory;
    int child_return;
    string file_name = ".tmp_result_file";
    switch (server_info.type) {
        case ServerType::Remote :
            if (fork() == 0) {
                //chilid process
                int fd = open(file_name.data(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
                dup2(fd, STDERR_FILENO);
                execlp("bcfs_remote", "bcfs_remote", directory.data(), server_info.ip.data(),
                       server_info.username.data(), server_info.password.data(), NULL);
                close(fd);
                exit(0);
            } else {
                //parent process
                wait(&child_return);
                FILE *file = fopen(file_name.data(), "r");
                char a[64];
                memset(a, 0, 64 * sizeof(char));
                fread(a, 1, 64, file);
                fclose(file);
                unlink(file_name.data());
                if (strcmp(a, "") == 0) {
                    has_start = true;
                    return StartStatus::Success;
                } else {
                    return StartStatus::StartError;
                }
            }
            break;
        case ServerType::LocalHost :
            if (fork() == 0) {
                //chilid process
                int fd = open(file_name.data(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
                dup2(fd, STDERR_FILENO);
                execlp("bcfs_local", "bcfs_local", directory.data(), NULL);
                close(fd);
            } else {
                //parent process
                wait(&child_return);
                FILE *file = fopen(file_name.data(), "r");
                char a[64];
                memset(a, 0, 64 * sizeof(char));
                fread(a, 1, 64, file);
                fclose(file);
                unlink(file_name.data());
                if (strcmp(a, "") == 0) {
                    has_start = true;
                    return StartStatus::Success;
                } else {
                    return StartStatus::StartError;
                }
            }
            break;
        default:
            break;
    }

    return StartStatus::StartError;
}

StartStatus BcfsClient::re_start(const ServerInfo &info) {
    shut_down();
    server_info = info;
    return start(mount_dir);
}

StartStatus BcfsClient::re_start() {
    shut_down();
    return start(mount_dir);
}

ShutDownStatus BcfsClient::shut_down() {
    pthread_mutex_lock(&mutex);
    exit_flag = true;
    string file_name = ".tmp_result_file";
    if (fork() == 0) {
        int fd = open(file_name.data(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
        dup2(fd, STDERR_FILENO);
        execlp("umount", "umount", mount_dir.data(), nullptr);
        close(fd);
        exit(0);
    } else {
        int child_return;
        wait(&child_return);
        FILE *file = fopen(file_name.data(), "r");
        char a[64];
        memset(a, 0, 64 * sizeof(char));
        fread(a, 1, 100, file);
        if (strstr(a, "Resource busy") != nullptr) {
            return ShutDownStatus::Busy;
        } else if (strstr(a, "not currently mounted") != nullptr) {
            return ShutDownStatus::Umounted;
        }
        fclose(file);
        unlink(file_name.data());
    }
    pthread_mutex_unlock(&mutex);
    return ShutDownStatus::Success;
}

void BcfsClient::get_balance(const string &from, const string &coin_address, const std::function<void(double)> &success,
                             const std::function<void(const string &)> &failure) {
    //CHECK_RE_MOUNT;
    std::fstream file(mount_dir + balance_file, std::ios::app);
    string data_to_write = from + ":" + coin_address;
    file << data_to_write << std::endl;
    file.close();
    start_monitor_file(mount_dir + balance_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            success(atof(the_data.data()));
        }
    }, false);
}

void BcfsClient::get_all_balance(const std::function<void(vector<BalanceInfo>)> &success,
                                 const std::function<void(const string &)> &failure) {
    struct stat file_state;
    lstat((mount_dir + balance_result_file).data(), &file_state);
    std::fstream file(mount_dir + balance_file, std::ios::app);
    file << "ALL" << std::endl;
    file.close();
    start_monitor_file(mount_dir + balance_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            vector<string> infos = split_string(the_data, '\n');
            vector<BalanceInfo> balances_infos;
            for (auto &info : infos) {
                vector<string> detail_infos = split_string(info, ':');
                balances_infos.push_back({detail_infos[0], detail_infos[1], atof(detail_infos[2].data())});
            }
            success(balances_infos);
        }
    }, false);
}

void BcfsClient::get_all_balance_in_single_account(const string &address,
                                                   const std::function<void(vector<BalanceInfo>)> &success,
                                                   const std::function<void(const string &error)> &failure) {
    std::fstream file(mount_dir + balance_file, std::ios::app);
    file << "ALL:" << address << std::endl;
    file.close();
    start_monitor_file(mount_dir + balance_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            vector<string> infos = split_string(the_data, '\n');
            vector<BalanceInfo> balances_infos;
            for (auto &info : infos) {
                vector<string> detail_infos = split_string(info, ':');
                balances_infos.push_back({detail_infos[0], detail_infos[1], atof(detail_infos[2].data())});
            }
            success(balances_infos);
        }
    }, false);
}

void BcfsClient::set_transaction(const string &from, const string &to, const string &coin_address, double count,
                                 const std::function<void(double, const string &)> &success,
                                 const std::function<void(const string &)> &failure) {
    //CHECK_RE_MOUNT;
    std::fstream file(mount_dir + transaction_file, std::ios::app);
    std::stringstream ss;
    ss << count;
    string data_to_write = from + ":" + to + ":" + coin_address + ":" + ss.str();
    file << data_to_write << std::endl;
    file.close();
    start_monitor_file(mount_dir + transaction_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            vector<string> detailDatas = split_string(the_data, ':');
            success(atof(detailDatas[0].data()), detailDatas[1]);
        }
    }, false);
}

void BcfsClient::set_eth_transaction(const string &from, const string &to, double count,
                                     const std::function<void(double, const string &)> &success,
                                     const std::function<void(const string &)> &failure) {
    std::fstream file(mount_dir + transaction_file, std::ios::app);
    std::stringstream ss;
    ss << count;
    string data_to_write = from + ":" + to + ":" + "ETH" + ":" + ss.str();
    file << data_to_write << std::endl;
    file.close();
    start_monitor_file(mount_dir + transaction_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            vector<string> detailDatas = split_string(the_data, ':');
            success(atof(detailDatas[0].data()), detailDatas[1]);
        }
    }, false);
}

void BcfsClient::create_account(const string &name, const std::function<void(const string &, const string &)> &success,
                                const std::function<void(const string &)> &failure) {
    //CHECK_RE_MOUNT;
    std::fstream file(mount_dir + account_file, std::ios::app);
    file << string("create") + ":" + name << std::endl;
    file.close();
    start_monitor_file(mount_dir + account_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            int index = the_data.find(':');
            success(the_data.substr(0, index), the_data.substr(index + 1));
        }
    }, false);
}

void BcfsClient::add_account(const string &name, const string &private_key,
                             const std::function<void(const string &address)> &success,
                             const std::function<void(const string &error)> &failure) {
    if (!std::regex_search(private_key, std::regex("^0x")) &&
        std::regex_search(private_key.substr(2), std::regex("[^0-9a-fA-F]"))) {
        failure("invalid private key pattern, user start with 0x, and should all be hex value");
        return;
    }
    //CHECK_RE_MOUNT;
    std::fstream file(mount_dir + account_file, std::ios::app);
    file << string("add") + ":" + name + ":" + private_key << std::endl;
    file.close();
    start_monitor_file(mount_dir + account_result_file, [success, failure](const string &the_data) {
        if (the_data.find("error") != string::npos) {
            failure(the_data);
        } else {
            success(the_data);
        }
    }, false);
}

void BcfsClient::add_accounts(const vector<AccountInfo> &accounts) {
    //TODO: finish this
}

vector<AccountInfo> BcfsClient::get_accounts() {
    std::fstream file(mount_dir + private_keys_file, std::ios::in);
    file.seekg(0, std::ios::end);
    int length = file.tellg();
    file.seekg(0, std::ios::beg);
    string data(length, ' ');
    file.read(data.data(), length);
    file.close();
    vector<AccountInfo> result;
    vector<string> accounts = split_string(data, '\n');
    for (auto &account : accounts) {
        if (account != "") {
            vector<string> details = split_string(account, ':');
            if (details.size() == 3) {
                result.push_back({details[0], details[1], details[2]});
            }
        }
    }
    return result;
}


unordered_map<string, string> BcfsClient::get_coins() {
    return unordered_map<string, string>();
}

void BcfsClient::add_coin(const string &coin_address, const std::function<void()> &success,
                          const std::function<void(const string &error)> &failure) {
    std::fstream file(mount_dir + add_coin_file, std::ios::app);
    file << coin_address << std::endl;
    file.close();
    start_monitor_file(mount_dir + add_coin_file, [success, failure](const string &the_data) {

    }, false);
}

void BcfsClient::register_receive_action(
        const std::function<void(const string &from, const string &to, const string &coin_address,
                                 const string &coin_name, double count,
                                 const string &transaction_hash, const string &date)> &action) {
    start_monitor_file(mount_dir + receive_file, [action](const string &new_data) {
        std::vector<string> transactions = split_string(new_data, '\n');
        for (const auto &transaction: transactions) {
            std::vector<string> transaction_info = split_string(transaction, ':');
            string date = transaction_info[6] + ":" + transaction_info[7] + ":" + transaction_info[8];
            action(transaction_info[0], transaction_info[1], transaction_info[2],
                   transaction_info[3], atof(transaction_info[4].data()), transaction_info[5], date);
        }
    }, true, true);
}

BcfsClient::~BcfsClient() {}

namespace BCFS {
    std::ostream &operator<<(std::ostream &os, StartStatus start_status) {
        {
            switch (start_status) {
                case StartStatus::Success :
                    os << "Success";
                    break;
                case StartStatus::AlreadyStart :
                    os << "Already start";
                    break;
                case StartStatus::StartError :
                    os << "Start error ";
                    break;
            }
            return os;
        }
    }

    std::ostream &operator<<(std::ostream &os, ShutDownStatus shut_down_status) {
        {
            switch (shut_down_status) {
                case ShutDownStatus::Success :
                    os << "Success shut down";
                    break;
                case ShutDownStatus::Umounted :
                    os << "Current directory is not mounted";
                    break;
                case ShutDownStatus::Busy :
                    os << "Current directory is still under operation, Please try later ";
                    break;
            }
            return os;
        }
    }
}






