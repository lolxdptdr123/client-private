#!/usr/bin/env python3
"""Helix site server: static files + admin API for changelog/contact."""
from __future__ import annotations

import json
import os
import secrets
import uuid
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

ROOT = os.path.dirname(os.path.abspath(__file__))
CHANGELOG = os.path.join(ROOT, "data", "changelog.json")
CONTACT = os.path.join(ROOT, "data", "contact.json")
PASSWORD_FILE = os.path.join(ROOT, ".admin-password")
BLOCKED = {".admin-password"}


def read_json(path, fallback):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return fallback


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")
    os.replace(tmp, path)


def admin_password():
    env = os.environ.get("HELIX_ADMIN_PASSWORD")
    if env:
        return env.strip()
    try:
        with open(PASSWORD_FILE, "r", encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return "helixadmin"


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _json(self, code, payload):
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _body(self):
        length = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(length) if length else b"{}"
        try:
            return json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError:
            return None

    def _authed(self, data):
        pw = (data or {}).get("password") or self.headers.get("X-Admin-Password", "")
        expected = admin_password()
        return bool(expected) and secrets.compare_digest(str(pw), expected)

    def do_GET(self):
        parsed = urlparse(self.path)
        name = os.path.basename(parsed.path)
        if name in BLOCKED or parsed.path.rstrip("/").endswith("/.admin-password"):
            self.send_error(404)
            return
        if parsed.path == "/api/changelog":
            entries = read_json(CHANGELOG, [])
            entries.sort(key=lambda e: e.get("date", ""), reverse=True)
            self._json(200, {"entries": entries})
            return
        if parsed.path == "/api/contact":
            self._json(200, read_json(CONTACT, {"discord": "", "telegram": ""}))
            return
        super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        data = self._body()
        if data is None:
            self._json(400, {"error": "Invalid JSON"})
            return
        if not self._authed(data):
            self._json(401, {"error": "Wrong password"})
            return
        if parsed.path == "/api/login":
            self._json(200, {"ok": True})
            return
        if parsed.path == "/api/changelog":
            entries = read_json(CHANGELOG, [])
            version = str(data.get("version", "")).strip()
            body = str(data.get("body", "")).strip()
            date = str(data.get("date", "")).strip()
            if not version or not body or not date:
                self._json(400, {"error": "date, version and body are required"})
                return
            entry = {
                "id": str(uuid.uuid4()),
                "date": date,
                "version": version,
                "body": body,
            }
            entries.insert(0, entry)
            write_json(CHANGELOG, entries)
            self._json(200, {"ok": True, "entry": entry})
            return
        if parsed.path == "/api/changelog/delete":
            entry_id = str(data.get("id", "")).strip()
            entries = [e for e in read_json(CHANGELOG, []) if e.get("id") != entry_id]
            write_json(CHANGELOG, entries)
            self._json(200, {"ok": True})
            return
        if parsed.path == "/api/contact":
            contact = read_json(CONTACT, {})
            if "discord" in data:
                contact["discord"] = str(data["discord"]).strip()
            if "telegram" in data:
                contact["telegram"] = str(data["telegram"]).strip()
            write_json(CONTACT, contact)
            self._json(200, {"ok": True, "contact": contact})
            return
        self._json(404, {"error": "Unknown API"})

    def log_message(self, fmt, *args):
        print("[%s] %s" % (self.log_date_time_string(), fmt % args))


if __name__ == "__main__":
    port = int(os.environ.get("PORT", "4173"))
    httpd = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Helix site on http://127.0.0.1:{port}/")
    print("Admin: http://127.0.0.1:%s/admin.html" % port)
    httpd.serve_forever()
