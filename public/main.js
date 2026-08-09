// Canvas rendering + input; simulation lives in game.wasm.

const canvas = document.getElementById("game");
const ctx = canvas.getContext("2d");

const { instance } = await WebAssembly.instantiateStreaming(fetch("game.wasm"));
const game = instance.exports;

const plateW = game.getPlateW();
const plateH = game.getPlateH();
const size = game.getPlayerSize();
canvas.width = plateW;
canvas.height = plateH;

const keys = new Set();
const KEYMAP = {
  KeyW: "up", ArrowUp: "up",
  KeyS: "down", ArrowDown: "down",
  KeyA: "left", ArrowLeft: "left",
  KeyD: "right", ArrowRight: "right",
};
addEventListener("keydown", (e) => {
  const dir = KEYMAP[e.code];
  if (dir) { keys.add(dir); e.preventDefault(); }
});
addEventListener("keyup", (e) => {
  const dir = KEYMAP[e.code];
  if (dir) keys.delete(dir);
});
addEventListener("blur", () => keys.clear());

function drawBaseplate() {
  ctx.fillStyle = "#2b6e46";
  ctx.fillRect(0, 0, plateW, plateH);

  // Checkerboard tint so motion is visible
  const tile = 50;
  ctx.fillStyle = "rgba(255, 255, 255, 0.05)";
  for (let y = 0; y < plateH / tile; y++) {
    for (let x = 0; x < plateW / tile; x++) {
      if ((x + y) % 2 === 0) ctx.fillRect(x * tile, y * tile, tile, tile);
    }
  }

  ctx.strokeStyle = "rgba(0, 0, 0, 0.35)";
  ctx.lineWidth = 4;
  ctx.strokeRect(2, 2, plateW - 4, plateH - 4);
}

function drawPlayer(x, y) {
  ctx.fillStyle = "rgba(0, 0, 0, 0.25)";
  ctx.fillRect(x + 4, y + 6, size, size);

  ctx.fillStyle = "#e4b330";
  ctx.fillRect(x, y, size, size);
  ctx.strokeStyle = "#8a6a15";
  ctx.lineWidth = 3;
  ctx.strokeRect(x + 1.5, y + 1.5, size - 3, size - 3);
}

let last = performance.now();
function frame(now) {
  // Clamp dt so a background tab doesn't teleport the player
  const dt = Math.min((now - last) / 1000, 0.05);
  last = now;

  const inx = (keys.has("right") ? 1 : 0) - (keys.has("left") ? 1 : 0);
  const iny = (keys.has("down") ? 1 : 0) - (keys.has("up") ? 1 : 0);
  game.update(dt, inx, iny);

  drawBaseplate();
  drawPlayer(game.getX(), game.getY());

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
