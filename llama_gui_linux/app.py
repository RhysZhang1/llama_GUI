#!/usr/bin/env python3
"""
llama_gui_wsl - Web GUI for llama.cpp
Zero external dependencies (pure Python stdlib).
Works on WSL, pure Linux, macOS.

Run:
  cd ~/llama_gui_wsl && python3 app.py

Before first run, seed models from existing .gguf files:
  python3 app.py --seed
"""

import os
import sys
import json
import time
import signal
import shlex
import subprocess
import webbrowser
import socket
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from threading import Thread
from urllib.parse import urlparse, parse_qs, unquote

# ── Paths ──────────────────────────────────────────────────
BASE_DIR         = Path(__file__).resolve().parent
CONFIG_DIR       = BASE_DIR / "config"
CONFIG_FILE      = CONFIG_DIR / "config.json"
MODELS_FILE      = CONFIG_DIR / "models.json"
LLAMA_BUILD_DIR  = Path.home() / "llama.cpp" / "build" / "bin"
LLAMA_SRC_DIR    = Path.home() / "llama.cpp"
MODELS_DIR       = Path.home() / "models"
UI_DIST_DIR      = Path.home() / "llama.cpp" / "tools" / "ui" / "dist"
TEMPLATE_FILE    = BASE_DIR / "templates" / "index.html"

# ── Default config ─────────────────────────────────────────
DEFAULT_CONFIG = {
    "run": {
        "mode":           "cli",
        "model":          "",
        "ctx_size":       65536,
        "ngl":            48,
        "temp":           0.7,
        "repeat_penalty": 1.1,
        "max_tokens":     2048,
        "port":           8081,
        "mli":            True,
        "mtp":            False,
        "extra_enabled":  False,
        "extra_params":   "",
        "use_llama_path": False,
        "llama_bin_path": str(LLAMA_BUILD_DIR),
    },
    "convert": {
        "llama_path":   str(LLAMA_SRC_DIR),
        "model_src":    "",
        "lora_src":     "",
        "model_dst":    "",
        "outtype":      "f16",
    },
    "server": {
        "running": False,
        "pid":     None,
    }
}

# ── Helpers ────────────────────────────────────────────────
def _load_json(path, default):
    try:
        with open(path) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return default

def _save_json(path, data):
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

def _server_pid_file():
    return BASE_DIR / ".server.pid"

def _server_log_path():
    return BASE_DIR / ".server.log"

def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return None

# ── JSON response helper ───────────────────────────────────
def _json_resp(handler, data, code=200):
    body = json.dumps(data, ensure_ascii=False).encode("utf-8")
    handler.send_response(code)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", len(body))
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.end_headers()
    handler.wfile.write(body)

def _read_body(handler):
    length = int(handler.headers.get("Content-Length", 0))
    if length == 0:
        return {}
    return json.loads(handler.rfile.read(length))

