cmake_minimum_required(VERSION 3.14)
project(FuseTest)

set(CMAKE_CXX_STANDARD 20)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(FUSE_INCLUDE_DIR /usr/local/include/osxfuse/fuse)
set(FUSE_LINK_DIR /usr/local/lib)

include_directories(${FUSE_INCLUDE_DIR})
link_directories(${FUSE_LINK_DIR})
link_libraries(-losxfuse)
link_libraries(-lpthread)

include_directories(/usr/local/Cellar/libssh2/1.9.0_1/include)
link_directories(/usr/local/Cellar/libssh2/1.9.0_1/lib)
link_libraries(libssh2.a)


add_compile_options(-D_FILE_OFFSET_BITS=64)
add_compile_options(-DFUSE_USE_VERSION=26)

add_executable(bcfs_remote source/bcfs_remote.cpp source/sftp_reachbility_manager.cpp source/sftp_reachbility_manager.h)
add_executable(bcfs_local source/bcfs_local.cpp)

add_library(bcfs SHARED include/bcfs_client.h source/bcfs_client.cpp)