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
        self.proc = subprocess.Popen(
            [
                "arecord", "-q",
                "-D", device,
                "-f", "S16_LE",
                "-c", str(channels),
                "-r", str(sample_rate),
                "-t", "raw",
            ],
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
        pcm = np.frombuffer(data, dtype=np.int16).reshape(1, self.samples)
        frame = AudioFrame.from_ndarray(pcm, format="s16", layout="mono")
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
        self.proc = subprocess.Popen(
            [
                "aplay", "-q",
                "-D", device,
                "-f", "S16_LE",
                "-c", str(channels),
                "-r", str(sample_rate),
                "-t", "raw",
            ],
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
                    data = out.to_ndarray().astype(np.int16, copy=False).tobytes()
                    if self.proc.stdin:
                        await asyncio.to_thread(self.proc.stdin.write, data)
                        await asyncio.to_thread(self.proc.stdin.flush)
        except Exception as exc:
            print(f"[webrtc] playback ended: {exc}", flush=True)
        finally:
            self.close()

    def close(self):
        self.closed = True
        try:
            if self.proc.stdin:
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
            if pc.connectionState in ("failed", "closed", "disconnected"):
                await close_pc(pc, mic_track, players, pcs)

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
    app = loop.run_until_complete(make_app(args))

    print(f"[webrtc] listening on http://{args.host}:{args.port}", flush=True)
    web.run_app(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
