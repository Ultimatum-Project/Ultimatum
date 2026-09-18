"""Isolated loopback-only runtime fixture server; normal LAN output is untouched."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=4174)
parser.add_argument("--engine-dir", type=Path)
parser.add_argument("--cloud-fixture", type=Path)
parser.add_argument("--cloud-adventure", type=Path)
args = parser.parse_args()

web = Path(__file__).resolve().parents[1]
client = web / ".cache/local-dist" if (web / ".cache/local-dist/index.html").is_file() else web / "dist"

class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Fixtures rebuild in place; never mix a cached Wasm binary with new JS.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def translate_path(self, path):
        name = path.split("?", 1)[0]
        if name == "/cloud-adventure.u4save" and args.cloud_adventure:
            return str(args.cloud_adventure)
        if name == "/cloud-fixture.json" and args.cloud_fixture:
            return str(args.cloud_fixture)
        if name == "/cloud-runtime-driver.js":
            return str(web / "tests/cloud-runtime-driver.js")
        if name == "/feedback-runtime-driver.js":
            return str(web / "tests/feedback-runtime-driver.js")
        if name == "/cloud-game-data-runtime.html":
            return str(web / "tests/cloud-game-data-runtime.html")
        if name.startswith("/engine/"):
            return str((args.engine_dir or web / ".cache/runtime/engine") / Path(name).name)
        if name == "/runtime-driver.js":
            return str(web / "tests/runtime-driver.js")
        if name == "/menu-runtime-driver.js":
            return str(web / "tests/menu-runtime-driver.js")
        if name == "/walk-runtime-driver.js":
            return str(web / "tests/walk-runtime-driver.js")
        if name in ("/hud-runtime-driver.js", "/parity-runtime-driver.js", "/lifecycle-runtime-driver.js", "/responsive-runtime.html", "/onboarding-runtime-driver.js", "/title-runtime-bootstrap.js", "/desktop-runtime-driver.js", "/audio-runtime-driver.js", "/account-race-runtime-driver.js"):
            return str(web / "tests" / Path(name).name)
        if name == "/journal-runtime-driver.js":
            return str(web / "tests/journal-runtime-driver.js")
        return super().translate_path(path)

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/":
            driver = "account-race-runtime-driver.js" if "account_race_suite" in self.path else "feedback-runtime-driver.js" if "feedback_suite" in self.path else "cloud-runtime-driver.js" if "cloud_suite" in self.path else "journal-runtime-driver.js" if "journal_suite" in self.path else "audio-runtime-driver.js" if "audio_suite" in self.path else "desktop-runtime-driver.js" if "desktop_suite" in self.path else "onboarding-runtime-driver.js" if "onboarding_suite" in self.path else "lifecycle-runtime-driver.js" if "lifecycle_suite" in self.path else "parity-runtime-driver.js" if "parity_suite" in self.path else "hud-runtime-driver.js" if "hud_suite" in self.path else "walk-runtime-driver.js" if "walk_suite" in self.path else "menu-runtime-driver.js" if "menu_suite" in self.path else "runtime-driver.js"
            bootstrap = '' if "feedback_suite" in self.path or "cloud_suite" in self.path or "onboarding_suite" in self.path or "desktop_suite" in self.path or "account_race_suite" in self.path else '<script src="/title-runtime-bootstrap.js"></script>'
            data = (client / "index.html").read_text(encoding="utf-8").replace("</body>", bootstrap + '<script src="/' + driver + '"></script></body>').encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            super().do_GET()

def handler(*args, **kwargs):
    return Handler(*args, directory=str(client), **kwargs)

print(f"Isolated runtime tests: http://127.0.0.1:{args.port}/?fixture=3", flush=True)
ThreadingHTTPServer(("127.0.0.1", args.port), handler).serve_forever()
