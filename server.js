// Tiny static server for the built game (public/).
// Serves .wasm with the correct application/wasm MIME type.

const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = process.env.PORT || 8080;
const ROOT = path.join(__dirname, "public");

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".wasm": "application/wasm",
  ".css": "text/css; charset=utf-8",
  ".png": "image/png",
  ".ico": "image/x-icon",
  ".json": "application/json; charset=utf-8",
};

http
  .createServer((req, res) => {
    let pathname;
    try {
      pathname = decodeURIComponent(req.url.split("?")[0]);
    } catch {
      res.writeHead(400);
      return res.end("Bad request");
    }
    if (pathname === "/") pathname = "/index.html";

    const file = path.join(ROOT, pathname);
    if (!file.startsWith(ROOT)) {
      res.writeHead(403);
      return res.end("Forbidden");
    }

    fs.readFile(file, (err, data) => {
      if (err) {
        res.writeHead(404);
        return res.end("Not found");
      }
      res.writeHead(200, { "Content-Type": MIME[path.extname(file)] || "application/octet-stream" });
      res.end(data);
    });
  })
  .listen(PORT, () => console.log(`Serving http://localhost:${PORT}`));
