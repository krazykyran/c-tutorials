# OpenWrt package Makefile — copy under package/, e.g. package/utils/tcp-chat-server
# SDK: make package/tcp-chat-server/compile V=s

include $(TOPDIR)/rules.mk

PKG_NAME := tcp-chat-server
PKG_VERSION := 1.0
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

CMAKE_INSTALL := 1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/tcp-chat-server
	SECTION := net
	CATEGORY := Network
	TITLE := libubox TCP chat server (uloop/usock)
	DEPENDS := +libubox
endef

define Package/tcp-chat-server/description
	Listens on TCP port 5022, greets with ping!, echoes CR-terminated lines with CRLF.
	SIGHUP sends ending session to clients and keeps listening; SIGINT/SIGTERM shut down and exit.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) ./src $(PKG_BUILD_DIR)/
endef

define Package/tcp-chat-server/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/tcp-chat-server $(1)/usr/bin/
endef

$(eval $(call BuildPackage,tcp-chat-server))
