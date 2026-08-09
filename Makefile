CC          := gcc
RAYLIB_SRC  := vendors/raylib/src
RAYLIB_LIB  := $(RAYLIB_SRC)/libraylib.a
RAYLIB_STAMP := $(RAYLIB_SRC)/.build-platform
SQLITE_DIR  := vendors/sqlite
SQLITE_C    := $(SQLITE_DIR)/sqlite3.c
SQLITE_O    := $(SQLITE_DIR)/sqlite3.o
INCLUDE_DIR := include
SRCS        := src/main.c src/query.c src/ecs.c src/physics.c
OBJS        := $(SRCS:.c=.o)
CFLAGS      := -std=c99 -Wall -Wextra -O2 -I$(RAYLIB_SRC) -I$(SQLITE_DIR) -I$(INCLUDE_DIR)
SQLITE_CFLAGS := -std=c99 -O2 -DSQLITE_OMIT_LOAD_EXTENSION
LIBS        := $(RAYLIB_LIB) $(SQLITE_O) -lopengl32 -lgdi32 -lwinmm -lm

game.exe: $(OBJS) $(RAYLIB_LIB) $(SQLITE_O)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o game.exe

$(SQLITE_O): $(SQLITE_C)
	$(CC) $(SQLITE_CFLAGS) -c $(SQLITE_C) -o $(SQLITE_O)

src/%.o: src/%.c $(wildcard include/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

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
	-del src\*.o
	-del "$(SQLITE_DIR)\sqlite3.o"
	mingw32-make -C $(RAYLIB_SRC) clean
	-del "$(RAYLIB_STAMP)"

.PHONY: run clean FORCE
