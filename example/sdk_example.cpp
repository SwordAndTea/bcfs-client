//
// Created by 向尉 on 2020/3/17.
//
#include "../include/bcfs_client.h"
#include <iostream>

using namespace BCFS;

int main() {
    BcfsClient client(ServerType::Remote, "xxx.xxx.xxx.xxx", "username", "password");
    if (client.start("mount_dir") == StartStatus::Success) {
        client.register_receive_action([](const string &from, const string &to, const string &coin_address,
                                          const string &coin_name, double count,
                                          const string &transaction_hash, const string &date) {
            std::cout << "transaction from address " << from << " to address " << to  << " on coin " << coin_name << " withe value "
                      << count << " on date " + date << " transaction hash is " << transaction_hash << std::endl;
        });
        std::cout << "start finished" << std::endl;
        int a;
        while (std::cin >> a) {
            if (a == 0) {
                auto result = client.shut_down();
                std::cout << result << std::endl;
                if (result == ShutDownStatus::Success || result == ShutDownStatus::Umounted) {
                    break;
                }

            } else if (a == 1) {
                //get balance
                client.get_balance("0x9e7D97F07097E9B3E21459776EEab507213DF52F",
                                   "0x3C1670cb9c9D27CeDf2119110165663efc77a22f",
                                   [](double balance) {
                                       std::cout << "balance is " << balance << std::endl;
                                   },
                                   [](const string &error) {
                                       std::cout << "error is " << error << std::endl;
                                   });
            } else if (a == 2) {
                //send transaction
                client.set_transaction("0x9e7D97F07097E9B3E21459776EEab507213DF52F",
                                       "0x297DED25327B5b9Ad04C4F3e72B2eb3Ff2920cA5",
                                       "0x3C1670cb9c9D27CeDf2119110165663efc77a22f", 5000,
                                       [](double gasUsed, const string &transaction_hash) {
                                           std::cout << "success and the transaction hash is " << transaction_hash
                                                     << " gas used is " << gasUsed << std::endl;
                                       }, [](const string &error) {
                            std::cout << error << std::endl;
                        });
            } else if (a == 3) {
                //add account
                string name;
                string private_key;
                std::cin >> name >> private_key;
                client.add_account(name, private_key, [](const string &address) {
                    std::cout << "success and address is " << address << std::endl;
                }, [](const string &error) {
                    std::cout << error << std::endl;
                });
            } else if (a == 4) {
                //create account
                client.create_account("account_4", [](const string &address, const string &private_key) {
                    std::cout << "success and the address is " << address << " private_key is " << private_key
                              << std::endl;
                }, [](const string &error) {
                    std::cout << error << std::endl;
                });
            } else if (a == 5) {
                client.get_all_balance([](vector<BalanceInfo> infos) {
                    for (auto &info : infos) {
                        std::cout << info.address << ":" << info.coin_name << ":" << info.balance << std::endl;
                    }
                }, [](const string &error) {
                    std::cout << "error is " << error << std::endl;
                });
            } else if (a == 6) {
                client.get_all_balance_in_single_account("0x9e7D97F07097E9B3E21459776EEab507213DF52F",
                                                         [](vector<BalanceInfo> infos) {
                                                             for (auto &info : infos) {
                                                                 std::cout << info.address << ":" << info.coin_name
                                                                           << ":" << info.balance << std::endl;
                                                             }
                                                         }, [](const string &error) {
                            std::cout << "error is " << error << std::endl;
                        });
            } else if (a == 7) {
                client.set_eth_transaction("0x9e7D97F07097E9B3E21459776EEab507213DF52F",
                                           "0x297DED25327B5b9Ad04C4F3e72B2eb3Ff2920cA5", 0.1,
                                           [](double gasUsed, const string &transaction_hash) {
                                               std::cout << "success and the transaction hash is " << transaction_hash
                                                         << " gas used is " << gasUsed << std::endl;
                                           }, [](const string &error) {
                            std::cout << error << std::endl;
                        });
            }
        }
    }
    return 0;
}
