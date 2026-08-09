CC      := gcc
RAYLIB  := vendors/raylib/src/libraylib.a
CFLAGS  := -std=c99 -Wall -Wextra -O2 -Ivendors/raylib/src
LIBS    := $(RAYLIB) -lopengl32 -lgdi32 -lwinmm -lm

game.exe: src/main.c $(RAYLIB)
	$(CC) $(CFLAGS) src/main.c $(LIBS) -o game.exe

$(RAYLIB):
	mingw32-make -C vendors/raylib/src

run: game.exe
	./game.exe

clean:
	-del game.exe
	mingw32-make -C vendors/raylib/src clean

.PHONY: run clean
