.DEFAULT_GOAL := all
TARGET := BookReader
APP_TITLE := BookReader
APP_AUTHOR := Homebrew
APP_VERSION := 0.2.0
BUILD := build
SOURCES := source source/app source/ui source/readers source/render source/storage source/config source/utils source/net

ifeq ($(MAKECMDGOALS),clean)
.PHONY: clean
clean:
	$(RM) -r build BookReader.nro BookReader.elf BookReader.nacp BookReader.map
else ifeq ($(MAKECMDGOALS),host)
.PHONY: host
host:
	$(MAKE) -f Makefile.host
else ifeq ($(MAKECMDGOALS),test)
.PHONY: test
test:
	$(MAKE) -f Makefile.host test
else ifeq ($(MAKECMDGOALS),package)
.PHONY: package
package:
	python3 tools/package.py
else
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Install devkitPro and run: export DEVKITPRO=/opt/devkitpro)
endif
include $(DEVKITPRO)/libnx/switch_rules
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -Wall -Wextra -ffunction-sections $(ARCH) $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -std=gnu++17 -fexceptions -frtti
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
# MuPDF 1.17 ships bundled third-party codecs in libmupdf-third.a.
# SDL2's full static dependency closure comes from the installed .pc file.
PKG_CONFIG := $(DEVKITPRO)/portlibs/switch/bin/aarch64-none-elf-pkg-config
LIBS := -Wl,--start-group -lmupdf -lmupdf-third $(shell $(PKG_CONFIG) --static --libs sdl2 freetype2) -ljpeg -lpng -lz -lbz2 -lm -lnx -Wl,--end-group
LIBDIRS := $(PORTLIBS) $(LIBNX)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o)
export INCLUDE := -I$(CURDIR)/source -I$(CURDIR)/include $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(PORTLIBS)/include/freetype2 -I$(PORTLIBS)/include/SDL2
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export NROFLAGS := --icon=$(CURDIR)/icon.jpg --nacp=$(CURDIR)/$(TARGET).nacp
export APP_TITLE APP_AUTHOR APP_VERSION
.PHONY: all $(BUILD)
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
else
.PHONY: all
all: $(OUTPUT).nro
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
-include $(OFILES:.o=.d)
endif
endif
