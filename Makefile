# Simple LG launcher, native.
#   make            build for the TV (build/app/) and package it (build/*.ipk)
#   make install    copy to the TV and install it (TV_HOST from tv.env or the environment)
#   make test       the logic's tests, built and run on this computer
#   make ID=<another id>   the same under another id (its own data folder and boot links), to try a build beside the installed one
# The toolchain (webosbrew native-toolchain) is in .sdk/toolchain, our SDL (webosbrew SDL-webOS 2.30.12) in .sdk/sdl2.

TC      := .sdk/toolchain
SR      := $(TC)/arm-webos-linux-gnueabi/sysroot
SDL     := .sdk/sdl2
CXX_TV  := $(TC)/bin/arm-webos-linux-gnueabi-g++
OUT     := build/app
OBJ     := build/obj
APP     := simplelauncher
WEB_ID  := tld.my.simplelauncher
ID      ?= tld.my.simplelauncher

SRCS    := $(shell find src -name '*.cpp')
OBJS    := $(SRCS:src/%.cpp=$(OBJ)/%.o) $(OBJ)/third_party/qrcodegen.o
FLAGS   := --sysroot=$(SR) -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -D__WEBOS__ -D_REENTRANT -MMD -MP \
           -Isrc -Ithird_party -I$(SDL)/include/SDL2 -Ibuild/inc
LIBS    := -L$(SDL)/lib -L$(SR)/usr/lib -lSDL2 -lSDL2_ttf -lGLESv2 -lcurl -lturbojpeg -lpng16 -lhelpers -lpthread -static-libstdc++ -static-libgcc -Wl,-rpath,'$$ORIGIN/lib'
# libraries whose version on the TV differs from the SDK's go into lib/ (libhelpers, GLES, libpng and the C library are the TV's);
# the C++ library is linked in (the TV's libstdc++ is older than the toolchain's)
BUNDLED := libSDL2_ttf-2.0.so.0 libfreetype.so.6 libbz2.so.1 libturbojpeg.so.0 libcurl.so.4 libssl.so.1.1 libcrypto.so.1.1 libatomic.so.1

.PHONY: all package install test clean gen FORCE

all: package

build/inc/SDL_ttf.h: $(SR)/usr/include/SDL2/SDL_ttf.h
	@mkdir -p build/inc && cp $< $@

src/i18n/dates.inc: scripts/gen-dates.mjs
	node scripts/gen-dates.mjs
