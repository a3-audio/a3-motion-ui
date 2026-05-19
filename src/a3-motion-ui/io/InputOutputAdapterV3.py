#!/usr/bin/env python3
"""
a3-motion host – polls the firmware and prints human-readable input events.

Usage:
    python3 host.py [PORT] [-v|--verbose]  (default: /dev/ttyACM0)

    -v / --verbose  Print all encoder and pot values every cycle,
                    not just changes. Useful to verify hardware connections.

Protocol (firmware/include/protocol.h):
    0x01  PING         → [0x01]                           1 B
    0x02  GET_POTS     → [0x02] + 4 × u16 LE              9 B
    0x03  GET_ENCODERS → [0x03] + 8 × (i16 LE, u8)       25 B
    0x04  GET_BUTTONS  → [0x04] + 11 packed bytes         12 B
"""

import sys
import time
import struct
import serial

# ── button labels in MATRIX_BUTTONS[] order ──────────────────────────────────
BUTTON_LABELS = [
    "40","30","20","50", "21","51","31","41",
    "42","32","22","52", "23","53","33","43",
    "44","34","24","54", "25","55","35","45",
    "46","36","26","56", "27","57","37","47",
    "48","38","28","58", "29","59","39","49",
    "00","10","09","19",
]

# ── 2-bit button-state meanings ───────────────────────────────────────────────
# bit0 = current level (0=pressed/LOW, 1=released/HIGH)
# bit1 = sawRise && sawFall (full cycle happened)
STATE_LABEL = {0: "HOLD", 1: "-", 2: "BOUNCE", 3: "CLICK"}

POT_DIVIDER = 5  # poll pots every Nth cycle

# Pre-built command batches: send all commands for a cycle in one write,
# receive all responses in one blocking read.
#   BTN(12) + ENC(25)          = 37 bytes  (normal cycle)
#   BTN(12) + ENC(25) + POT(9) = 46 bytes  (pot cycle)
_CMD_BTN_ENC     = bytes([0x04, 0x03])
_CMD_BTN_ENC_POT = bytes([0x04, 0x03, 0x02])

# Timeout long enough for 3 × firmware loop delay (3 × 20 ms) + margin.
# A single port.read(n) call blocks until all n bytes arrive or this elapses.
SERIAL_TIMEOUT = 0.25


def read_exact(port: serial.Serial, n: int) -> bytes:
    """Block until exactly n bytes are received (relies on SERIAL_TIMEOUT)."""
    data = port.read(n)
    if len(data) != n:
        raise TimeoutError(f"expected {n} B, got {len(data)}")
    return data


def _process_buttons(raw: bytes, offset: int) -> None:
    """Parse 12-byte button frame starting at raw[offset]."""
    if raw[offset] != 0x04:
        return
    packed = raw
    base = offset + 1
    events = []
    for i in range(44):
        state = (packed[base + (i >> 2)] >> ((i & 3) << 1)) & 0x03
        if state != 1:
            events.append(f"BTN_{BUTTON_LABELS[i]}={STATE_LABEL[state]}")
    if events:
        print("BUTTONS  ", "  ".join(events))


def _process_encoders(raw: bytes, offset: int, verbose: bool) -> None:
    """Parse 25-byte encoder frame starting at raw[offset]."""
    if raw[offset] != 0x03:
        return
    events = []
    base = offset + 1
    for i in range(8):
        o = base + i * 3
        delta = struct.unpack_from("<h", raw, o)[0]
        sw    = raw[o + 2]
        if verbose or delta != 0:
            direction = "CW" if delta > 0 else ("CCW" if delta < 0 else "--")
            events.append(f"ENC{i+1}={direction}({delta:+d})")
        if verbose or sw != 1:
            events.append(f"ENC{i+1}_SW={STATE_LABEL[sw]}")
    if events:
        print("ENCODERS ", "  ".join(events))


def _process_pots(raw: bytes, offset: int, prev: list, verbose: bool) -> list:
    """Parse 9-byte pot frame starting at raw[offset]; return updated values."""
    if raw[offset] != 0x02:
        return prev
    vals = list(struct.unpack_from("<4H", raw, offset + 1))
    if verbose:
        print("POTS     ", "  ".join(f"POT{i+1}={vals[i]}" for i in range(4)))
    else:
        changed = [f"POT{i+1}={vals[i]}" for i in range(4)
                   if abs(vals[i] - prev[i]) > 8]
        if changed:
            print("POTS     ", "  ".join(changed))
    return vals


def main():
    args = sys.argv[1:]
    verbose = "-v" in args or "--verbose" in args
    args = [a for a in args if a not in ("-v", "--verbose")]
    port_name = args[0] if args else "/dev/ttyACM0"
    print(f"Connecting to {port_name} …" + (" [verbose]" if verbose else ""))

    with serial.Serial(port_name, 115200, timeout=SERIAL_TIMEOUT) as port:
        time.sleep(0.1)
        port.reset_input_buffer()
        print("Ready. Press Ctrl-C to quit.\n")

        pot_vals = [0, 0, 0, 0]
        cycle = 0
        while True:
            try:
                if cycle % POT_DIVIDER == 0:
                    port.write(_CMD_BTN_ENC_POT)
                    raw = read_exact(port, 46)
                    _process_buttons(raw, 0)
                    _process_encoders(raw, 12, verbose)
                    pot_vals = _process_pots(raw, 37, pot_vals, verbose)
                else:
                    port.write(_CMD_BTN_ENC)
                    raw = read_exact(port, 37)
                    _process_buttons(raw, 0)
                    _process_encoders(raw, 12, verbose)
                cycle += 1
            except TimeoutError as e:
                print(f"[timeout] {e}")
                port.reset_input_buffer()
                cycle = 0


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nBye.")
