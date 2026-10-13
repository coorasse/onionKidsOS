CC ?= cc
PKGS = sdl2 SDL2_image SDL2_ttf
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -std=gnu11 -DPLATFORM_DESKTOP -DUSE_SDL2
# menu music on the desktop needs SDL2_mixer (brew install sdl2_mixer); without it the launcher is silent
ifeq ($(shell pkg-config --exists SDL2_mixer && echo yes),yes)
PKGS += SDL2_mixer
CFLAGS += -DHAVE_MIXER
endif
CFLAGS += $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
LDLIBS = $(shell pkg-config --libs $(PKGS) 2>/dev/null) -lm

COMMON = src/main.c src/gfx.c src/library.c src/kid_ui.c src/resume_ui.c src/osd.c src/input.c src/coins.c src/i18n.c
HEADERS = $(wildcard src/*.h)

SHOT_SCRIPT = shot:01-kid-first,right,shot:02-kid-second,right,right,shot:03-kid-fourth,\
hold:select+start:1500,shot:04-hold-progress,wait:600,a,shot:05-playing,b,volup,shot:06-volume,\
voldown,voldown,voldown,shot:07-volume-down

build/kidsos: $(COMMON) src/platform_desktop.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(COMMON) src/platform_desktop.c $(LDLIBS)

run: build/kidsos
	./build/kidsos

# runs on a copy of sdcard/ so the scripted toggles do not change the fixture
shots: build/kidsos
	@mkdir -p shots && rm -f shots/*.png
	rm -rf build/sdcard && cp -R sdcard build/sdcard
	KIDS_ROOT=build/sdcard ./build/kidsos --script "$(SHOT_SCRIPT)" --shots shots

shots-onion: build/kidsos
	@mkdir -p shots/onion && rm -f shots/onion/*.png
	rm -rf build/sdcard-onion && cp -R sdcard-onion build/sdcard-onion
	KIDS_ROOT=build/sdcard-onion ./build/kidsos --script "$(SHOT_SCRIPT)" --shots shots/onion

# arcade coins: only the arcade game enabled, insert coins in the desktop stub until they run out
shots-coins: build/kidsos
	@mkdir -p shots/coins && rm -f shots/coins/*.png
	rm -rf build/sdcard-coins && cp -R sdcard build/sdcard-coins
	printf 'Arcade (FBN)/Street Fighter II.zip\nGame Boy (GB)/Tetris.gb\n' > build/sdcard-coins/kids/enabled.txt
	printf 'arcade_coins_per_day=3\n' > build/sdcard-coins/kids/settings.txt
	rm -f build/sdcard-coins/kids/coins.txt
	KIDS_ROOT=build/sdcard-coins ./build/kidsos --script "shot:01-three-coins,a,select,select,shot:02-playing-one-left,select,shot:03-playing-none-left,select,shot:04-select-ignored,b,a,shot:05-blocked" --shots shots/coins

# resume screen: the Onion fixture has an auto save state for Pokemon (with thumbnail) and Mario Kart (without)
shots-resume: build/kidsos
	@mkdir -p shots/resume
	rm -rf build/sdcard-resume && cp -R sdcard-onion build/sdcard-resume
	python3 tools/make_covers.py --favourites build/sdcard-resume "GB/Tetris.gb" "GBA/Mario Kart Super Circuit.gba" "GBA/Pokemon Emerald.gba"
	KIDS_ROOT=build/sdcard-resume ./build/kidsos --script "shot:01-kid,a,shot:02-no-state-plays-directly,b,right,right,a,shot:03-continue,right,shot:04-start-over,b,shot:05-back,left,a,shot:06-no-thumbnail,a,shot:07-playing" --shots shots/resume

# every language on the screens with text: kid screen, hold-to-leave hint, resume choice, no coins, no games
LANGS = en de it fr es nl
shots-lang: build/kidsos
	rm -rf shots/lang build/sdcard-lang build/sdcard-lang-coins build/sdcard-lang-empty
	cp -R sdcard-onion build/sdcard-lang
	python3 tools/make_covers.py --favourites build/sdcard-lang "GB/Tetris.gb" "GBA/Mario Kart Super Circuit.gba" "GBA/Pokemon Emerald.gba"
	cp -R sdcard build/sdcard-lang-coins
	printf 'Arcade (FBN)/Street Fighter II.zip\n' > build/sdcard-lang-coins/kids/enabled.txt
	printf 'arcade_coins_per_day=3\n' > build/sdcard-lang-coins/kids/settings.txt
	cp -R sdcard build/sdcard-lang-empty
	: > build/sdcard-lang-empty/kids/enabled.txt
	for l in $(LANGS); do \
		mkdir -p shots/lang/$$l; \
		rm -f build/sdcard-lang-coins/kids/coins.txt; \
		KIDS_LANG=$$l KIDS_ROOT=build/sdcard-lang ./build/kidsos --script "shot:01-kid,hold:select+start:1500,shot:02-hold,wait:600,right,right,a,shot:03-continue,right,shot:04-start-over" --shots shots/lang/$$l; \
		KIDS_LANG=$$l KIDS_ROOT=build/sdcard-lang-coins ./build/kidsos --script "a,select,select,select,b,a,shot:05-no-coins" --shots shots/lang/$$l; \
		KIDS_LANG=$$l KIDS_ROOT=build/sdcard-lang-empty ./build/kidsos --script "shot:06-no-games" --shots shots/lang/$$l; \
	done

covers:
	python3 tools/make_covers.py

# Box art for the real card from thumbnails.libretro.com: make fetch-covers SD=/Volumes/ONIONOS
SD ?= /Volumes/ONIONOS
fetch-covers:
	ruby tools/fetch_covers.rb $(SD)

# ---- Miyoo Mini Plus: cross-compiled inside MinUI's toolchain container ----
TOOLCHAIN_DIR = toolchains/miyoomini-toolchain
TOOLCHAIN_IMAGE = miyoomini-toolchain
PAK_DIR = build/pak/Tools/miyoomini/Kids.pak

$(TOOLCHAIN_DIR)/.build:
	@test -d $(TOOLCHAIN_DIR) || git clone --depth 1 https://github.com/shauninman/union-miyoomini-toolchain.git $(TOOLCHAIN_DIR)
	cd $(TOOLCHAIN_DIR) && docker build -t $(TOOLCHAIN_IMAGE) . && touch .build

# the container runs as root; on Linux it would leave build/ unwritable for the host user, hence the chown
CROSS_BUILD = docker run --rm -v "$(CURDIR)":/root/workspace $(TOOLCHAIN_IMAGE) /bin/bash -c \
	'. /root/setup-env.sh && cd /root/workspace && make $(1) && chown -R $(shell id -u):$(shell id -g) build'

miyoomini: $(TOOLCHAIN_DIR)/.build
	@mkdir -p build
	$(call CROSS_BUILD,build/miyoomini/kidsos.elf)

# this rule runs inside the container, where CROSS_COMPILE and PREFIX are set
build/miyoomini/kidsos.elf: $(COMMON) src/platform_miyoomini.c $(HEADERS)
	@mkdir -p build/miyoomini
	$(CROSS_COMPILE)gcc -O2 -Wall -Wextra -std=gnu11 -DPLATFORM_MIYOOMINI -Wno-format-truncation -I$(PREFIX)/include/SDL \
		-o $@ $(COMMON) src/platform_miyoomini.c -lSDL -lSDL_image -lSDL_ttf -lm -lpthread -s

pak: miyoomini
	rm -rf build/pak && mkdir -p $(PAK_DIR)/res
	cp build/miyoomini/kidsos.elf $(PAK_DIR)/
	cp pak/launch.sh $(PAK_DIR)/
	cp assets/Fredoka-Bold.ttf assets/logo.png $(PAK_DIR)/res/
	cd build/pak && rm -f ../KidsOS-miyoomini.zip && zip -qr ../KidsOS-miyoomini.zip Tools
	@echo "Copy build/pak/Tools onto the SD card root (or unzip build/KidsOS-miyoomini.zip there)."

# ---- Miyoo Mini Plus running OnionOS: the launcher replaces MainUI at boot ----
onion: $(TOOLCHAIN_DIR)/.build
	@mkdir -p build
	$(call CROSS_BUILD,build/onion/kidsos.elf)

build/onion/kidsos.elf: $(COMMON) src/platform_onion.c src/coinmon.c src/coinlock.c $(HEADERS)
	@mkdir -p build/onion
	$(CROSS_COMPILE)gcc -O2 -Wall -Wextra -std=gnu11 -DPLATFORM_ONION -Wno-format-truncation -I$(PREFIX)/include/SDL \
		-o $@ $(COMMON) src/platform_onion.c -lSDL -lSDL_image -lSDL_ttf -lm -lpthread -ldl -s
	$(CROSS_COMPILE)gcc -O2 -Wall -Wextra -std=gnu11 -o build/onion/coinmon.elf src/coinmon.c src/coins.c -s
	$(CROSS_COMPILE)gcc -O2 -Wall -Wextra -std=gnu11 -shared -fPIC -I$(PREFIX)/include/SDL -o build/onion/libcoinlock.so src/coinlock.c -ldl -s

DIST = build/onion/dist/.kidsos
onion-dist: onion
	rm -rf build/onion/dist && mkdir -p $(DIST)/res
	cp build/onion/kidsos.elf build/onion/coinmon.elf build/onion/libcoinlock.so onion/launch.sh onion/game.sh onion/covers.sh onion/startup.sh onion/install.sh onion/uninstall.sh $(DIST)/
	cp assets/Fredoka-Bold.ttf assets/logo.png $(DIST)/res/
	cp -R onion/app $(DIST)/app
	cp -R onion/theme $(DIST)/theme
	cp onion/bootScreen.png $(DIST)/

# ---- The zip parents install: the official Onion release with the kids launcher inside its onion.pak ----
KIDSOS_VERSION = 0.1.1
ONION_VERSION = 4.3.1-1
ONION_SHA256 = 4739938e45688ade02d2c8947d4012345916d1ca04ab33f5b2b1949bf8e66166
KIDSOS_ZIP = build/KidsOS-$(KIDSOS_VERSION)-onion$(ONION_VERSION).zip

dist: onion-dist
	ruby tools/build_dist.rb $(KIDSOS_VERSION) $(ONION_VERSION) $(ONION_SHA256)

# boots the runtime.sh from the zip's onion.pak in BusyBox: the startup hook must install the shims by itself
test-dist: ../reference/Onion
	ruby tools/test_onion_install.rb ../reference/Onion --dist $(KIDSOS_ZIP)

# plays Onion's installer from the zip on a fresh card: no screen may wait for a button before the reboot
test-first-install:
	ruby tools/test_first_install.rb $(KIDSOS_ZIP)

# Over Wi-Fi: enable SSH in Onion (Apps > Tweaks > Network), then: make deploy MIYOO=<device ip>
MIYOO ?=
SSH = tools/miyoo-ssh.sh $(MIYOO)

deploy: onion-dist
	@test -n "$(MIYOO)" || { echo "usage: make deploy MIYOO=<device ip>"; exit 1; }
	tar -C build/onion/dist -cf - .kidsos | $(SSH) 'tar -C /mnt/SDCARD -xf -'
	$(SSH) 'sh /mnt/SDCARD/.kidsos/install.sh'

undeploy:
	$(SSH) 'sh /mnt/SDCARD/.kidsos/uninstall.sh'

# the SD card is mounted on this Mac (SD=/Volumes/ONIONOS): install without the device, install.sh runs in BusyBox
deploy-card: onion-dist
	@test -n "$(SD)" || { echo "usage: make deploy-card SD=/Volumes/<card>"; exit 1; }
	@mkdir -p build/device-logs
	-cp "$(SD)/.kidsos/kidsos.log" "$(SD)/.kidsos/coinmon.log" build/device-logs/ 2>/dev/null
	rm -rf "$(SD)/.kidsos" && cp -R build/onion/dist/.kidsos "$(SD)/.kidsos"
	dot_clean -m "$(SD)/.kidsos"
# Docker Desktop keeps a stale view of a volume that was ejected and reinserted: run install.sh only when needed
	@if grep -q KIDSOS "$(SD)/.tmp_update/bin/MainUI-354-clean" "$(SD)/.tmp_update/bin/gameSwitcher"; then \
		echo "shims already installed"; \
	else \
		docker run --rm -v "$(SD):/mnt/SDCARD" busybox:1.37.0 sh /mnt/SDCARD/.kidsos/install.sh && dot_clean -m "$(SD)/.tmp_update/bin"; \
	fi
	sync

# the SD card is mounted on this Mac (e.g. SD=/Volumes/NO\ NAME): put Onion's original binaries back
recover:
	@test -n "$(SD)" || { echo "usage: make recover SD=/Volumes/<card>"; exit 1; }
	for real in "$(SD)"/.tmp_update/bin/*.real; do mv "$$real" "$${real%.real}"; done
	@echo "Onion binaries restored, eject the card"

# runs onion/install.sh against a copy of Onion's SD layout in a BusyBox container and boots runtime.sh;
# RUNTIME=/Volumes/<card>/.tmp_update/runtime.sh boots the one of a real card instead
RUNTIME ?=
test-onion: ../reference/Onion
	ruby tools/test_onion_install.rb ../reference/Onion $(RUNTIME)

../reference/Onion:
	git clone --depth 1 https://github.com/onionui/Onion $@

# runs onion/covers.sh in a BusyBox container against a fake card and a fake thumbnails.libretro.com
test-covers:
	ruby tools/test_covers.rb

# restart the UI loop without rebooting: Onion's runtime.sh comes straight back to the kids launcher
device-restart:
	$(SSH) 'rm -f /tmp/kidsos_exit; killall MainUI MainUI-354-clean.real MainUI-354-expert.real kidsos.elf 2>/dev/null; true'

device-log:
	$(SSH) 'cat /mnt/SDCARD/.kidsos/kidsos.log; echo --- coinmon; tail -20 /mnt/SDCARD/.kidsos/coinmon.log 2>/dev/null; echo --- runtime; tail -40 /mnt/SDCARD/.tmp_update/logs/runtime.log 2>/dev/null'

device-shot:
	@mkdir -p shots
	$(SSH) 'cat /dev/fb0' > build/fb.raw && python3 tools/fb2png.py build/fb.raw shots/device.png

# the README screenshots: the launcher runs a second time on the device with scripted input and saves BMPs
# of its own frames (the first favourite needs a save state for the continue / start over screens)
DEVICE_SHOT_SCRIPT = shot:kid-ui,right,shot:kid-ui-second,hold:select+start:1500,shot:parent-hold,wait:600,\
volup,shot:volume,voldown,left,wait:1500,a,wait:800,shot:resume-continue,right,wait:800,shot:resume-start-over,b
DEVICE_SHOT_ENV = cd /mnt/SDCARD/.kidsos && export LD_LIBRARY_PATH=/lib:/config/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte
device-shots:
	@mkdir -p shots/device && rm -f shots/device/*
	$(SSH) 'rm -rf /tmp/shots && mkdir -p /tmp/shots && $(DEVICE_SHOT_ENV) && \
		./kidsos.elf --script "$(DEVICE_SHOT_SCRIPT)" --shots /tmp/shots > /dev/null 2>&1 && \
		echo "boot-$$(cat /proc/sys/kernel/random/boot_id) 10" > /mnt/SDCARD/kids/coins.txt && \
		./kidsos.elf --script "shot:coins-none" --shots /tmp/shots > /dev/null 2>&1; rm -f /mnt/SDCARD/kids/coins.txt; \
		cd /tmp/shots && tar cf - .' | tar -C shots/device -xf -
	for f in shots/device/*.png; do sips -s format png "$$f" --out "$$f" > /dev/null; done
	@echo "copy the ones you want to docs/images/ and compress them with tinify"

clean:
	rm -rf build shots/*.png shots/onion

.PHONY: test-first-install run shots shots-onion shots-coins shots-resume covers miyoomini pak onion onion-dist dist test-dist deploy deploy-card undeploy recover test-onion test-covers device-restart device-log device-shot device-shots clean
