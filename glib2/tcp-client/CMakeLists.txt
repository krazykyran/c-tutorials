# GLib2 TCP client — CMake build (native or cross).
# pkg-config must resolve glib-2.0 and gio-2.0 (set PKG_CONFIG / PKG_CONFIG_PATH
# when cross-compiling, same as the Makefile).
#
# Example:
#   cmake -S . -B build && cmake --build build
# Cross (toolchain sets compiler; point pkg-config at the sysroot):
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=... -DPKG_CONFIG_EXECUTABLE=...

cmake_minimum_required(VERSION 3.16)
project(glib-tcp-client LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0 gio-2.0)

add_executable(tcp_client src/tcp_client.c)
target_link_libraries(tcp_client PRIVATE PkgConfig::GLIB2)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tcp_client PRIVATE -Wall -Wextra -O2)
endif()
