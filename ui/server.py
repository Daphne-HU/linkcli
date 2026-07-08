#!/usr/bin/env python3
"""
Embedded CLI debug server.

Bridges a CLI byte stream to the web UI over a WebSocket and serves the UI
on http://localhost:8080. Two backends:

  sim (default)  — spawns ../sim/cli_sim and bridges its stdio
  serial         — opens a serial port and talks to real firmware

Usage:
    cd ui && python server.py                      # simulator
    cd ui && python server.py --port                # first /dev/tty.usbmodem*
    cd ui && python server.py --port /dev/tty.usbmodem1101 --baud 115200

The serial backend needs pyserial (pip install pyserial). The command
sidebar is populated by sending 'help' once at startup and parsing the
"  <name> - <help>" lines the firmware answers with.
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
import time
from pathlib import Path

try:
    import websockets
except ImportError:
    sys.exit("Missing dependency — run: pip install websockets")

SCRIPT_DIR = Path(__file__).parent
SIM_BIN    = SCRIPT_DIR.parent / "sim" / "cli_sim"
HTTP_PORT  = 8080
WS_PORT    = 8765

# ---------------------------------------------------------------------------
# Shared state
# ---------------------------------------------------------------------------

proc     = None   # sim backend
ser      = None   # serial backend
commands = []
clients  = set()


async def broadcast(msg: str):
    for ws in list(clients):
        try:
            await ws.send(msg)
        except Exception:
            clients.discard(ws)


# ---------------------------------------------------------------------------
# Sim backend
# ---------------------------------------------------------------------------

def sim_get_commands():
    """Run cli_sim --list-cmds to get the command list as JSON without touching the live stream."""
    import subprocess
    result = subprocess.run([str(SIM_BIN), '--list-cmds'], capture_output=True, text=True, timeout=3)
    return json.loads(result.stdout)


async def start_sim():
    global proc, commands

    if not SIM_BIN.exists():
        sys.exit(f"cli_sim not found — build it first (or use --port for real hardware).")

    # Get command list from a separate one-shot run — does not touch the live stream
    commands = sim_get_commands()

    proc = await asyncio.create_subprocess_exec(
        str(SIM_BIN),
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
    )


async def sim_output_loop():
    """Forward sim stdout to every connected browser tab."""
    while True:
        data = await proc.stdout.read(256)
        if not data:
            # Sim exited (e.g. 'quit' command)
            await broadcast(json.dumps({
                'type': 'output',
                'data': '\r\n\x1b[31m[sim exited — restart the server]\x1b[0m\r\n',
            }))
            break
        await broadcast(json.dumps({'type': 'output', 'data': data.decode(errors='replace')}))


# ---------------------------------------------------------------------------
# Serial backend
# ---------------------------------------------------------------------------

def serial_probe_commands():
    """Send 'help' and parse '  <name> - <help>' lines to fill the sidebar.
    Runs before any browser is connected, so the reply never hits the UI."""
    ser.reset_input_buffer()
    ser.write(b"help\r\n")

    buf = b""
    deadline = time.time() + 1.5
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk
        elif buf.rstrip().endswith(b">"):   # prompt seen — reply is complete
            break

    cmds = []
    for line in buf.decode(errors='replace').splitlines():
        m = re.match(r'^\s+(\S+) - (.*)$', line)
        if m:
            cmds.append({'name': m.group(1), 'help': m.group(2).strip()})
    return cmds


def start_serial(port: str, baud: int):
    global ser, commands

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
    commands = serial_probe_commands()


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
            if msg.get('type') != 'input':
                continue
            data = msg['data'].encode()
            if ser:
                ser.write(data)
            elif proc and proc.stdin:
                proc.stdin.write(data)
                await proc.stdin.drain()
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
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', nargs='?', const='auto', default=None,
                    help="serial port to real firmware; no value = first /dev/tty.usbmodem*")
    ap.add_argument('--baud', type=int, default=115200)
    args = ap.parse_args()

    if args.port:
        start_serial(args.port, args.baud)
        asyncio.create_task(serial_output_loop())
    else:
        print(f"Starting {SIM_BIN.name}…")
        await start_sim()
        asyncio.create_task(sim_output_loop())

    print(f"Commands found: {[c['name'] for c in commands]}")

    threading.Thread(target=http_thread, daemon=True).start()

    print(f"\n  UI  →  http://localhost:{HTTP_PORT}")
    print(f"  WS  →  ws://{'localhost'}:{WS_PORT}\n")

    async with websockets.serve(ws_handler, 'localhost', WS_PORT):
        await asyncio.Future()


asyncio.run(main())
