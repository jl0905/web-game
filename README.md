# Raylib Square Game

A C + raylib game: a player-controlled square moving on a rectangular
baseplate. All simulation (input acceleration, friction, speed clamping, wall
collision) is written in C, with game state (entities and their position,
velocity, and square components) stored in an in-memory SQLite database.

## Layout

- `src/main.c` — game loop, input, rendering
- `src/query.c`, `src/ecs.c`, `src/physics.c`, `src/update_system.c` — engine modules
- `include/` — public headers for the engine modules
- `vendors/raylib` — raylib as a git submodule
- `vendors/sqlite` — SQLite amalgamation (`sqlite3.c`) as a git submodule
- `Makefile` — builds raylib, SQLite, and the game

## Setup

Dependencies are pinned as git submodules. On a fresh clone:

```sh
git submodule update --init
```

## Build

Requires a MinGW toolchain (`gcc`, `mingw32-make`).

```sh
mingw32-make      # build game.exe
mingw32-make run  # build + run
```

The first build compiles raylib from `vendors/raylib/src`, which takes a
minute; subsequent builds only recompile the game.

Move with WASD or arrow keys.
