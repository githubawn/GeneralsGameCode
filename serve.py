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



# The web build needs the full Zero Hour archive set (~1.2 GB: INI, textures, W3D
# models, maps, audio/speech/music, etc.) — only the ZH_Generals base-Generals subfolder
# is excluded (see below). All ZH *.big files are served as-is.

class VirtualGeneralsServer(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Enable COOP/COEP headers to support WebAssembly threads / SharedArrayBuffer if needed
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_GET(self):
        # Serve the JSON list of all available .big files
        if self.path == '/list-big-files':
            big_files = []
            
            # Scan Zero Hour folder — serve every *.big archive.
            if os.path.exists(STEAM_ZH_DIR):
                for f in os.listdir(STEAM_ZH_DIR):
                    if f.lower().endswith('.big'):
                        path = os.path.join(STEAM_ZH_DIR, f)
                        big_files.append({"name": f, "source": "zh", "size": os.path.getsize(path)})

            # Scan Generals base folder (ZH_Generals). Skipped for the WASM build — Zero
            # Hour ships self-contained archives and the base Generals assets aren't needed
            # to boot the shell/menu, so excluding them saves ~hundreds of MB of MEMFS.
            # Set GGC_WEB_ALL_BIGS=1 to include them.
            if os.environ.get("GGC_WEB_ALL_BIGS") == "1" and os.path.exists(STEAM_GEN_DIR):
                for f in os.listdir(STEAM_GEN_DIR):
                    if f.lower().endswith('.big'):
                        path = os.path.join(STEAM_GEN_DIR, f)
                        big_files.append({"name": f, "source": "gen", "size": os.path.getsize(path)})

            response_data = json.dumps(big_files).encode('utf-8')
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
            size = os.path.getsize(file_path)

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

            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', size)
            self.end_headers()
            if self.command == 'HEAD':
                return
            with open(file_path, 'rb') as f:
                shutil.copyfileobj(f, self.wfile, length=1024 * 1024)
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
