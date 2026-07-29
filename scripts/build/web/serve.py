import os
import json
import shutil
import http.server
import socketserver
import configparser

PORT = 8000
# The Emscripten z_generals target writes to build/<preset>/web/<CONFIG> because the
# Ninja Multi-Config generator appends the config name. Prefer Release, fall back to Debug.
_WEB_ROOT = os.path.abspath(r"build/web-generalsmd-sdl3-bgfx/web")
def _pick_build_dir():
    for cfg in ("Release", "RelWithDebInfo", "Debug"):
        d = os.path.join(_WEB_ROOT, cfg)
        if os.path.exists(os.path.join(d, "index.html")):
            return d
    return _WEB_ROOT
BUILD_DIR = _pick_build_dir()

# Read configuration from serve.ini
_CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "serve.ini")
_config = configparser.ConfigParser()
STEAM_ZH_DIR = ""

if os.path.exists(_CONFIG_FILE):
    try:
        _config.read(_CONFIG_FILE)
        if "Paths" in _config and "STEAM_ZH_DIR" in _config["Paths"]:
            STEAM_ZH_DIR = _config["Paths"]["STEAM_ZH_DIR"]
    except Exception as e:
        print(f"Warning: Failed to read config from {_CONFIG_FILE}: {e}")

# Fallback to default Steam install location if not configured in serve.ini
if not STEAM_ZH_DIR:
    STEAM_ZH_DIR = r"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour"
    if not os.path.exists(STEAM_ZH_DIR):
        print(f"Note: STEAM_ZH_DIR not configured in {_CONFIG_FILE}")
        print(f"Using default path: {STEAM_ZH_DIR}")
        print(f"If your installation is elsewhere, please specify it in {_CONFIG_FILE}")

STEAM_GEN_DIR = os.path.join(STEAM_ZH_DIR, "ZH_Generals")

# The game also reads LOOSE files from the install's Data\ folder (these override the
# .big archives): Data\WaterPlane\*.tga (water), Data\Cursors\*.ani, Data\INI\*.ini,
# Data\Scripts\*.scb, etc. We serve that tree too, but skip Movies\ (*.bik — no video
# backend on web) to save bandwidth/MEMFS.
STEAM_DATA_DIR = os.path.join(STEAM_ZH_DIR, "Data")
_DATA_SKIP_DIRS = {"movies"}

def list_data_files():
    """Loose Data\\ files as posix-relative paths (e.g. 'Data/WaterPlane/foo.tga')."""
    out = []
    if not os.path.isdir(STEAM_DATA_DIR):
        return out
    for root, dirs, files in os.walk(STEAM_DATA_DIR):
        dirs[:] = [d for d in dirs if d.lower() not in _DATA_SKIP_DIRS]
        for fn in files:
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, STEAM_ZH_DIR).replace("\\", "/")
            out.append({"path": rel, "size": os.path.getsize(full)})
    return out



# The web build needs the full Zero Hour archive set (~1.2 GB: INI, textures, W3D
# models, maps, audio/speech/music, etc.) — only the ZH_Generals base-Generals subfolder
# is excluded (see below). All ZH *.big files are served as-is.

