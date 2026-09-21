# ==========================================================
#  Dungeons of the Emberdeep - PS Vita
#  `make` produces emberdeep.vpk
# ==========================================================

TARGET   := emberdeep
TITLE_ID := EMBR00001
TITLE    := Dungeons of the Emberdeep
VERSION  := 01.00

OBJS := src/main.o src/actors.o src/dungeon.o src/render.o src/texture.o \
        src/geom.o src/model.o src/model_data.o src/anim.o src/font.o

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
# vitaGL is C++, so the C driver needs the C++ runtime explicitly or the link
# fails with undefined __cxa_* / operator new references
LDLIBS += -lstdc++ -lsupc++

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
	@for c in 0 1 2; do /tmp/emberdeep_test $$c 1 303; done
	@echo "--- unassisted ---"
	@for c in 0 1 2; do /tmp/emberdeep_test $$c 0 404; done
	@echo "--- world-mesh cache key (regression: stale walls on a restart) ---"
	@cc -std=c99 -Wall -Wextra -Isrc tests/regen_test.c src/dungeon.c -lm \
		-o /tmp/emberdeep_regen
	@/tmp/emberdeep_regen

# type-check the Vita-only files without the SDK, using stub headers
check:
	cc -fsyntax-only -std=c99 -Wall -Wextra -Wno-unused-parameter \
		-Isrc -Itests/stubs src/render.c src/main.c src/texture.c src/geom.c \
		src/model.c src/model_data.c src/anim.c
	@echo "vita-only sources type-check clean"

# prove the procedural surfaces still match the browser build byte for byte.
# needs node and a copy of emberdeep.html:  make texcheck HTML=path/to/emberdeep.html
HTML ?= emberdeep.html
texcheck:
	@command -v node >/dev/null || { echo "texcheck needs node"; exit 1; }
	@test -f "$(HTML)" || { echo "texcheck needs the browser build: make texcheck HTML=path/to/emberdeep.html"; exit 1; }
	@rm -rf /tmp/emberdeep_tex && mkdir -p /tmp/emberdeep_tex/ref /tmp/emberdeep_tex/got
	@node tests/tex_ref.js "$(HTML)" /tmp/emberdeep_tex/ref >/dev/null
	@cc -std=c99 -Wall -Wextra -O2 -DTEX_HOST_HARNESS -Isrc \
		tests/tex_dump.c src/texture.c -lm -o /tmp/emberdeep_tex/dump
	@/tmp/emberdeep_tex/dump /tmp/emberdeep_tex/got >/dev/null
	@fail=0; for f in floorStone wallMason rock bone cloth leather metal wood skin hide; do \
		if cmp -s /tmp/emberdeep_tex/ref/$$f.raw /tmp/emberdeep_tex/got/$$f.raw; then \
			printf "  %-11s exact match\n" "$$f"; \
		else printf "  %-11s DIFFERS\n" "$$f"; fail=1; fi; done; \
	if [ $$fail = 0 ]; then echo "all 10 surfaces byte-identical to the browser build"; \
	else echo "surface mismatch"; exit 1; fi

# prove the tessellators still match Three.js r128 triangle for triangle.
# needs node, the three package, and a copy of emberdeep.html:
#   make geomcheck HTML=path/to/emberdeep.html
geomcheck:
	@command -v node >/dev/null || { echo "geomcheck needs node"; exit 1; }
	@test -f "$(HTML)" || { echo "geomcheck needs the browser build: make geomcheck HTML=path/to/emberdeep.html"; exit 1; }
	@mkdir -p /tmp/emberdeep_geom
	@node tests/model_ref.js "$(HTML)" --geom > /tmp/emberdeep_geom/geoms.json
	@cc -std=c99 -Wall -Wextra -O2 -Isrc tests/geom_dump.c src/geom.c -lm \
		-o /tmp/emberdeep_geom/dump
	@python3 tests/geom_check.py /tmp/emberdeep_geom/geoms.json /tmp/emberdeep_geom/dump

# prove the baked model trees still pose exactly as Three.js does.
#   make modelcheck HTML=path/to/emberdeep.html
modelcheck:
	@command -v node >/dev/null || { echo "modelcheck needs node"; exit 1; }
	@test -f "$(HTML)" || { echo "modelcheck needs the browser build: make modelcheck HTML=path/to/emberdeep.html"; exit 1; }
	@mkdir -p /tmp/emberdeep_model
	@node tests/model_ref.js "$(HTML)" > /tmp/emberdeep_model/models.json
	@cc -std=c99 -Wall -Wextra -O2 -DTEX_HOST_HARNESS -Isrc tests/model_dump.c \
		src/model.c src/model_data.c src/geom.c -lm -o /tmp/emberdeep_model/dump
	@python3 tests/model_check.py /tmp/emberdeep_model/models.json /tmp/emberdeep_model/dump

# prove the ported animateActor still poses identically to the browser's.
#   make animcheck HTML=path/to/emberdeep.html
animcheck:
	@command -v node >/dev/null || { echo "animcheck needs node"; exit 1; }
	@test -f "$(HTML)" || { echo "animcheck needs the browser build: make animcheck HTML=path/to/emberdeep.html"; exit 1; }
	@mkdir -p /tmp/emberdeep_anim
	@node tests/model_ref.js "$(HTML)" --anim > /tmp/emberdeep_anim/anim.json
	@cc -std=c99 -Wall -Wextra -O2 -DTEX_HOST_HARNESS -Isrc tests/anim_dump.c \
		src/anim.c src/model.c src/model_data.c src/geom.c -lm \
		-o /tmp/emberdeep_anim/dump
	@python3 tests/anim_check.py /tmp/emberdeep_anim/anim.json /tmp/emberdeep_anim/dump

clean:
	rm -f $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(TARGET).elf.unstripped.elf \
	      eboot.bin param.sfo $(OBJS)

.PHONY: all clean test check texcheck geomcheck modelcheck animcheck
