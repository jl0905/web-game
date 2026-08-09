# Raylib Square Game

A C + raylib game: a player-controlled square moving on a rectangular
baseplate. All simulation (input acceleration, friction, speed clamping, wall
collision) is written in C using raylib for input, windowing, and rendering.

## Layout

- `src/main.c` — game logic and rendering
- `vendors/raylib` — raylib as a git submodule
- `Makefile` — builds raylib (via `mingw32-make`) and the game

## Setup

raylib is pinned as a git submodule. On a fresh clone:

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
