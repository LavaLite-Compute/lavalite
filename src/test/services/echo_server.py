#!/usr/bin/env python3
"""
Minimal echo server, stdlib only. Answers every request with its own
method/path/headers/body -- just enough to prove a bservice endpoint
is reachable end to end (registration -> port alloc -> proxy -> here)
before touching apptainer/sif at all.

Usage: echo_server.py [port]   (default 8888)
"""
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class EchoHandler(BaseHTTPRequestHandler):
    def _echo(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length else b""

        lines = [f"{self.command} {self.path} {self.request_version}"]
        lines += [f"{k}: {v}" for k, v in self.headers.items()]
        lines.append("")
        lines.append(body.decode(errors="replace"))
        payload = "\n".join(lines).encode()

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    do_GET = _echo
    do_POST = _echo
    do_PUT = _echo

    def log_message(self, fmt, *args):
        sys.stderr.write("echo: " + (fmt % args) + "\n")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
    HTTPServer(("0.0.0.0", port), EchoHandler).serve_forever()
