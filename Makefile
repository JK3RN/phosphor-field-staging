# Phosphor Field — build, test and install.
CC ?= cc
CFLAGS ?= -O2 -g
override CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
PYTHON ?= python3

APP := phosphor-field
DEPS := sdl2 SDL2_ttf
SRC := src/main.c src/theme.c src/observe.c
SUPPORT := src/theme.c src/observe.c
HEADERS := $(wildcard src/*.h)
SDL_TESTS := core operational selection lab creation aquarium
SAN := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

ifeq ($(filter clean uninstall,$(MAKECMDGOALS)),)
ifneq ($(shell $(PKG_CONFIG) --exists $(DEPS) 2>/dev/null && echo ok),ok)
$(error Missing dependency: SDL2 and SDL2_ttf development files (pkg-config modules: $(DEPS)))
endif
endif
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(DEPS) 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs $(DEPS) 2>/dev/null)

all: $(APP)

$(APP): $(SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(SDL_LIBS) -lm

build/qa:
	mkdir -p $@

# Unit, render and headless smoke tests. Needs a monospace TTF (see README).
check: $(APP) | build/qa
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_generator.c -o build/test-generator -lm
	build/test-generator
	set -e; for t in $(SDL_TESTS); do \
	  $(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) tests/test_$$t.c $(SUPPORT) -o build/test-$$t $(LDFLAGS) $(SDL_LIBS) -lm; \
	  SDL_VIDEODRIVER=dummy build/test-$$t; \
	done
	./$(APP) --version
	./$(APP) --help > /dev/null
	SDL_VIDEODRIVER=dummy ./$(APP) --seconds 1
	SDL_VIDEODRIVER=dummy ./$(APP) --points 50000 --seconds 1
	SDL_VIDEODRIVER=dummy ./$(APP) --qa --seconds 1

# The same suites under AddressSanitizer and UBSan (leak detection off: SDL/driver noise).
sanitize: | build/qa
	$(CC) -std=c11 $(SAN) tests/test_generator.c -o build/san-generator -lm
	UBSAN_OPTIONS=halt_on_error=1 build/san-generator
	set -e; for t in $(SDL_TESTS); do \
	  $(CC) -std=c11 $(SAN) $(CPPFLAGS) $(SDL_CFLAGS) tests/test_$$t.c $(SUPPORT) -o build/san-$$t $(SDL_LIBS) -lm; \
	  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 SDL_VIDEODRIVER=dummy build/san-$$t; \
	done
	$(CC) -std=c11 $(SAN) $(CPPFLAGS) $(SDL_CFLAGS) $(SRC) -o build/san-$(APP) $(SDL_LIBS) -lm
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 SDL_VIDEODRIVER=dummy build/san-$(APP) --points 50000 --seconds 3
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 SDL_VIDEODRIVER=dummy build/san-$(APP) --qa --seconds 3

# Process, JSON status, pipe, logging and live-palette tests in private temp dirs.
check-integration: $(APP) | build/qa
	$(PYTHON) tests/test_integration.py

# Real event loop: Aquarium with mixed native/formula favourites at 10k and 50k points.
check-aquarium: | build/qa
	$(PYTHON) tests/check_aquarium.py

# Regenerate docs/media from the real render path (needs ffmpeg).
showcase: $(APP)
	tools/showcase/capture.sh

install: $(APP)
	install -Dm755 $(APP) $(DESTDIR)$(BINDIR)/$(APP)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(APP)

clean:
	rm -rf build $(APP)

.PHONY: all check sanitize check-integration check-aquarium showcase install uninstall clean
