#!/usr/bin/env python3
"""WebRTC two-way audio sidecar for Spacehands Pi.

- Browser/projector sends mic audio over WebRTC.
- Pi plays received audio to ALSA default output.
- Pi captures ALSA default microphone and sends it back over WebRTC.

Signaling is plain HTTP on :8091:
  POST /offer {sdp,type} -> {sdp,type}
  GET  /health
"""
import argparse
import asyncio
import json
import signal
import subprocess
import sys
import time
from fractions import Fraction

import av
import numpy as np
from aiohttp import web
from aiortc import RTCPeerConnection, RTCSessionDescription
from aiortc.mediastreams import AudioFrame, MediaStreamTrack


class AlsaAudioTrack(MediaStreamTrack):
    kind = "audio"

    def __init__(self, device="default", sample_rate=48000, channels=1, frame_ms=20):
        super().__init__()
        self.sample_rate = sample_rate
        self.channels = channels
        self.samples = int(sample_rate * frame_ms / 1000)
        self.frame_bytes = self.samples * channels * 2  # s16le
        self.pts = 0
        self.last_level_log = 0.0
        if device.startswith("pulse:") or device.startswith("bluez_"):
            pulse_device = device.removeprefix("pulse:")
            cmd = [
                "parec",
                "--format", "s16le",
                "--channels", str(channels),
                "--rate", str(sample_rate),
                "--raw",
            ]
            if pulse_device and pulse_device not in ("default", "@DEFAULT_SOURCE@"):
                cmd[1:1] = ["--device", pulse_device]
            print(f"[webrtc] capturing Pi mic via Pulse/PipeWire source: {pulse_device or '@DEFAULT_SOURCE@'}", flush=True)
        else:
            cmd = [
                "arecord", "-q",
                "-D", device,
                "-f", "S16_LE",
                "-c", str(channels),
                "-r", str(sample_rate),
                "-t", "raw",
            ]
            print(f"[webrtc] capturing Pi mic via ALSA device: {device}", flush=True)
        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )

    async def recv(self):
        if not self.proc.stdout:
            raise EOFError("arecord stdout closed")
        data = await asyncio.to_thread(self.proc.stdout.read, self.frame_bytes)
        if len(data) != self.frame_bytes:
            raise EOFError("short ALSA capture read")
        samples_i16 = np.frombuffer(data, dtype=np.int16)
        now = time.time()
        if now - self.last_level_log >= 2.0:
            rms = float(np.sqrt(np.mean(samples_i16.astype(np.float32) ** 2))) if samples_i16.size else 0.0
            peak = int(np.max(np.abs(samples_i16))) if samples_i16.size else 0
            print(f"[webrtc] Pi mic level rms={rms:.1f} peak={peak}", flush=True)
            self.last_level_log = now

        # Build a mono s16 frame directly. This avoids PyAV ndarray shape
        # ambiguity for packed-vs-planar mono audio and is more reliable in aiortc.
        frame = AudioFrame(format="s16", layout="mono", samples=self.samples)
        frame.planes[0].update(data)
        frame.sample_rate = self.sample_rate
        frame.pts = self.pts
        frame.time_base = Fraction(1, self.sample_rate)
        self.pts += self.samples
        return frame

    def stop(self):
        super().stop()
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()


class AlsaPlayer:
    def __init__(self, device="default", sample_rate=48000, channels=1):
        self.sample_rate = sample_rate
        self.channels = channels
        if device.startswith("pulse:") or device.startswith("bluez_"):
            pulse_device = device.removeprefix("pulse:")
            cmd = [
                "pacat",
                "--playback",
                "--format", "s16le",
                "--channels", str(channels),
                "--rate", str(sample_rate),
                "--raw",
            ]
            if pulse_device and pulse_device not in ("default", "@DEFAULT_SINK@"):
                cmd[2:2] = ["--device", pulse_device]
            print(f"[webrtc] playing projector mic via Pulse/PipeWire sink: {pulse_device or '@DEFAULT_SINK@'}", flush=True)
        else:
            cmd = [
                "aplay", "-q",
                "-D", device,
                "-f", "S16_LE",
                "-c", str(channels),
                "-r", str(sample_rate),
                "-t", "raw",
            ]
            print(f"[webrtc] playing projector mic via ALSA device: {device}", flush=True)
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )
        self.resampler = av.AudioResampler(format="s16", layout="mono", rate=sample_rate)
        self.closed = False

    async def play_track(self, track):
        try:
            while not self.closed:
                frame = await track.recv()
                frames = self.resampler.resample(frame)
                if frames is None:
                    continue
                if not isinstance(frames, list):
                    frames = [frames]
                for out in frames:
                    if self.closed or self.proc.poll() is not None or not self.proc.stdin:
                        return
                    data = out.to_ndarray().astype(np.int16, copy=False).tobytes()
                    try:
                        await asyncio.to_thread(self.proc.stdin.write, data)
                        await asyncio.to_thread(self.proc.stdin.flush)
                    except (BrokenPipeError, ValueError, OSError) as exc:
                        # Happens during normal peer replacement/teardown when aplay's
                        # stdin has already closed. Treat as graceful, not an error.
                        print(f"[webrtc] Pi playback pipe closed: {exc}", flush=True)
                        return
        except Exception as exc:
            if not self.closed:
                print(f"[webrtc] playback ended: {exc}", flush=True)
        finally:
            self.close()

    def close(self):
        self.closed = True
        try:
            if self.proc.stdin and not self.proc.stdin.closed:
                self.proc.stdin.close()
        except Exception:
            pass
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()


