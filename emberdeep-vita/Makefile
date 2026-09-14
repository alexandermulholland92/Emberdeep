# ==========================================================
#  Dungeons of the Emberdeep - PS Vita
#  `make` produces emberdeep.vpk
# ==========================================================

TARGET   := emberdeep
TITLE_ID := EMBR00001
TITLE    := Dungeons of the Emberdeep
VERSION  := 01.00

OBJS := src/main.o src/actors.o src/dungeon.o src/render.o src/texture.o src/font.o

PREFIX := arm-vita-eabi
CC     := $(PREFIX)-gcc
STRIP  := $(PREFIX)-strip

CFLAGS := -Wl,-q -Wall -O3 -std=c99 -mtune=cortex-a9 -mfpu=neon -ffast-math -Isrc

# vitaGL pulls in the shader compiler stubs; if your vitaGL predates
# vitashark, drop -lvitashark -lSceShaccCgExt -lSceShaccCg_stub -ltaihen_stub
LIBS := -lvitaGL -lvitashark -lSceShaccCgExt -lmathneon \
        -ltaihen_stub -lSceShaccCg_stub \
        -lSceKernelDmacMgr_stub -lSceGxm_stub -lSceDisplay_stub \
        -lSceSysmodule_stub -lSceCtrl_stub -lSceAppMgr_stub \
        -lSceCommonDialog_stub -lm
# Add to the end of your LDLIBS or LDFLAGS (LDLIBS is usually after objects)
LDLIBS += -lstdc++ -lsupc++
# or LDFLAGS += -lstdc++ -lsupc++
all: $(TARGET).vpk

$(TARGET).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		$@
	@echo ""
	@echo "  built $(TARGET).vpk - copy to the Vita and install with VitaShell"
	@echo ""

eboot.bin: $(TARGET).velf
	vita-make-fself -s $< $@

param.sfo:
	vita-mksfoex -s TITLE_ID="$(TITLE_ID)" -s APP_VER="$(VERSION)" "$(TITLE)" param.sfo

$(TARGET).velf: $(TARGET).elf
	cp $< $<.unstripped.elf
	$(STRIP) -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) $(LDLIBS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# host-side verification of the simulation (no Vita toolchain needed)
test:
	cc -std=c99 -Wall -Isrc tests/host_test.c src/dungeon.c src/actors.c -lm \
		-o /tmp/emberdeep_test
	@echo "--- full run, survival disabled (checks all three floors) ---"
	@for c in 0 1 2; do /tmp/emberdeep_test $$c 1 101; done
	@echo "--- unassisted ---"
	@for c in 0 1 2; do /tmp/emberdeep_test $$c 0 202; done

# type-check the Vita-only files without the SDK, using stub headers
check:
	cc -fsyntax-only -std=c99 -Wall -Wextra -Wno-unused-parameter \
		-Isrc -Itests/stubs src/render.c src/main.c src/texture.c
	@echo "render.c and main.c type-check clean"

clean:
	rm -f $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(TARGET).elf.unstripped.elf \
	      eboot.bin param.sfo $(OBJS)

.PHONY: all clean test check
