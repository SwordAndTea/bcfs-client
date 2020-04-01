#!/bin/bash

if [ "$1" == "install" ]; then
    cmake .
    make
    mkdir /usr/local/libbcfs
    cp -r include /usr/local/libbcfs/include
    cp -r lib /usr/local/libbcfs/lib
    cp bin/bcfs_local /usr/local/bin/bcfs_local
    chmod 755 /usr/local/bin/bcfs_local
    cp bin/bcfs_remote /usr/local/bin/bcfs_remote
    chmod 755 /usr/local/bin/bcfs_remote
    cp bcfs_client.sh /usr/local/bin/bcfs-client
    chmod 755 /usr/local/bin/bcfs-client
elif [ "$1" == "uninstall" ]; then
    rm -r /usr/local/libbcfs
    rm /usr/local/bin/bcfs_local
    rm /usr/local/bin/bcfs_remote
    rm /usr/local/bin/bcfs-client
    echo "done"
else
    echo "wrong parameter, use <install | uninstall>"
fi

