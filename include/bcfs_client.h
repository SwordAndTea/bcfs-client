//
// Created by 向尉 on 2020/3/17.
//

#ifndef FUSETEST_BCFS_CLIENT_H
#define FUSETEST_BCFS_CLIENT_H

#include <string>
#include <ostream>
#include <unordered_map>
#include <vector>

using std::string;

using std::vector;

using std::unordered_map;

using std::ostream;

using std::pair;

namespace BCFS {

    enum class ServerType {
        Remote,
        LocalHost,
    };

    /**
     * information to identify a server
     * @property ip the IPv4 address of your server
     * @property username username to login your server
     * @property password password to login your server
     * */
    struct ServerInfo {
        ServerType type;
        string ip;
        string username;
        string password;

        ServerInfo() = default;

        ServerInfo(ServerType type, const string &ip, const string &username, const string &password) :
                type(type), ip(ip), username(username), password(password) {}
    };


    struct BalanceInfo {
        string address;
        string coin_name;
        double balance;
    };

    struct AccountInfo {
        string name;
        string address;
        string private_key;
    };

    enum class StartStatus {
        Success,
        AlreadyStart,
        StartError,
    };

    enum class ShutDownStatus {
        Success,
        Umounted,
        Busy,
    };

    class BcfsClient {
    private:
        ServerInfo server_info;

        string mount_dir;

        bool has_start = false;

    public:
        /**
         * if the type you give LocalHost, then the server_info will be ignored
         * */
        BcfsClient(ServerType type, const string &ip, const string &username, const string &password);

        BcfsClient(const ServerInfo &server_info);

        /**
         * start mount the file system in the given directory
         * */
        StartStatus start(const string &directory);


        /**
         * remount the bcfs using the new info
         * */
        StartStatus re_start(const ServerInfo &info);

        /**
         * remount the bcfs with previous server info
         * */
        StartStatus re_start();

        /**
         * end up running the file system
         * */
        ShutDownStatus shut_down();


        /**
         * trigger a new transaction action
         * if success, return nothing (use the success callback to deal with it)
         * if fail, return error information (use the failure callback to deal with it)
         *
         * @param from : the address start the transaction
         * @param to : the address receive the transaction
         * @param coin_address : the coin kind
         * @param count : coin number to send
         * */
        void set_transaction(const string &from, const string &to, const string &coin_address, double count,
                             const std::function<void(double gasUsed, const string &transaction_hash)> &success,
                             const std::function<void(const string &error)> &failure);

        void set_eth_transaction(const string &from, const string &to, double count,
                                 const std::function<void(double gasUsed, const string &transaction_hash)> &success,
                                 const std::function<void(const string &error)> &failure);

        /**
         * create a new account
         * if success, return back the address of the account (use the success callback to deal with it)
         * if fail, return error information (use the failure callback to deal with it)
         *
         * @param name : account alias
         * @param success
         * @param failure
         * */
        void create_account(const string &name,
                            const std::function<void(const string &address, const string &private_key)> &success,
                            const std::function<void(const string &error)> &failure);


        /**
         * add an exit count with private key
         * @param private_key
         * @param success
         * @param failure
         * */
        void add_account(const string &name, const string &private_key,
                         const std::function<void(const string &address)> &success,
                         const std::function<void(const string &error)> &failure);

        /**
         * @deprecated
         * */
        void add_accounts(const vector<AccountInfo> &accounts);

        /**
         * get all account
         * */
        vector<AccountInfo> get_accounts();

        /**
         * get balance
         * @param from
         * @param coin_address
         * @param success
         * @param failure
         * */
        void
        get_balance(const string &from, const string &coin_address, const std::function<void(double balance)> &success,
                    const std::function<void(const string &error)> &failure);


        void get_all_balance(const std::function<void(vector<BalanceInfo>)> &success,
                             const std::function<void(const string &error)> &failure);

        void get_all_balance_in_single_account(const string &address,
                                               const std::function<void(vector<BalanceInfo>)> &success,
                                               const std::function<void(const string &error)> &failure);

        /**
         * @deprecated
         * */
        unordered_map<string, string> get_coins();

        /**
         * add a new type of coin
         * @deprecated
         * */
        void add_coin(const string &coin_address, const std::function<void()> &success,
                      const std::function<void(const string &error)> &failure);

        /**
         *
         * */
        void register_receive_action(
                const std::function<void(const string &from, const string &to, const string &coin_address, const string &coin_name, double count,
                                         const string &transaction_hash, const string &date)> &action);

        virtual ~BcfsClient();

    };

    ostream &operator<<(std::ostream &os, StartStatus start_status);

    ostream &operator<<(std::ostream &os, ShutDownStatus shut_down_status);
}


#endif //FUSETEST_BCFS_CLIENT_H
