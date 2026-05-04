cmake_minimum_required(VERSION 3.13)
project(tcp-chat-server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(tcp-chat-server src/tcp-chat-server.c)
add_executable(tsc-sync-encode src/scats_pdu.c)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(LIBUBOX libubox)
endif()

if(LIBUBOX_FOUND)
	target_include_directories(tcp-chat-server PRIVATE ${LIBUBOX_INCLUDE_DIRS})
	target_link_libraries(tcp-chat-server PRIVATE ${LIBUBOX_LIBRARIES})
	target_compile_options(tcp-chat-server PRIVATE ${LIBUBOX_CFLAGS_OTHER})
	target_include_directories(tsc-sync-encode PRIVATE ${LIBUBOX_INCLUDE_DIRS})
	target_link_libraries(tsc-sync-encode PRIVATE ${LIBUBOX_LIBRARIES})
	target_compile_options(tsc-sync-encode PRIVATE ${LIBUBOX_CFLAGS_OTHER})
else()
	find_path(LIBUBOX_INCLUDE_DIR NAMES libubox/uloop.h PATH_SUFFIXES include)
	find_library(LIBUBOX_LIBRARY NAMES ubox)
	if(NOT LIBUBOX_INCLUDE_DIR OR NOT LIBUBOX_LIBRARY)
		message(
			FATAL_ERROR
			"libubox not found. Install libubox (OpenWrt staging) or libubox-dev / pkg-config on host."
		)
	endif()
	target_include_directories(tcp-chat-server PRIVATE ${LIBUBOX_INCLUDE_DIR})
	target_link_libraries(tcp-chat-server PRIVATE ${LIBUBOX_LIBRARY})
	target_include_directories(tsc-sync-encode PRIVATE ${LIBUBOX_INCLUDE_DIR})
	target_link_libraries(tsc-sync-encode PRIVATE ${LIBUBOX_LIBRARY})
endif()

install(TARGETS tcp-chat-server RUNTIME DESTINATION bin)
install(TARGETS tsc-sync-encode RUNTIME DESTINATION bin)
