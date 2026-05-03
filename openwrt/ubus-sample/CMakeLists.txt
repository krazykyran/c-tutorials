cmake_minimum_required(VERSION 3.13)
project(ubus-sample C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ubus-sample src/ubus-sample.c)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(LIBUBOX libubox)
	pkg_check_modules(LIBUBUS libubus)
endif()

if(LIBUBOX_FOUND)
	target_include_directories(ubus-sample PRIVATE ${LIBUBOX_INCLUDE_DIRS})
	target_link_libraries(ubus-sample PRIVATE ${LIBUBOX_LIBRARIES})
	target_compile_options(ubus-sample PRIVATE ${LIBUBOX_CFLAGS_OTHER})
else()
	find_path(LIBUBOX_INCLUDE_DIR NAMES libubox/uloop.h PATH_SUFFIXES include)
	find_library(LIBUBOX_LIBRARY NAMES ubox)
	if(NOT LIBUBOX_INCLUDE_DIR OR NOT LIBUBOX_LIBRARY)
		message(
			FATAL_ERROR
			"libubox not found. Install libubox (OpenWrt staging) or libubox-dev / pkg-config on host."
		)
	endif()
	target_include_directories(ubus-sample PRIVATE ${LIBUBOX_INCLUDE_DIR})
	target_link_libraries(ubus-sample PRIVATE ${LIBUBOX_LIBRARY})
endif()

if(LIBUBUS_FOUND)
	target_include_directories(ubus-sample PRIVATE ${LIBUBUS_INCLUDE_DIRS})
	target_link_libraries(ubus-sample PRIVATE ${LIBUBUS_LIBRARIES})
	target_compile_options(ubus-sample PRIVATE ${LIBUBUS_CFLAGS_OTHER})
else()
	find_path(LIBUBUS_INCLUDE_DIR NAMES libubus.h PATH_SUFFIXES include)
	find_library(LIBUBUS_LIBRARY NAMES ubus)
	if(NOT LIBUBUS_INCLUDE_DIR OR NOT LIBUBUS_LIBRARY)
		message(
			FATAL_ERROR
			"libubus not found. Install libubus (OpenWrt staging) or libubus-dev / pkg-config on host."
		)
	endif()
	target_include_directories(ubus-sample PRIVATE ${LIBUBUS_INCLUDE_DIR})
	target_link_libraries(ubus-sample PRIVATE ${LIBUBUS_LIBRARY})
endif()

# blobmsg_json uses json-c; pkg-config usually pulls it in — needed for manual find_library fallback
if(NOT LIBUBOX_FOUND OR NOT LIBUBUS_FOUND)
	find_library(JSON_C_LIBRARY NAMES json-c)
	if(JSON_C_LIBRARY)
		target_link_libraries(ubus-sample PRIVATE ${JSON_C_LIBRARY})
	endif()
endif()

install(TARGETS ubus-sample RUNTIME DESTINATION bin)
