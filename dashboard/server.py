#!/usr/bin/env python3
"""Local dashboard server. SonarQube credentials stay server-side."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs
import base64
import json
import os
import subprocess
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
STATIC = Path(__file__).resolve().parent
BINARY = Path(os.environ.get("FI_DASHBOARD_BINARY", ROOT / "build" / "fixed_income_dashboard_data"))
HOST = os.environ.get("FI_DASHBOARD_HOST", "127.0.0.1")
PORT = int(os.environ.get("FI_DASHBOARD_PORT", "8080"))


def sonar_summary():
    base = os.environ.get("SONAR_HOST_URL", "").rstrip("/")
    project = os.environ.get("SONAR_PROJECT_KEY", "")
    token = os.environ.get("SONAR_TOKEN", "")
    if not (base and project and token):
        return {"configured": False, "status": "Not analyzed / not configured", "measures": {}, "qualityGate": None}

    auth = base64.b64encode((token + ":").encode()).decode()
    headers = {"Authorization": "Basic " + auth, "Accept": "application/json"}
    metrics_url = base + "/api/measures/component?" + urllib.parse.urlencode({
        "component": project,
        "metricKeys": "bugs,vulnerabilities,code_smells,coverage,duplicated_lines_density,ncloc,security_hotspots"
    })
    gate_url = base + "/api/qualitygates/project_status?" + urllib.parse.urlencode({"projectKey": project})

    def get_json(url):
        request = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(request, timeout=5) as response:
            return json.loads(response.read().decode("utf-8"))

    try:
        data = get_json(metrics_url)
        measures = {m["metric"]: m.get("value") for m in data.get("component", {}).get("measures", [])}
        gate = None
        try:
            gate_data = get_json(gate_url)
            gate = gate_data.get("projectStatus", {}).get("status")
        except (urllib.error.URLError, TimeoutError, ValueError, KeyError):
            gate = "Unavailable"
        return {"configured": True, "status": "Connected" if measures else "No analysis results", "measures": measures, "qualityGate": gate,
                "project": project, "dashboardUrl": base + "/dashboard?id=" + urllib.parse.quote(project)}
    except (urllib.error.URLError, TimeoutError, ValueError, KeyError) as exc:
        # Do not leak the token or detailed response to the browser.
        return {"configured": True, "status": "Analysis unavailable", "measures": {},
                "qualityGate": None, "error": type(exc).__name__}


class Handler(BaseHTTPRequestHandler):
    def _send(self, status, body, content_type):
        data = body if isinstance(body, bytes) else body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self'; img-src 'self' data:")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/sonar":
            self._send(200, json.dumps(sonar_summary()), "application/json; charset=utf-8")
            return
        if parsed.path == "/api/analytics":
            qs = parse_qs(parsed.query)
            try:
                notional = float(qs.get("notional", ["1000"])[0])
                coupon = float(qs.get("coupon", ["0.05"])[0])
                ytm = float(qs.get("ytm", ["0.05"])[0])
                if notional < 1 or notional > 1e12 or not 0 <= coupon <= 1 or not -0.01 <= ytm <= 1:
                    raise ValueError("Input outside allowed range")
                if not BINARY.is_file():
                    self._send(503, json.dumps({"error": "Analytics binary not built. Run the CMake build first."}), "application/json; charset=utf-8")
                    return
                result = subprocess.run([str(BINARY), str(notional), str(coupon), str(ytm)],
                    capture_output=True, text=True, timeout=5, check=False)
                if result.returncode != 0:
                    self._send(400, json.dumps({"error": result.stderr.strip() or "Analytics calculation failed"}), "application/json; charset=utf-8")
                    return
                json.loads(result.stdout)  # Validate output before returning it.
                self._send(200, result.stdout, "application/json; charset=utf-8")
            except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                self._send(400, json.dumps({"error": str(exc)}), "application/json; charset=utf-8")
            return

        relative = "index.html" if parsed.path == "/" else parsed.path.lstrip("/")
        target = (STATIC / relative).resolve()
        if STATIC.resolve() not in target.parents and target != STATIC.resolve():
            self._send(403, "Forbidden", "text/plain; charset=utf-8")
            return
        if not target.is_file():
            self._send(404, "Not found", "text/plain; charset=utf-8")
            return
        mime = {".html": "text/html; charset=utf-8", ".js": "application/javascript; charset=utf-8", ".css": "text/css; charset=utf-8"}.get(target.suffix, "text/plain; charset=utf-8")
        self._send(200, target.read_bytes(), mime)

    def log_message(self, fmt, *args):
        print("[dashboard] " + (fmt % args))


if __name__ == "__main__":
    print(f"Fixed-Income dashboard listening on http://{HOST}:{PORT}")
    print("SonarQube metrics are shown only when SONAR_HOST_URL, SONAR_PROJECT_KEY and SONAR_TOKEN are configured.")
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
