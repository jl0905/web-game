CC          := gcc
RAYLIB_SRC  := vendors/raylib/src
RAYLIB_LIB  := $(RAYLIB_SRC)/libraylib.a
RAYLIB_STAMP := $(RAYLIB_SRC)/.build-platform
CFLAGS      := -std=c99 -Wall -Wextra -O2 -I$(RAYLIB_SRC)
LIBS        := $(RAYLIB_LIB) -lopengl32 -lgdi32 -lwinmm -lm

game.exe: src/main.c $(RAYLIB_LIB)
	$(CC) $(CFLAGS) src/main.c $(LIBS) -o game.exe

# raylib's Makefile compiles objects in-tree, so a web build (build.js) would
# leave stale objects behind. Rebuild for desktop if it was built for web.
$(RAYLIB_LIB): FORCE
	@type "$(RAYLIB_STAMP)" 2>nul | findstr /C:"desktop" >nul || mingw32-make -C $(RAYLIB_SRC) clean
	mingw32-make -C $(RAYLIB_SRC)
	@echo desktop> "$(RAYLIB_STAMP)"

FORCE:

run: game.exe
	./game.exe

clean:
	-del game.exe
	mingw32-make -C $(RAYLIB_SRC) clean
	-del "$(RAYLIB_STAMP)"

.PHONY: run clean FORCE
