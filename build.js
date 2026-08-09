// Compiles src/game.wat -> public/game.wasm using wabt.
const fs = require("fs");
const path = require("path");

async function main() {
  const wabt = await require("wabt")();
  const watPath = path.join(__dirname, "src", "game.wat");
  const outDir = path.join(__dirname, "public");
  const source = fs.readFileSync(watPath, "utf8");

  const mod = wabt.parseWat("game.wat", source);
  mod.resolveNames();
  mod.validate();
  const { buffer } = mod.toBinary({});
  mod.destroy();

  fs.mkdirSync(outDir, { recursive: true });
  fs.writeFileSync(path.join(outDir, "game.wasm"), Buffer.from(buffer));
  console.log(`Wrote public/game.wasm (${buffer.length} bytes)`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