class VirtualGeneralsServer(http.server.SimpleHTTPRequestHandler):
    # HTTP/1.1 keeps the TCP connection alive between requests so the ~36 .big +
    # loose-Data downloads (fetched 6-at-a-time by the shell) reuse connections instead
    # of paying a fresh TCP/handshake per file. Requires Content-Length on every reply
    # (we always send it), which is also what enables the browser to cache + revalidate.
    protocol_version = "HTTP/1.1"

    def end_headers(self):
        # Enable COOP/COEP headers to support WebAssembly threads / SharedArrayBuffer if needed
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_GET(self):
        # Serve the JSON list of available .big files. The native engine's
        # loadBigFilesFromDirectory("", "*.big") RECURSES into subdirectories, loading the
        # top-level ZH bigs AND ZH_Generals\*.big (36 archives on a Steam install). We
        # replicate that exactly: walk the whole install tree and return each *.big by its
        # path RELATIVE to the install root (posix slashes). The shell writes each to the
        # same relative path in MEMFS so the engine's recursive scan finds them. Nothing is
        # skipped — the game depends on all of them.
        if self.path == '/list-big-files':
            big_files = []
            for root, dirs, files in os.walk(STEAM_ZH_DIR):
                for f in files:
                    if not f.lower().endswith('.big'):
                        continue
                    full = os.path.join(root, f)
                    rel = os.path.relpath(full, STEAM_ZH_DIR).replace("\\", "/")
                    big_files.append({"name": rel, "size": os.path.getsize(full)})

            response_data = json.dumps(big_files).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', len(response_data))
            self.end_headers()
            self.wfile.write(response_data)
            return

        # Serve the JSON list of loose Data\ files (water/cursors/INI/scripts), so the
        # shell can stream them into MEMFS at the same relative path the engine reads.
        if self.path == '/list-data-files':
            response_data = json.dumps(list_data_files()).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', len(response_data))
            self.end_headers()
            self.wfile.write(response_data)
            return

        # Clean the path to get the filename
        filename = self.path.lstrip('/')
        # Remove any query parameters
        if '?' in filename:
            filename = filename.split('?')[0]

        # Loose Data\ files (e.g. "Data/WaterPlane/foo.tga") served from the ZH install.
        if filename.lower().startswith('data/'):
            data_path = os.path.join(STEAM_ZH_DIR, filename.replace('/', os.sep))
            if os.path.exists(data_path) and not os.path.isdir(data_path):
                self.serve_file_from_path(data_path)
                return

        # Check if the requested file is a .big file
        if filename.lower().endswith('.big'):
            # 1. Try to find in Zero Hour directory
            zh_path = os.path.join(STEAM_ZH_DIR, filename)
            if os.path.exists(zh_path):
                self.serve_file_from_path(zh_path)
                return

            # 2. Try to find in Generals directory
            gen_path = os.path.join(STEAM_GEN_DIR, filename)
            if os.path.exists(gen_path):
                self.serve_file_from_path(gen_path)
                return

        # Default: Serve from Build/Release folder
        local_build_path = os.path.join(BUILD_DIR, filename if filename else "index.html")
        if os.path.exists(local_build_path) and not os.path.isdir(local_build_path):
            self.serve_file_from_path(local_build_path)
        else:
            super().do_GET()

    def serve_file_from_path(self, file_path):
        # Stream the file to avoid loading multi-hundred-MB .big archives fully into RAM.
        try:
            st = os.stat(file_path)
            size = st.st_size

            # Determine content type
            content_type = 'application/octet-stream'
            if file_path.endswith('.html'):
                content_type = 'text/html'
            elif file_path.endswith('.js'):
                content_type = 'application/javascript'
            elif file_path.endswith('.wasm'):
                content_type = 'application/wasm'
            elif file_path.endswith('.css'):
                content_type = 'text/css'

            # Conditional GET: an ETag derived from size+mtime lets the browser (and the
            # shell's Cache Storage layer) revalidate cheaply. If the client already holds
            # the current version, reply 304 with no body instead of re-sending the bytes.
            etag = '"%x-%x"' % (int(st.st_mtime), size)
            last_modified = self.date_time_string(int(st.st_mtime))
            if self.headers.get('If-None-Match') == etag:
                self.send_response(304)
                self.send_header('ETag', etag)
                self.send_header('Cache-Control', 'no-cache')
                self.send_header('Content-Length', 0)
                self.end_headers()
                return

            # Honor a single byte-range (resumable / chunked downloads). Anything we can't
            # parse falls back to a normal full 200 response.
            start, length = 0, size
            range_hdr = self.headers.get('Range')
            is_range = False
            if range_hdr and range_hdr.startswith('bytes=') and ',' not in range_hdr:
                try:
                    spec = range_hdr[len('bytes='):].strip()
                    s, _, e = spec.partition('-')
                    if s == '':            # suffix range: bytes=-N (last N bytes)
                        n = int(e)
                        start = max(0, size - n)
                        length = size - start
                    else:
                        start = int(s)
                        end = int(e) if e else size - 1
                        end = min(end, size - 1)
                        if start <= end:
                            length = end - start + 1
                            is_range = True
                except (ValueError, TypeError):
                    start, length, is_range = 0, size, False

            if is_range and start < size:
                self.send_response(206)
                self.send_header('Content-Range', 'bytes %d-%d/%d' % (start, start + length - 1, size))
            else:
                self.send_response(200)
                start, length = 0, size
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', length)
            self.send_header('ETag', etag)
            self.send_header('Last-Modified', last_modified)
            # no-cache = the browser may store the response but must revalidate (cheap 304)
            # before reuse, so a rebuilt asset is never served stale.
            self.send_header('Cache-Control', 'no-cache')
            # Lets the browser issue Range requests (resumable / chunked downloads).
            self.send_header('Accept-Ranges', 'bytes')
            self.end_headers()
            if self.command == 'HEAD':
                return
            with open(file_path, 'rb') as f:
                if start:
                    f.seek(start)
                remaining = length
                chunk = 1024 * 1024
                while remaining > 0:
                    data = f.read(min(chunk, remaining))
                    if not data:
                        break
                    self.wfile.write(data)
                    remaining -= len(data)
        except (BrokenPipeError, ConnectionResetError):
            # Browser cancelled the download (e.g. navigated away) — not an error.
            pass
        except Exception as e:
            self.send_error(500, f"Error reading file: {e}")

# Threaded so multiple clients (and multiple parallel .big downloads) don't block each
# other while streaming hundreds of MB. Allow quick port reuse on restart.
class ThreadingHTTPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True

with ThreadingHTTPServer(("", PORT), VirtualGeneralsServer) as httpd:
    print(f"============================================================")
    print(f"C&C Generals Zero Hour Web Port Server Active")
    print(f"Serving Launcher from: {BUILD_DIR}")
    print(f"Serving Zero Hour Assets from: {STEAM_ZH_DIR}")
    print(f"Serving Generals Assets from: {STEAM_GEN_DIR}")
    print(f"============================================================")
    print(f"URL: http://localhost:{PORT}")
    print(f"============================================================")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
