#!/usr/bin/env python3
"""
Embedded CLI debug server.

Opens a serial port to firmware running the embedded CLI, bridges the byte
stream to the web UI over a WebSocket, and serves the UI on
http://localhost:8080.

Usage:
    python server.py                                # first /dev/tty.usbmodem*
    python server.py --port /dev/tty.usbmodem1101 --baud 115200
    python server.py --src ~/my_project/app         # scan extra source dirs

The command sidebar is populated by scanning source files for CLI_CMD()
definitions — the help string exists only in the sources, not on the
device (cli_cmd_t stores just name + handler). Pass --src once per
directory to scan; defaults to the library's src/ and examples/.

Needs: pip install websockets pyserial
"""

import argparse
import asyncio
import functools
import glob
import http.server
import json
import re
import sys
import threading
from pathlib import Path

try:
    import websockets
except ImportError:
    sys.exit("Missing dependency — run: pip install websockets")

SCRIPT_DIR = Path(__file__).parent
REPO_DIR   = SCRIPT_DIR.parent
HTTP_PORT  = 8080
WS_PORT    = 8765

SOURCE_EXTS = {'.c', '.cc', '.cpp', '.h', '.hpp'}

# ---------------------------------------------------------------------------
# Command discovery: scan sources for CLI_CMD(name, "help")
# ---------------------------------------------------------------------------

CLI_CMD_RE = re.compile(r'CLI_CMD\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')


def strip_comments(code: str) -> str:
    """Remove // and /* */ comments so commented-out commands are not listed."""
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    return code


def scan_commands(src_dirs):
    cmds = {}
    for src in src_dirs:
        src = Path(src).expanduser()
        if not src.is_dir():
            sys.exit(f"--src {src}: not a directory")
        for path in sorted(src.rglob('*')):
            if path.suffix not in SOURCE_EXTS or not path.is_file():
                continue
            text = strip_comments(path.read_text(errors='replace'))
            for m in CLI_CMD_RE.finditer(text):
                cmds[m.group(1)] = {'name': m.group(1), 'help': m.group(2)}
    return sorted(cmds.values(), key=lambda c: c['name'])


# ---------------------------------------------------------------------------
# Shared state
# ---------------------------------------------------------------------------

ser      = None
commands = []
clients  = set()


async def broadcast(msg: str):
    for ws in list(clients):
        try:
            await ws.send(msg)
        except Exception:
            clients.discard(ws)


# ---------------------------------------------------------------------------
# Serial backend
# ---------------------------------------------------------------------------

def start_serial(port: str, baud: int):
    global ser

    try:
        import serial
    except ImportError:
        sys.exit("Missing dependency — run: pip install pyserial")

    if port == 'auto':
        matches = sorted(glob.glob('/dev/tty.usbmodem*'))
        if not matches:
            sys.exit("No /dev/tty.usbmodem* device found — is the board plugged in?")
        port = matches[0]

    try:
        ser = serial.Serial(port, baud, timeout=0.05)
    except serial.SerialException as e:
        sys.exit(f"Could not open {port}: {e} (is picocom still holding the port?)")

    print(f"Connected to {port} @ {baud}")


async def serial_output_loop():
    """Forward serial bytes to every connected browser tab."""
    loop = asyncio.get_running_loop()
    while True:
        data = await loop.run_in_executor(None, ser.read, 256)
        if data:
            await broadcast(json.dumps({'type': 'output', 'data': data.decode(errors='replace')}))


# ---------------------------------------------------------------------------
# WebSocket handler
# ---------------------------------------------------------------------------

async def ws_handler(ws):
    clients.add(ws)
    # Send command list immediately so the sidebar populates
    await ws.send(json.dumps({'type': 'commands', 'data': commands}))
    try:
        async for raw in ws:
            msg = json.loads(raw)
            if msg.get('type') == 'input':
                ser.write(msg['data'].encode())
    finally:
        clients.discard(ws)


# ---------------------------------------------------------------------------
# HTTP server (serves ui/ directory)
# ---------------------------------------------------------------------------

def http_thread():
    handler = functools.partial(
        http.server.SimpleHTTPRequestHandler,
        directory=str(SCRIPT_DIR),
    )
    # Silence the default request log
    handler.log_message = lambda *a: None
    with http.server.HTTPServer(('localhost', HTTP_PORT), handler) as httpd:
        httpd.serve_forever()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

async def main():
    global commands

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', default='auto',
                    help="serial port (default: first /dev/tty.usbmodem*)")
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--src', action='append', metavar='DIR',
                    help="directory to scan for CLI_CMD definitions (repeatable); "
                         "default: the library's src/ and examples/")
    args = ap.parse_args()

    commands = scan_commands(args.src or [REPO_DIR / 'src', REPO_DIR / 'examples'])
    print(f"Commands found: {[c['name'] for c in commands]}")

    start_serial(args.port, args.baud)
    asyncio.create_task(serial_output_loop())

    threading.Thread(target=http_thread, daemon=True).start()

    print(f"\n  UI  →  http://localhost:{HTTP_PORT}")
    print(f"  WS  →  ws://localhost:{WS_PORT}\n")

    async with websockets.serve(ws_handler, 'localhost', WS_PORT):
        await asyncio.Future()


asyncio.run(main())