src/i18n/locales.inc: scripts/gen-i18n.mjs $(wildcard src/i18n/locales/*.json)
	node scripts/gen-i18n.mjs
src/data/editions.inc: scripts/gen-feeds.mjs data/editions.json
	node scripts/gen-feeds.mjs
src/data/catalog.inc: scripts/gen-catalog.mjs data/catalog.json
	node scripts/gen-catalog.mjs
assets/glyphs/house.png: scripts/gen-glyphs.mjs data/glyphs.json
	node scripts/gen-glyphs.mjs
gen: src/i18n/dates.inc src/i18n/locales.inc src/data/catalog.inc src/data/editions.inc assets/glyphs/house.png

# the id and the folder of its data, for the code (rewritten only when they change, so a change rebuilds what uses them)
build/inc/appid.h: FORCE
	@mkdir -p build/inc
	@printf '#pragma once\n#define APP_ID_TEXT "%s"\n#define DATA_DIR_TEXT "%s"\n' "$(ID)" \
		"/media/developer/$(if $(filter $(WEB_ID),$(ID)),simplelauncher,simplelauncher-native)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@
FORCE:

$(OBJ)/%.o: src/%.cpp build/inc/SDL_ttf.h build/inc/appid.h src/i18n/dates.inc src/i18n/locales.inc src/data/catalog.inc src/data/editions.inc
	@mkdir -p $(dir $@)
	$(CXX_TV) $(FLAGS) -c $< -o $@

$(OBJ)/third_party/qrcodegen.o: third_party/qrcodegen/qrcodegen.cpp
	@mkdir -p $(dir $@)
	$(CXX_TV) $(FLAGS) -c $< -o $@

$(OUT)/$(APP): $(OBJS)
	@mkdir -p $(OUT)/lib
	$(CXX_TV) --sysroot=$(SR) -o $@ $(OBJS) $(LIBS)

package: $(OUT)/$(APP) assets/glyphs/house.png
	cp -L $(SDL)/lib/libSDL2-2.0.so.0 $(OUT)/lib/
	for l in $(BUNDLED); do cp -L $(SR)/usr/lib/$$l $(OUT)/lib/ 2>/dev/null || cp -L $(SR)/lib/$$l $(OUT)/lib/; done
	node -e "const a=require('./appinfo.json'); if ('$(ID)'!=='$(WEB_ID)') { a.title='Simple launcher (' + '$(ID)'.split('.').pop() + ')'; a.appDescription='A test build of Simple LG launcher.'; } \
		a.id='$(ID)'; require('fs').writeFileSync('$(OUT)/appinfo.json', JSON.stringify(a, null, 2) + '\n')"
	rm -rf $(OUT)/tools && sh scripts/gen-tools.sh $(ID) $(OUT)/tools
	cp assets/app/icon.png assets/app/largeIcon.png $(OUT)/
	# what the web version's Swap opens after the update to this one (tools/migrate.sh)
	cp assets/app/index.html $(OUT)/
	# the web version kept loaded compares its build with this file and reloads (into index.html) when it differs (lib/update.ts)
	mkdir -p $(OUT)/assets && node -e "process.stdout.write(String(Date.now()))" > $(OUT)/assets/version.txt
	cp assets/app/splash-default.jpg $(OUT)/splash-default.jpg && cp assets/app/splash-default.jpg $(OUT)/splash-wallpaper.jpg
	mkdir -p $(OUT)/icons $(OUT)/licenses
	rm -rf $(OUT)/icons/apps $(OUT)/icons/inputs
	cp -R assets/icons/apps assets/icons/inputs $(OUT)/icons/
	rm -rf $(OUT)/glyphs && cp -R assets/glyphs $(OUT)/glyphs
	rm -rf $(OUT)/fonts && mkdir -p $(OUT)/fonts && cp fonts/*.ttf $(OUT)/fonts/ && cp fonts/OFL-*.txt $(OUT)/licenses/
	cp $(SDL)/share/licenses/SDL2/LICENSE.txt $(OUT)/licenses/SDL2.txt
	cp third_party/nlohmann/LICENSE.MIT $(OUT)/licenses/nlohmann-json.txt
	cp LICENSE $(OUT)/licenses/LICENSE.txt
	rm -f build/*.ipk
	npx ares-package $(OUT) -o build 2>&1 | grep -v -i -E "acg|depending on|for details|luna apis|^$$" || true
	@ls build/*.ipk

install: package
	sh scripts/install.sh

# ---- tests on this computer: the pure logic only ----
TEST_SRCS := src/data/rows.cpp src/data/order.cpp src/data/config.cpp src/data/prefs.cpp src/data/gallery.cpp src/data/homemode.cpp src/data/quick.cpp src/data/uninstall.cpp src/ui/boardlayout.cpp src/ui/plates.cpp src/i18n/dates.cpp src/i18n/i18n.cpp src/data/weather.cpp src/data/ics.cpp src/data/tz.cpp src/data/calendar.cpp src/data/htmltext.cpp src/data/feeds.cpp src/data/quote.cpp src/core/unicode.cpp src/core/sha256.cpp src/data/update.cpp src/data/storage.cpp src/data/cities.cpp src/data/inputs.cpp src/data/sun.cpp src/sys/http.cpp src/sys/fs.cpp src/core/log.cpp tests/stubs.cpp tests/tests.cpp
build/tests: $(TEST_SRCS) $(shell find src tests -name '*.h') src/i18n/dates.inc src/i18n/locales.inc src/data/editions.inc
	@mkdir -p build
	c++ -std=c++17 -O1 -Wall -Isrc -Ithird_party -o $@ $(TEST_SRCS) -lcurl -liconv
test: build/tests
	./build/tests
	node scripts/check-i18n.mjs
	sh tests/test-homehook.sh

clean:
	rm -rf build

-include $(OBJS:.o=.d)
