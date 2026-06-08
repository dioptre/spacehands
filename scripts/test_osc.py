#!/usr/bin/env python3
"""Simulate hand OSC messages to test SC instrument without camera/YOLO."""
import socket, struct, time, math, sys

HOST = '127.0.0.1'
PORT = 57120

def pad4(b):
    return b + b'\x00' * ((4 - len(b) % 4) % 4)

def osc(path, fmt='', *args):
    msg = pad4(path.encode() + b'\x00')
    msg += pad4((',' + fmt).encode() + b'\x00')
    for a, f in zip(args, fmt):
        if f == 'i': msg += struct.pack('>i', int(a))
        if f == 'f': msg += struct.pack('>f', float(a))
    return msg

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"Sending simulated hands to {HOST}:{PORT}")
print("Ctrl+C to stop\n")

t = 0
try:
    while True:
        t += 0.05

        # Hand 0: bucket 0 (z=150mm = bass), Y moves up/down for pitch
        x0 = 0.5 + 0.3 * math.sin(t * 0.7)   # X → tempo
        y0 = 0.5 + 0.3 * math.sin(t * 1.1)   # Y → pitch
        z0 = 150.0                              # Z → bucket 0 (bass)
        sock.sendto(osc('/hand','iffffff', 0, x0, y0, z0, 0.0, 1.0), (HOST, PORT))

        # Hand 1: bucket 1 (z=450mm = pad), slower movement
        x1 = 0.5 + 0.2 * math.cos(t * 0.5)
        y1 = 0.5 + 0.2 * math.cos(t * 0.8)
        z1 = 450.0                              # Z → bucket 1 (pad)
        sock.sendto(osc('/hand','iffffff', 1, x1, y1, z1, 0.0, 1.0), (HOST, PORT))

        # hands_end: buckets 0 and 1 active
        sock.sendto(osc('/hands_end','ii', 0, 1), (HOST, PORT))

        bpm = 60.0 * (2.0 ** (x0 * 2.0))
        pitch = int(36 + (1 - y0) * 48)
        print(f"\r  hands: 2  bucket0=bass bucket1=pad  bpm={bpm:.0f}  pitch={pitch}", end='')
        time.sleep(0.033)  # ~30Hz

except KeyboardInterrupt:
    # Send mute on exit
    sock.sendto(osc('/mute','i', 1), (HOST, PORT))
    print("\nStopped.")
sock.close()
