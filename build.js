// Build the raylib game for the web.
//
// Steps:
//   1. Compile raylib (from the vendors/raylib submodule) for PLATFORM_WEB
//      into vendors/raylib/src/libraylib.web.a using its own Makefile.
//   2. Compile the game sources in src/ (with headers from include/) using
//      emcc, linking raylib and sqlite and wrapping everything in
//      src/shell.html, producing public/index.html + index.js + index.wasm.
//
// Requires Node.js and the Emscripten SDK (emcc) on PATH.

const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const ROOT = __dirname;
const RAYLIB_SRC = path.join("vendors", "raylib", "src");
const RAYLIB_WEB_LIB = path.join(RAYLIB_SRC, "libraylib.web.a");
const STAMP = path.join(RAYLIB_SRC, ".build-platform");
const SHELL = path.join("src", "shell.html");
const OUT = path.join("public", "index.html");
const SQLITE_DIR = path.join("vendors", "sqlite");
const SQLITE_C = path.join(SQLITE_DIR, "sqlite3.c");
const SRCS = [
  "src/main.c",
  "src/query.c",
  "src/ecs.c",
  "src/physics.c",
  "src/update_system.c",
];

function fail(msg) {
  console.error(`ERROR: ${msg}`);
  process.exit(1);
}

function childEnv() {
  // Note: on Windows the env block spells PATH as "Path"; spreading
  // process.env into a plain object loses the case-insensitive lookup.
  const env = { ...process.env };
  const pathKey = Object.keys(env).find((k) => k.toLowerCase() === "path");
  const pathValue = pathKey ? env[pathKey] : "";
  if (process.env.EMSDK) {
    const emccDir = path.join(process.env.EMSDK, "upstream", "emscripten");
    if (!pathValue.split(path.delimiter).includes(emccDir)) {
      if (pathKey && pathKey !== "PATH") delete env[pathKey];
      env.PATH = emccDir + path.delimiter + pathValue;
    }
  }
  return env;
}

function run(cmd, args, opts = {}) {
  const res = spawnSync(cmd, args, {
    stdio: "inherit",
    cwd: ROOT,
    env: childEnv(),
    ...opts,
  });
  if (res.status !== 0) {
    fail(`'${cmd} ${args.join(" ")}' failed (exit code ${res.status})`);
  }
}

// emcc resolves to emcc.bat on Windows and must run through a shell.
function emcc(args) {
  run("emcc", args, process.platform === "win32" ? { shell: true } : {});
}

function make() {
  return process.platform === "win32" ? "mingw32-make" : "make";
}

function hasEmcc() {
  const probe = spawnSync(process.platform === "win32" ? "where" : "which", ["emcc"], {
    stdio: "ignore",
    env: childEnv(),
  });
  if (probe.status === 0) return true;
  return process.env.EMSDK !== undefined;
}

function readStamp() {
  try {
    return fs.readFileSync(STAMP, "utf8").trim();
  } catch {
    return "";
  }
}

function buildRaylib(force) {
  const ready = fs.existsSync(RAYLIB_WEB_LIB) && readStamp() === "web" && !force;
  if (ready) {
    console.log("raylib web library already built (vendors/raylib/src/libraylib.web.a)");
    return;
  }
  console.log("Building raylib for the web (vendors/raylib/src/libraylib.web.a)...");
  run(make(), ["-C", RAYLIB_SRC, "clean"]);
  run(make(), ["-C", RAYLIB_SRC, "PLATFORM=PLATFORM_WEB"]);
  fs.writeFileSync(STAMP, "web");
}

function buildGame() {
  console.log("Compiling game to public/index.wasm...");
  fs.mkdirSync("public", { recursive: true });
  const args = [
    ...SRCS,
    "-o", OUT,
    "-DPLATFORM_WEB",
    "-DSQLITE_OMIT_LOAD_EXTENSION",
    "-Isrc",
    "-Iinclude",
    `-I${RAYLIB_SRC}`,
    `-I${SQLITE_DIR}`,
    SQLITE_C,
    RAYLIB_WEB_LIB,
    "-sUSE_GLFW=3",
    "-sFORCE_FILESYSTEM=1",
    "-sEXPORTED_RUNTIME_METHODS=ccall",
    "-sASYNCIFY",
    "-sMINIFY_HTML=0",
    "-sTOTAL_MEMORY=134217728",
    "-O2",
    "--shell-file", SHELL,
  ];
  emcc(args);
  console.log("Built public/index.html (+ index.js, index.wasm)");
}

const force = process.argv.includes("--clean") || process.argv.includes("--force");

if (!hasEmcc()) {
  fail(
    "emcc (Emscripten) was not found.\n" +
      "Install the Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html),\n" +
      "activate it (emsdk activate latest) and make sure 'emcc' is on your PATH,\n" +
      "or set the EMSDK environment variable to your emsdk directory."
  );
}

buildRaylib(force);
buildGame();
