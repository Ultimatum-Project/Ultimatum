"""Loopback-only public release QA. Private fixtures are never build assets."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from io import BytesIO
import json
import zipfile
import fnmatch
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--port",type=int,default=4181)
parser.add_argument("--audit-zip",type=Path,help="Explicit private ZIP to test; served only by this loopback harness")
args = parser.parse_args()

site = Path(__file__).resolve().parents[1]
web = site.parent / "web"
game = web / ".cache/game-data"
manifest_text = (web / "dist/game-data-manifest.js").read_text()
manifest = json.loads(manifest_text.split("const manifest = ",1)[1].split(";\n",1)[0])

def fixture(kind):
    result = BytesIO()
    with zipfile.ZipFile(result,"w",zipfile.ZIP_DEFLATED) as archive:
        if kind == "too-many":
            for i in range(2049): archive.writestr(f"note-{i}.txt",b"x")
        else:
            for name in manifest["files"]:
                if kind == "missing" and name == "WORLD.MAP": continue
                data = (game / name).read_bytes()
                if kind == "wrong-version" and name == "AVATAR.EXE": data = bytes([data[0] ^ 1]) + data[1:]
                if kind == "too-large" and name == "WORLD.MAP": data = b"x" * (2 * 1024 * 1024 + 1)
                directory = ("part-a/" if list(manifest["files"]).index(name)%2 else "part-b/") if kind == "split" else "u4/"
                archive.writestr(directory+name,data)
                if kind == "ambiguous": archive.writestr("second-installation/"+name,data)
            archive.writestr("readme.txt",b"Unrelated documentation should not be installed.")
            archive.writestr("PARTY.SAV",b"Embedded saves should not be installed.")
            if kind == "unsafe": archive.writestr("../WORLD.MAP",b"unsafe")
            if kind == "duplicate": archive.writestr("u4/./world.map",(game / "WORLD.MAP").read_bytes())
    data = result.getvalue()
    if kind == "corrupt":
        # Damage compressed bytes without changing the central directory CRC.
        changed = bytearray(data); changed[60] ^= 255; data = bytes(changed)
    return data

class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        pattern = None
        for line in (site / "build/_headers").read_text().splitlines():
            if not line.startswith(" "): pattern = line
            elif pattern and fnmatch.fnmatch(self.path.split("?",1)[0],pattern):
                key,value=line.strip().split(":",1)
                if key == "Content-Security-Policy" and self.path.startswith("/runtime/"):
                    # Only the isolated QA harness may embed the app at exact
                    # dimensions; production retains frame-ancestors 'none'.
                    value=value.replace("frame-ancestors 'none'","frame-ancestors 'self'")
                self.send_header(key,value.strip())
        super().end_headers()

    def send_bytes(self,data,kind):
        self.send_response(200)
        self.send_header("Content-Type",kind)
        self.send_header("Content-Length",str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        name=self.path.split("?",1)[0]
        if name in ("/runtime/responsive.html","/runtime/responsive.js"):
            self.send_bytes((site / "tests" / Path(name).name).read_bytes(),"text/html" if name.endswith("html") else "text/javascript")
        elif name == "/runtime/home.html":
            text=(site / "build/index.html").read_text().replace("<head>",'<head><base href="/">')
            self.send_bytes(text.encode(),"text/html; charset=utf-8")
        elif name == "/runtime/overlay.zip":
            self.send_bytes((web / ".cache/assets/u4upgrad-1.3.zip").read_bytes(),"application/zip")
        elif name == "/runtime/bad-overlay.zip":
            self.send_bytes(b"not a VGA upgrade","application/zip")
        elif name == "/runtime/audit.zip" and args.audit_zip:
            self.send_bytes(args.audit_zip.read_bytes(),"application/zip")
        elif name.startswith("/runtime/fixtures/"):
            self.send_bytes(fixture(Path(name).stem),"application/zip")
        elif name == "/runtime/driver.js":
            self.send_bytes((site / "tests/data-runtime-driver.js").read_bytes(),"text/javascript")
        elif name == "/runtime/audio-safety-bootstrap.js":
            self.send_bytes((site / "tests/audio-safety-bootstrap.js").read_bytes(),"text/javascript")
        elif name == "/runtime/index.html":
            text=(site / "build/play/index.html").read_text().replace("<head>",'<head><base href="/play/"><script src="/runtime/audio-safety-bootstrap.js"></script>').replace("</body>",'<script src="/runtime/driver.js"></script></body>')
            self.send_bytes(text.encode(),"text/html; charset=utf-8")
        else: super().do_GET()

def handler(*args,**kwargs):
    return Handler(*args,directory=str(site / "build"),**kwargs)

print(f"Public release runtime QA: http://127.0.0.1:{args.port}/runtime/index.html",flush=True)
ThreadingHTTPServer(("127.0.0.1",args.port),handler).serve_forever()
