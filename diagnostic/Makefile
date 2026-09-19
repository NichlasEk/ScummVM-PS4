TITLE      := ScummVM PS4 Diagnostic
VERSION    := 1.00
TITLE_ID   := SCVM00001
CONTENT_ID := IV0000-SCVM00001_00-SCUMMVMDIAG00001

TOOLCHAIN ?= $(OO_PS4_TOOLCHAIN)
ifeq ($(strip $(TOOLCHAIN)),)
TOOLCHAIN := /opt/openorbis/OpenOrbis/PS4Toolchain
endif

HOST_OS := $(shell uname -s)
ifeq ($(HOST_OS),Linux)
HOST_BIN := linux
else ifeq ($(HOST_OS),Darwin)
HOST_BIN := macos
else
$(error Unsupported build host: $(HOST_OS))
endif

BUILD_DIR := build-openorbis
OBJ_DIR   := $(BUILD_DIR)/obj
STAGE_DIR := $(BUILD_DIR)/pkgroot
DIST_DIR  := dist-openorbis
TARGET    := $(BUILD_DIR)/scummvm-ps4-diagnostic
PKG       := $(DIST_DIR)/$(CONTENT_ID).pkg

CXX        := $(shell command -v clang++)
LD         := $(shell command -v ld.lld)
PKG_TOOL   := $(TOOLCHAIN)/bin/$(HOST_BIN)/PkgTool.Core
CREATE_GP4 := $(TOOLCHAIN)/bin/$(HOST_BIN)/create-gp4
CREATE_SELF := $(TOOLCHAIN)/bin/$(HOST_BIN)/create-fself

CXXFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 \
	-D_LIBCPP_HAS_NO_LOCALE -fno-exceptions -fno-rtti \
	-isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
	-isystem $(TOOLCHAIN)/include/c++/v1 \
	-Icompat/include -I$(TOOLCHAIN)/usr/include -I$(TOOLCHAIN)/usr/include/SDL2

LIBS := -lc -lkernel -lSDL2 -lSceUserService -lSceVideoOut \
	-lSceAudioOut -lScePad -lSceSysmodule

LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
	-L$(TOOLCHAIN)/lib -L$(TOOLCHAIN)/usr/lib \
	$(TOOLCHAIN)/lib/crt1.o $(LIBS)

.PHONY: all clean verify check-toolchain

export OO_PS4_TOOLCHAIN := $(TOOLCHAIN)

all: $(PKG) verify

check-toolchain:
	@test -x "$(CXX)" || { echo "Missing compiler: $(CXX)"; exit 1; }
	@test -x "$(LD)" || { echo "Missing linker: $(LD)"; exit 1; }
	@test -x "$(CREATE_SELF)" || { echo "Missing create-fself: $(CREATE_SELF)"; exit 1; }
	@test -x "$(CREATE_GP4)" || { echo "Missing create-gp4: $(CREATE_GP4)"; exit 1; }
	@test -x "$(PKG_TOOL)" || { echo "Missing PkgTool.Core: $(PKG_TOOL)"; exit 1; }
	@test -f "$(TOOLCHAIN)/lib/libSDL2.a" || { echo "Missing SDL2 port library"; exit 1; }

$(OBJ_DIR)/main.o: src/main.cpp | check-toolchain
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ_DIR)/main.o
	@mkdir -p $(BUILD_DIR)
	$(LD) $< -o $@ $(LDFLAGS)

$(STAGE_DIR)/eboot.bin: $(TARGET).elf
	@mkdir -p $(STAGE_DIR)
	$(CREATE_SELF) -in=$< -out=$(TARGET).oelf --eboot "$@" \
		--paid 0x3800000000000011

$(STAGE_DIR)/sce_sys/about/right.sprx:
	@mkdir -p $(@D)
	cp $(TOOLCHAIN)/samples/piglet/sce_sys/about/right.sprx $@

$(STAGE_DIR)/sce_sys/icon0.png:
	@mkdir -p $(@D)
	cp $(TOOLCHAIN)/samples/SDL2/sce_sys/icon0.png $@

$(STAGE_DIR)/sce_module/libc.prx:
	@mkdir -p $(@D)
	cp $(TOOLCHAIN)/samples/SDL2/sce_module/libc.prx $@

$(STAGE_DIR)/sce_module/libSceFios2.prx:
	@mkdir -p $(@D)
	cp $(TOOLCHAIN)/samples/SDL2/sce_module/libSceFios2.prx $@

$(STAGE_DIR)/sce_sys/param.sfo: Makefile | check-toolchain
	@mkdir -p $(@D)
	$(PKG_TOOL) sfo_new $@
	$(PKG_TOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKG_TOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKG_TOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKG_TOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKG_TOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKG_TOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKG_TOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

PACKAGE_FILES := \
	$(STAGE_DIR)/eboot.bin \
	$(STAGE_DIR)/sce_sys/param.sfo \
	$(STAGE_DIR)/sce_sys/icon0.png \
	$(STAGE_DIR)/sce_sys/about/right.sprx \
	$(STAGE_DIR)/sce_module/libc.prx \
	$(STAGE_DIR)/sce_module/libSceFios2.prx

$(PKG): $(PACKAGE_FILES)
	@mkdir -p $(DIST_DIR)
	cd $(STAGE_DIR) && $(CREATE_GP4) -out pkg.gp4 --content-id=$(CONTENT_ID) \
		--files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx"
	cd $(STAGE_DIR) && $(PKG_TOOL) pkg_build pkg.gp4 .
	cp $(STAGE_DIR)/$(CONTENT_ID).pkg $@

verify: $(PKG)
	@test -s "$(TARGET).elf"
	@test -s "$(STAGE_DIR)/eboot.bin"
	@test -s "$(PKG)"
	@file "$(TARGET).elf"
	@sha256sum "$(PKG)"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
