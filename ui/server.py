#!/usr/bin/env python3
"""
Embedded CLI debug server.

Spawns cli_sim, bridges its stdio over a WebSocket, and serves the web UI
on http://localhost:8080.

Usage:
    cd ui && python server.py
"""

import asyncio
import functools
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
SIM_BIN    = SCRIPT_DIR.parent / "sim" / "cli_sim"
HTTP_PORT  = 8080
WS_PORT    = 8765
# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def get_commands():
    """Run cli_sim --list-cmds to get the command list as JSON without touching the live stream."""
    import subprocess, json
    result = subprocess.run([str(SIM_BIN), '--list-cmds'], capture_output=True, text=True, timeout=3)
    return json.loads(result.stdout)


# ---------------------------------------------------------------------------
# Sim process + client registry
# ---------------------------------------------------------------------------

proc     = None
commands = []
clients  = set()


async def broadcast(msg: str):
    for ws in list(clients):
        try:
            await ws.send(msg)
        except Exception:
            clients.discard(ws)


async def start_sim():
    global proc, commands

    # Get command list from a separate one-shot run — does not touch the live stream
    commands = get_commands()

    proc = await asyncio.create_subprocess_exec(
        str(SIM_BIN),
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
    )


async def output_loop():
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
# WebSocket handler
# ---------------------------------------------------------------------------

async def ws_handler(ws):
    clients.add(ws)
    # Send command list immediately so the sidebar populates
    await ws.send(json.dumps({'type': 'commands', 'data': commands}))
    try:
        async for raw in ws:
            msg = json.loads(raw)
            if msg.get('type') == 'input' and proc and proc.stdin:
                proc.stdin.write(msg['data'].encode())
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
    if not SIM_BIN.exists():
        sys.exit(f"cli_sim not found — run 'make' in {SIM_BIN.parent} first.")

    print(f"Starting {SIM_BIN.name}…")
    await start_sim()
    print(f"Commands found: {[c['name'] for c in commands]}")

    asyncio.create_task(output_loop())

    threading.Thread(target=http_thread, daemon=True).start()

    print(f"\n  UI  →  http://localhost:{HTTP_PORT}")
    print(f"  WS  →  ws://localhost:{WS_PORT}\n")

    async with websockets.serve(ws_handler, 'localhost', WS_PORT):
        await asyncio.Future()


asyncio.run(main())
