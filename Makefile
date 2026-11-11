# Sumvita - PS Vita calculator. Plain make, VitaSDK, built in WSL:
#   make -j4          (in WSL, from the project folder)
#   -> build/Sumvita.vpk
# Host (PC) engine tests: make -C tests   (see tests/Makefile)

VITASDK ?= $(HOME)/vitasdk
PREFIX  := $(VITASDK)/bin/arm-vita-eabi
CC      := $(PREFIX)-gcc
SIZE    := $(PREFIX)-size

TITLE    := Sumvita
TITLE_ID := SUMV00001
APP_VER  := 01.00
TARGET   := sumvita
VPK_NAME := Sumvita.vpk
BUILD    ?= build

.DEFAULT_GOAL := all

SRCS := $(shell find src -name '*.c' | sort)
EXTRA_CFLAGS :=
# UI_ONLY=1: build the UI against the small engine core only (arena/num/steps/expr)
# plus the UI's own dev stub (src/ui/dev_stub.c, compiled only in this mode), so UI
# work never waits on engine files that are mid-edit. Output goes to build-ui/.
ifeq ($(UI_ONLY),1)
EXTRA_CFLAGS := -DSUMVITA_UI_ONLY=1
SRCS := $(sort $(shell find src/ui -name '*.c') src/main.c src/engine/arena.c src/engine/num.c src/engine/steps.c src/engine/expr.c)
# parse/print/eval join once the engine agent marks them finished (.markers/front-ready)
ifneq ($(wildcard .markers/front-ready),)
SRCS += src/engine/parse.c src/engine/parse_lex.c src/engine/print.c src/engine/eval.c
EXTRA_CFLAGS += -DSUMVITA_HAVE_FRONT=1
endif
BUILD := build-ui
endif
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

CFLAGS := -std=gnu11 -Wall -Wextra -Werror -O2 -Isrc -MMD -MP -ffast-math -fno-finite-math-only
CFLAGS += -I$(VITASDK)/arm-vita-eabi/include -I$(VITASDK)/arm-vita-eabi/include/freetype2
CFLAGS += -DSUMVITA_VITA=1 -DSUMVITA_VERSION=\"$(APP_VER)\" $(EXTRA_CFLAGS)

LIBS := -lvita2d -lfreetype -lbz2 -lpng16 -lz -lm \
        -lSceTouch_stub -lSceIme_stub -lSceCtrl_stub -lSceGxm_stub -lSceDisplay_stub \
        -lSceSysmodule_stub -lSceCommonDialog_stub -lSceAppUtil_stub -lSceAppMgr_stub \
        -lScePgf_stub -lScePvf_stub -lSceKernelDmacMgr_stub -lScePower_stub -lSceRtc_stub \
        -lc

# Files packed into the VPK next to eboot.bin (app0:...)
VPK_FILES := -s $(BUILD)/param.sfo \
	-b $(BUILD)/eboot.bin \
	-a sce_sys/icon0.png=sce_sys/icon0.png \
	-a sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
	-a sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
	-a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	$(foreach f,$(wildcard assets/fonts/*.ttf),-a $(f)=fonts/$(notdir $(f))) \
	$(foreach f,$(wildcard assets/img/*.png),-a $(f)=img/$(notdir $(f)))

all: $(BUILD)/$(VPK_NAME)

$(BUILD)/$(VPK_NAME): $(BUILD)/eboot.bin $(BUILD)/param.sfo
	$(VITASDK)/bin/vita-pack-vpk $(VPK_FILES) $@

$(BUILD)/eboot.bin: $(BUILD)/$(TARGET).velf
	$(VITASDK)/bin/vita-make-fself -s $< $@

$(BUILD)/$(TARGET).velf: $(BUILD)/$(TARGET).elf
	$(VITASDK)/bin/vita-elf-create $< $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) -Wl,-q $(OBJS) $(LIBS) -o $@

$(BUILD)/param.sfo:
	@mkdir -p $(BUILD)
	$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_VER=$(APP_VER) -d ATTRIBUTE2=12 "$(TITLE)" $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

sizes: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean sizes
-include $(DEPS)
