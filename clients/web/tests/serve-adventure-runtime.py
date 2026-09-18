"""Loopback-only native/web package runtime test. Artifacts stay in a test dir."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=4211)
parser.add_argument("--fixture-dir", type=Path, required=True)
args = parser.parse_args()
web = Path(__file__).resolve().parents[1]

class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def translate_path(self, path):
        name = path.split("?", 1)[0]
        if name.startswith("/engine/"):
            return str(web / ".cache/runtime/engine" / Path(name).name)
        if name == "/native.u4save":
            return str(args.fixture_dir / "world.u4save")
        if name == "/adventure-package-runtime-driver.js":
            return str(web / "tests/adventure-package-runtime-driver.js")
        return super().translate_path(path)

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/":
            data = (web / "dist/index.html").read_text().replace(
                "</body>", '<script src="/adventure-package-runtime-driver.js"></script></body>').encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            super().do_GET()

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        if self.path != "/browser.u4save" or not 0 < length <= 16 * 1024 * 1024:
            self.send_error(400)
            return
        (args.fixture_dir / "browser.u4save").write_bytes(self.rfile.read(length))
        self.send_response(204)
        self.end_headers()

def handler(*a, **kw):
    return Handler(*a, directory=str(web / "dist"), **kw)

print(f"Adventure package runtime: http://127.0.0.1:{args.port}/?fixture=3", flush=True)
ThreadingHTTPServer(("127.0.0.1", args.port), handler).serve_forever()