# ── Request Handler ────────────────────────────────────────
class LlamaGUIHandler(BaseHTTPRequestHandler):

    def log_message(self, format, *args):
        # Quiet logging
        pass

    def do_GET(self):
        parsed = urlparse(self.path)
        path = unquote(parsed.path)
        qs = parse_qs(parsed.query)

        # ── Static /api routes ──────────────────────────
        if path == "/api/models":
            models = _load_json(MODELS_FILE, [])
            return _json_resp(self, models)

        if path == "/api/config":
            cfg = _load_json(CONFIG_FILE, DEFAULT_CONFIG)
            # Resolve live server status
            pid_file = _server_pid_file()
            cfg["server"]["running"] = False
            cfg["server"]["pid"] = None
            if pid_file.exists():
                try:
                    old_pid = int(pid_file.read_text().strip())
                    os.kill(old_pid, 0)
                    cfg["server"]["running"] = True
                    cfg["server"]["pid"] = old_pid
                except (ValueError, ProcessLookupError, OSError):
                    pid_file.unlink(missing_ok=True)
            return _json_resp(self, cfg)

        if path == "/api/server/status":
            pid_file = _server_pid_file()
            running = False
            pid = None
            if pid_file.exists():
                try:
                    pid = int(pid_file.read_text().strip())
                    os.kill(pid, 0)
                    running = True
                except (ValueError, ProcessLookupError, OSError):
                    pid_file.unlink(missing_ok=True)

            logs = ""
            log_path = _server_log_path()
            if log_path.exists():
                try:
                    with open(log_path) as f:
                        f.seek(0, 2)
                        size = f.tell()
                        f.seek(max(0, size - 10240))
                        logs = f.read()
                except OSError:
                    logs = "[Error reading log file]"
            return _json_resp(self, {"running": running, "pid": pid, "logs": logs})

        if path == "/api/browse":
            base = qs.get("path", [str(Path.home())])[0]
            ext  = qs.get("ext", [""])[0]
            try:
                p = Path(base).resolve()
                if not p.exists():
                    return _json_resp(self, {"error": f"Path does not exist: {p}"}, 404)
                if p.is_dir():
                    items = []
                    for child in sorted(p.iterdir()):
                        try:
                            items.append({
                                "name": child.name,
                                "path": str(child),
                                "is_dir": child.is_dir(),
                                "size": child.stat().st_size if child.is_file() else 0,
                            })
                        except PermissionError:
                            continue
                    if ext:
                        items = [i for i in items if i["is_dir"] or i["name"].endswith(ext)]
                    parent = str(p.parent) if p != Path("/") else None
                    return _json_resp(self, {"path": str(p), "parent": parent, "items": items})
                else:
                    return _json_resp(self, {"path": str(p), "is_file": True})
            except Exception as e:
                return _json_resp(self, {"error": str(e)}, 500)

        # ── Fallback: serve index.html for / ─────────────
        html = _read_file(TEMPLATE_FILE)
        if html is None:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(b"index.html not found")
            return
        body = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = unquote(parsed.path)

        if path == "/api/models":
            data = _read_body(self)
            name = data.get("name", "").strip()
            mp = data.get("path", "").strip()
            if not name or not mp:
                return _json_resp(self, {"error": "Name and path are required."}, 400)
            models = _load_json(MODELS_FILE, [])
            for m in models:
                if m["name"] == name:
                    m["path"] = mp
                    break
            else:
                models.append({"name": name, "path": mp})
            _save_json(MODELS_FILE, models)
            return _json_resp(self, {"ok": True, "models": models})

        if path == "/api/config":
            data = _read_body(self)
            _save_json(CONFIG_FILE, data)
            return _json_resp(self, {"ok": True})

        if path == "/api/generate-command":
            return self._cmd_generate()

        if path == "/api/server/start":
            return self._server_start()

        if path == "/api/server/stop":
            return self._server_stop()

        if path == "/api/convert/model":
            return self._convert("model")

        if path == "/api/convert/lora":
            return self._convert("lora")

        if path == "/api/open-terminal":
            return self._open_terminal()

        # 404
        self.send_response(404)
        self.end_headers()

    def do_DELETE(self):
        parsed = urlparse(self.path)
        path = unquote(parsed.path)
        prefix = "/api/models/"
        if path.startswith(prefix):
            name = path[len(prefix):]
            models = _load_json(MODELS_FILE, [])
            models = [m for m in models if m["name"] != name]
            _save_json(MODELS_FILE, models)
            return _json_resp(self, {"ok": True, "models": models})

        self.send_response(404)
        self.end_headers()

    # ── Command generation ────────────────────────────────
    def _cmd_generate(self):
        cfg = _read_body(self)
        mode           = cfg.get("mode", "cli")
        model_name     = cfg.get("model", "")
        ctx_size       = cfg.get("ctx_size", 65536)
        ngl            = cfg.get("ngl", 48)
        temp           = cfg.get("temp", 0.7)
        repeat_penalty = cfg.get("repeat_penalty", 1.1)
        max_tokens     = cfg.get("max_tokens", 2048)
        port           = cfg.get("port", 8081)
        mli            = cfg.get("mli", False)
        mtp            = cfg.get("mtp", False)
        extra_enabled  = cfg.get("extra_enabled", False)
        extra_params   = cfg.get("extra_params", "")
        use_llama_path = cfg.get("use_llama_path", False)
        llama_bin_path = cfg.get("llama_bin_path", str(LLAMA_BUILD_DIR))

        # Resolve model path
        models = _load_json(MODELS_FILE, [])
        model_path = model_name
        for m in models:
            if m["name"] == model_name:
                model_path = m["path"]
                break

        if use_llama_path and llama_bin_path:
            bin_dir = Path(llama_bin_path)
        else:
            bin_dir = LLAMA_BUILD_DIR

        if mode == "server":
            exe = bin_dir / "llama-server"
            cmd = (
                f'{shlex.quote(str(exe))} -m {shlex.quote(model_path)} '
                f'--ctx-size {ctx_size} -ngl {ngl} --flash-attn on --port {port}'
                f' --path {shlex.quote(str(UI_DIST_DIR))}'
            )
        else:
            exe = bin_dir / "llama-cli"
            parts = [
                shlex.quote(str(exe)),
                f'-m {shlex.quote(model_path)}',
                '-cnv', '--color', 'auto',
                f'-ngl {ngl}',
                f'--ctx-size {ctx_size}',
                '--flash-attn on',
                f'--temp {temp}',
                f'--repeat-penalty {repeat_penalty}',
                f'-n {max_tokens}',
            ]
            if mli:
                parts.append('-mli')
            if mtp:
                parts.append('--spec-type draft-mtp --spec-draft-n-max 4')
            if extra_enabled and extra_params:
                parts.append(extra_params)
            cmd = " ".join(parts)

        return _json_resp(self, {"command": cmd, "mode": mode, "port": port if mode == "server" else None})

    # ── Server lifecycle ─────────────────────────────────
    def _server_start(self):
        cfg = _read_body(self)
        port     = cfg.get("port", 8081)
        model    = cfg.get("model", "")
        ctx_size = cfg.get("ctx_size", 65536)
        ngl      = cfg.get("ngl", 48)
        use_llama_path = cfg.get("use_llama_path", False)
        llama_bin_path = cfg.get("llama_bin_path", str(LLAMA_BUILD_DIR))

        pid_file = _server_pid_file()
        if pid_file.exists():
            try:
                old_pid = int(pid_file.read_text().strip())
                os.kill(old_pid, 0)
                return _json_resp(self, {"error": f"Server already running (PID {old_pid}).", "pid": old_pid})
            except (ValueError, ProcessLookupError, OSError):
                pid_file.unlink(missing_ok=True)

        models = _load_json(MODELS_FILE, [])
        model_path = model
        for m in models:
            if m["name"] == model:
                model_path = m["path"]
                break

        if use_llama_path and llama_bin_path:
            bin_dir = Path(llama_bin_path)
        else:
            bin_dir = LLAMA_BUILD_DIR

        server_exe = bin_dir / "llama-server"
        args = [
            str(server_exe), "-m", model_path,
            "--ctx-size", str(ctx_size),
            "-ngl", str(ngl),
            "--flash-attn", "on",
            "--port", str(port),
            "--host", "0.0.0.0",
            "--path", str(UI_DIST_DIR),
        ]

        log_f = open(_server_log_path(), "w")
        proc = subprocess.Popen(
            args,
            stdout=log_f, stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        pid_file.write_text(str(proc.pid))

        def _open_browser():
            time.sleep(3)
            webbrowser.open(f"http://127.0.0.1:{port}")

        Thread(target=_open_browser, daemon=True).start()
        return _json_resp(self, {"ok": True, "pid": proc.pid, "port": port, "url": f"http://127.0.0.1:{port}"})

    def _server_stop(self):
        pid_file = _server_pid_file()
        if not pid_file.exists():
            return _json_resp(self, {"ok": True, "message": "No server running."})
        try:
            pid = int(pid_file.read_text().strip())
            os.kill(pid, signal.SIGTERM)
            for _ in range(50):
                time.sleep(0.1)
                try:
                    os.kill(pid, 0)
                except (ProcessLookupError, OSError):
                    break
            else:
                try:
                    os.kill(pid, signal.SIGKILL)
                except (ProcessLookupError, OSError):
                    pass
            pid_file.unlink(missing_ok=True)
            return _json_resp(self, {"ok": True, "message": f"Server (PID {pid}) stopped."})
        except (ValueError, ProcessLookupError, OSError):
            pid_file.unlink(missing_ok=True)
            return _json_resp(self, {"ok": True, "message": "Server was not running."})

    # ── Conversion ───────────────────────────────────────
    def _convert(self, conv_type):
        data = _read_body(self)
        llama_path = data.get("llama_path", str(LLAMA_SRC_DIR))
        src_path   = data.get("model_src", "")
        lora_path  = data.get("lora_src", "")
        dst_path   = data.get("model_dst", "")
        outtype    = data.get("outtype", "f16")

        if conv_type == "model":
            if not src_path or not dst_path:
                return _json_resp(self, {"error": "Source and destination paths are required."}, 400)
            script = Path(llama_path) / "convert_hf_to_gguf.py"
            if not script.exists():
                return _json_resp(self, {"error": f"convert_hf_to_gguf.py not found at {script}"}, 400)
            cmd = (
                f'cd {shlex.quote(str(llama_path))} && '
                f'python convert_hf_to_gguf.py {shlex.quote(src_path)} '
                f'--outtype {shlex.quote(outtype)} --outfile {shlex.quote(dst_path)}'
            )
        else:
            if not lora_path or not dst_path or not src_path:
                return _json_resp(self, {"error": "LoRA source, base model, and destination paths are required."}, 400)
            script = Path(llama_path) / "convert_lora_to_gguf.py"
            if not script.exists():
                return _json_resp(self, {"error": f"convert_lora_to_gguf.py not found at {script}"}, 400)
            cmd = (
                f'cd {shlex.quote(str(llama_path))} && '
                f'python convert_lora_to_gguf.py {shlex.quote(lora_path)} '
                f'--outfile {shlex.quote(dst_path)} --outtype {shlex.quote(outtype)} '
                f'--base {shlex.quote(src_path)}'
            )
        return _json_resp(self, {"command": cmd, "ok": True})

    # ── Terminal opener ──────────────────────────────────
    def _open_terminal(self):
        data = _read_body(self)
        cmd = data.get("command", "")
        is_wsl = data.get("is_wsl", False)

        # Detect WSL: /proc/sys/fs/binfmt_misc/WSLInterop or /mnt/c/Windows
        actually_wsl = os.path.exists("/proc/sys/fs/binfmt_misc/WSLInterop") or os.path.exists("/mnt/c/Windows")

        if is_wsl or actually_wsl:
            escaped_cmd = cmd.replace("\\", "\\\\").replace('"', '\\"')
            wsl_distro = data.get("wsl_distro", "archlinux")
            wt_cmd = (
                f'wt.exe -d "\\\\wsl.localhost\\{wsl_distro}\\home\\rhys" '
                f'wsl ~ -e bash -c "{escaped_cmd}"'
            )
            return _json_resp(self, {"terminal_cmd": wt_cmd, "original_cmd": cmd, "platform": "wsl"})
        else:
            return _json_resp(self, {"terminal_cmd": cmd, "original_cmd": cmd, "platform": "linux"})


# ── Auto-seed models from ~/models/ ────────────────────────
def seed_models():
    """Scan ~/models/ for .gguf files and auto-populate models.json."""
    if not MODELS_DIR.exists():
        print(f"  Models dir not found: {MODELS_DIR}")
        return

    existing = _load_json(MODELS_FILE, [])
    existing_paths = {m["path"] for m in existing}

    gguf_files = sorted(MODELS_DIR.rglob("*.gguf"))
    added = 0
    for gf in gguf_files:
        if str(gf) in existing_paths:
            continue
        name = gf.stem
        # Trim common suffixes
        for suf in ["-Q4_K_M", "-Q5_K_M", "-Q5_K_S", "-Q4_0", "-Q8_0", "-IQ4_NL",
                     "-Q2_K", "-Q3_K_M", "-Q3_K_S", "-Q6_K", "-MTP", "-uncensored",
                     "-Aggressive", "-it", "-chat", "-instruct"]:
            name = name.replace(suf, "")
        # Truncate very long names
        if len(name) > 40:
            name = name[:40]
        existing.append({"name": name, "path": str(gf)})
        existing_paths.add(str(gf))
        added += 1
        print(f"  + {name}  →  {gf}")

    if added > 0:
        _save_json(MODELS_FILE, existing)
        print(f"  Added {added} model(s). Total: {len(existing)}")
    else:
        print(f"  No new models found. Total: {len(existing)}")


# ── Main ───────────────────────────────────────────────────
if __name__ == "__main__":
    # Seed if requested
    if "--seed" in sys.argv:
        print("Scanning ~/models/ for .gguf files...")
        seed_models()
        print("Done.")
        sys.exit(0)

    # Auto-seed on first run if no models.json
    if not MODELS_FILE.exists():
        print("First run: seeding models from ~/models/...")
        seed_models()

    print("=" * 55)
    print("  llama_gui_wsl - Web GUI for llama.cpp (WSL)")
    print("=" * 55)
    print()
    print(f"  Open in browser:  http://localhost:5000")
    print(f"  Models dir:       {MODELS_DIR}")
    print(f"  Llama binaries:   {LLAMA_BUILD_DIR}")
    print()
    print("  Press Ctrl+C to stop.")
    print("=" * 55)

    host = "0.0.0.0"
    port = 5000
    server = HTTPServer((host, port), LlamaGUIHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.shutdown()
