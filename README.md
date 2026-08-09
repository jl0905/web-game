# WASM Canvas Game

A minimal WebAssembly + Canvas game: a player-controlled square moving on a
rectangular baseplate. All simulation (input acceleration, friction, speed
clamping, wall collision) runs in WebAssembly; JavaScript only handles
keyboard input and Canvas rendering.

## Layout

- `src/game.wat` — game logic in WebAssembly text format
- `build.js` — compiles the WAT to `public/game.wasm` via [wabt](https://www.npmjs.com/package/wabt)
- `public/` — static site (`index.html`, `main.js`, `game.wasm`)
- `server.js` — tiny static server with the correct `application/wasm` MIME type

## Run

```sh
npm install
npm run dev   # build + serve
```

Then open http://localhost:8080 and move with WASD or arrow keys.

`npm run build` recompiles the wasm after editing `src/game.wat`;
`npm start` serves without rebuilding.
