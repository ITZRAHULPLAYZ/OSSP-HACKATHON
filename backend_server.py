#!/usr/bin/env python3
"""
Smart CPU Scheduler — Backend API Server & Frontend Host
Bridges the compiled C binary (scheduler.exe) with the high-contrast Web Visualizer.

Routes:
  GET  /api/status     - Checks C binary health & compilation status
  GET  /api/c-compare  - Invokes scheduler.exe --compare --json and returns real C benchmark
  GET  /api/c-run      - Invokes scheduler.exe --algo ... --json and returns real C execution
  GET  /*              - Serves frontend/ static assets with zero-cache headers
"""

import os
import sys
import json
import subprocess
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

PORT = 8765
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FRONTEND_DIR = os.path.join(BASE_DIR, "frontend")
BINARY_PATH = os.path.join(BASE_DIR, "scheduler.exe")

class SchedulerAPIHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=FRONTEND_DIR, **kwargs)

    def end_headers(self):
        # Force strict anti-caching so browsers always execute fresh JS
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        params = parse_qs(parsed.query)

        # ── API: Health & Status ──
        if path == "/api/status":
            binary_exists = os.path.isfile(BINARY_PATH)
            resp = {
                "status": "online",
                "c_binary": BINARY_PATH,
                "c_binary_found": binary_exists,
                "engine": "Smart CPU Scheduler C Engine (MinGW-w64)",
                "port": PORT
            }
            self.send_json(resp)
            return

        # ── API: Run C Comparison Benchmark ──
        if path == "/api/c-compare":
            procs = params.get("procs", ["8"])[0]
            ticks = params.get("ticks", ["120"])[0]

            if not os.path.isfile(BINARY_PATH):
                self.send_json({"error": "scheduler.exe not found. Please compile with build.bat"}, 500)
                return

            cmd = [BINARY_PATH, "--compare", "--procs", str(procs), "--ticks", str(ticks), "--json"]
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, cwd=BASE_DIR, timeout=10)
                if proc.returncode == 0:
                    data = json.loads(proc.stdout)
                    data["source"] = "C Binary (scheduler.exe)"
                    self.send_json(data)
                else:
                    self.send_json({"error": "C execution failed", "stderr": proc.stderr}, 500)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # ── API: Single C Run ──
        if path == "/api/c-run":
            algo = params.get("algo", ["mlfq"])[0]
            procs = params.get("procs", ["8"])[0]
            ticks = params.get("ticks", ["120"])[0]

            if not os.path.isfile(BINARY_PATH):
                self.send_json({"error": "scheduler.exe not found."}, 500)
                return

            cmd = [BINARY_PATH, "--algo", algo, "--procs", str(procs), "--ticks", str(ticks), "--json"]
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, cwd=BASE_DIR, timeout=10)
                if proc.returncode == 0:
                    data = json.loads(proc.stdout)
                    data["source"] = "C Binary (scheduler.exe)"
                    self.send_json(data)
                else:
                    self.send_json({"error": "C execution failed", "stderr": proc.stderr}, 500)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # Default: Serve static files from frontend/
        super().do_GET()

    def send_json(self, data, code=200):
        body = json.dumps(data, indent=2).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

def run():
    server_address = ("", PORT)
    httpd = ThreadingHTTPServer(server_address, SchedulerAPIHandler)
    httpd.daemon_threads = True
    print(f"[+] Smart CPU Scheduler Multi-Threaded Backend running on http://localhost:{PORT}")
    print(f"[+] Connected C Binary: {BINARY_PATH} ({'FOUND' if os.path.isfile(BINARY_PATH) else 'MISSING'})")
    httpd.serve_forever()

if __name__ == "__main__":
    run()