async def make_app(args):
    pcs = set()

    async def health(_request):
        return web.json_response({"ok": True, "peers": len(pcs)})

    async def offer(request):
        params = await request.json()
        offer = RTCSessionDescription(sdp=params["sdp"], type=params["type"])

        # Keep this sidecar single-client. If the browser reconnects, retire any
        # old peer first so duplicate Pi mic tracks do not fight each other. Do the
        # actual close asynchronously because aioice can still have packets in
        # flight during offer handling.
        old_pcs = list(pcs)
        pcs.clear()
        for old in old_pcs:
            asyncio.create_task(old.close())

        pc = RTCPeerConnection()
        pcs.add(pc)
        print(f"[webrtc] peer connected; peers={len(pcs)}", flush=True)

        mic_track = AlsaAudioTrack(args.input_device, args.rate)
        pc.addTrack(mic_track)

        players = []

        @pc.on("track")
        def on_track(track):
            print(f"[webrtc] received track: {track.kind}", flush=True)
            if track.kind == "audio":
                player = AlsaPlayer(args.output_device, args.rate)
                players.append(player)
                asyncio.create_task(player.play_track(track))

        @pc.on("connectionstatechange")
        async def on_connectionstatechange():
            print(f"[webrtc] connection state: {pc.connectionState}", flush=True)
            # Do not close from inside the state callback. aiortc/aioice may still
            # have STUN packets in flight, and closing here can trigger noisy
            # NoneType.sendto races. The browser owns reconnects; this side just logs
            # state and replaces old peers when a new offer arrives.
            if pc.connectionState == "closed" and pc in pcs:
                pcs.remove(pc)

        await pc.setRemoteDescription(offer)
        answer = await pc.createAnswer()
        await pc.setLocalDescription(answer)

        return web.json_response({"sdp": pc.localDescription.sdp, "type": pc.localDescription.type}, headers={"Access-Control-Allow-Origin": "*"})

    async def options(_request):
        return web.Response(status=204, headers={
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "POST, GET, OPTIONS",
            "Access-Control-Allow-Headers": "Content-Type",
        })

    async def close_all(_app):
        await asyncio.gather(*(pc.close() for pc in list(pcs)), return_exceptions=True)
        pcs.clear()

    app = web.Application()
    app.router.add_get("/health", health)
    app.router.add_post("/offer", offer)
    app.router.add_options("/{tail:.*}", options)
    app.on_shutdown.append(close_all)
    return app


async def close_pc(pc, mic_track, players, pcs):
    try:
        mic_track.stop()
    except Exception:
        pass
    for p in players:
        p.close()
    if pc in pcs:
        pcs.remove(pc)
    await pc.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8091)
    parser.add_argument("--input-device", default="default")
    parser.add_argument("--output-device", default="default")
    parser.add_argument("--rate", type=int, default=48000)
    args = parser.parse_args()

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    def handle_loop_exception(_loop, context):
        exc = context.get("exception")
        msg = context.get("message", "")
        # aioice can log this during peer replacement if a datagram send races with
        # transport close. It is harmless but confusing/noisy in performance logs.
        if isinstance(exc, AttributeError) and "sendto" in str(exc):
            print("[webrtc] ignored late ICE send after close", flush=True)
            return
        print(f"[webrtc] loop exception: {msg} {exc}", file=sys.stderr, flush=True)

    loop.set_exception_handler(handle_loop_exception)
    app = loop.run_until_complete(make_app(args))

    print(f"[webrtc] listening on http://{args.host}:{args.port}", flush=True)
    web.run_app(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
