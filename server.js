// Minimal static server for public/ with correct wasm MIME type.
const http = require("http");
const fs = require("fs");
const path = require("path");

const ROOT = path.join(__dirname, "public");
const PORT = process.env.PORT || 8080;

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".wasm": "application/wasm",
  ".css": "text/css; charset=utf-8",
};

http.createServer((req, res) => {
  const urlPath = decodeURIComponent(new URL(req.url, "http://localhost").pathname);
  const filePath = path.join(ROOT, urlPath === "/" ? "index.html" : urlPath);

  if (!filePath.startsWith(ROOT)) {
    res.writeHead(403).end("Forbidden");
    return;
  }

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404).end("Not found");
      return;
    }
    res.writeHead(200, {
      "Content-Type": MIME[path.extname(filePath)] || "application/octet-stream",
    });
    res.end(data);
  });
}).listen(PORT, () => {
  console.log(`Serving http://localhost:${PORT}`);
});
