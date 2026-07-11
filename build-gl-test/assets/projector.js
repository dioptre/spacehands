'use strict';

(function () {
    const img = document.getElementById('cam');
    const hostInput = document.getElementById('host');
    const connectBtn = document.getElementById('connect');
    const talkBtn = document.getElementById('talk');
    const listenBtn = document.getElementById('listen');
    const piAudio = document.getElementById('pi-audio');
    const status = document.getElementById('status');

    const params = new URLSearchParams(window.location.search);
    hostInput.value = params.get('pi') || localStorage.getItem('spacehandsPiHost') || 'raspberrypi.local';

    function cleanHost(raw) {
        return raw.trim().replace(/^https?:\/\//, '').replace(/\/$/, '');
    }

    function setStatus(text) { status.textContent = text; }

    function connectCamera() {
        const host = cleanHost(hostInput.value);
        if (!host) return;
        localStorage.setItem('spacehandsPiHost', host);
        img.src = `http://${host}:8082/stream.mjpeg?t=${Date.now()}`;
        setStatus(`camera ← ${host}:8082`);
    }

    // The current C++ MJPEG endpoint returns one JPEG per request, so refresh it.
    setInterval(() => {
        if (!img.src) return;
        const host = cleanHost(hostInput.value);
        img.src = `http://${host}:8082/stream.mjpeg?t=${Date.now()}`;
    }, 66);

    connectBtn.addEventListener('click', connectCamera);
    hostInput.addEventListener('keydown', (e) => { if (e.key === 'Enter') connectCamera(); });
    img.addEventListener('error', () => setStatus('camera reconnecting...'));
    img.addEventListener('load', () => setStatus(`camera ← ${cleanHost(hostInput.value)}:8082`));

    // More resilient low-latency two-way audio:
    //   projector mic → Pi earphones: short Opus/WebM chunks over HTTP POST.
    //   Pi mic        → projector: browser audio element connected to /pi-mic.
    // This is still not true WebRTC latency, but smaller chunks + reconnects make it
    // much closer to real-time and prevent one broken ffmpeg pipe from killing audio.
    let micStream = null;
    let recorder = null;
    let projectorMicActive = false;
    let piMicActive = false;
    let audioLinkActive = false;
    let piMicReconnectTimer = null;
    let projectorMicRestartTimer = null;
    let chunkQueue = Promise.resolve();
    let lastChunkOkAt = 0;
    let watchdogTimer = null;

    const CHUNK_MS = 40;
    const RECONNECT_MS = 700;
    const STALE_MS = 3500;

    function talkUrl(path) {
        return `http://${cleanHost(hostInput.value)}:8080/talk/${path}`;
    }

    async function postTalk(path, body, contentType) {
        const controller = new AbortController();
        const timer = setTimeout(() => controller.abort(), 1200);
        try {
            const opts = { method: 'POST', mode: 'cors', cache: 'no-store', signal: controller.signal };
            if (body) opts.body = body;
            if (contentType) opts.headers = { 'Content-Type': contentType };
            const res = await fetch(talkUrl(path), opts);
            if (!res.ok) throw new Error(`${path} HTTP ${res.status}`);
            return res;
        } finally {
            clearTimeout(timer);
        }
    }

    function setAudioUi(active) {
        talkBtn.classList.toggle('active', active);
        listenBtn.classList.toggle('active', active);
        talkBtn.textContent = active ? '2-way audio running' : 'start 2-way audio';
        listenBtn.textContent = active ? 'stop audio' : 'audio stopped';
    }

    function clearProjectorMicRestart() {
        if (projectorMicRestartTimer) clearTimeout(projectorMicRestartTimer);
        projectorMicRestartTimer = null;
    }

    function scheduleProjectorMicRestart(reason) {
        if (!audioLinkActive) return;
        console.warn('[talk] restarting projector→Pi audio:', reason);
        setStatus('projector mic reconnecting...');
        clearProjectorMicRestart();
        projectorMicRestartTimer = setTimeout(async () => {
            try {
                await stopProjectorMicToPi(false);
                await startProjectorMicToPi();
                setStatus('constant 2-way audio running');
            } catch (err) {
                console.warn('[talk] restart failed', err);
                scheduleProjectorMicRestart(err.message || err.name || 'restart failed');
            }
        }, RECONNECT_MS);
    }

    async function startProjectorMicToPi() {
        if (projectorMicActive) return;
        if (!window.MediaRecorder) throw new Error('MediaRecorder unavailable in this browser');
        micStream = micStream || await navigator.mediaDevices.getUserMedia({
            audio: {
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true,
                channelCount: 1,
                sampleRate: 48000
            },
            video: false
        });
        await postTalk('start');

        const mime = MediaRecorder.isTypeSupported('audio/webm;codecs=opus')
            ? 'audio/webm;codecs=opus'
            : 'audio/webm';
        recorder = new MediaRecorder(micStream, { mimeType: mime, audioBitsPerSecond: 32000 });
        recorder.ondataavailable = (e) => {
            if (!projectorMicActive || !e.data || e.data.size === 0) return;
            // Serialize posts so chunks arrive in order. If one fails, restart the
            // Pi receiver and recorder instead of silently losing audio forever.
            chunkQueue = chunkQueue.then(async () => {
                if (!projectorMicActive) return;
                try {
                    await postTalk('chunk', e.data, mime);
                    lastChunkOkAt = Date.now();
                } catch (err) {
                    scheduleProjectorMicRestart(err.message || err.name || 'chunk failed');
                }
            });
        };
        recorder.onerror = (e) => scheduleProjectorMicRestart(e.error?.message || 'recorder error');
        recorder.onstop = () => {
            if (audioLinkActive && projectorMicActive) scheduleProjectorMicRestart('recorder stopped');
        };
        projectorMicActive = true;
        lastChunkOkAt = Date.now();
        chunkQueue = Promise.resolve();
        recorder.start(CHUNK_MS);
    }

    async function stopProjectorMicToPi(sendStop = true) {
        clearProjectorMicRestart();
        const wasActive = projectorMicActive;
        projectorMicActive = false;
        try {
            if (recorder && recorder.state !== 'inactive') recorder.stop();
        } catch (_) {}
        recorder = null;
        if (sendStop && wasActive) {
            try { await postTalk('stop'); } catch (_) {}
        }
    }

    async function startPiMicToProjector() {
        if (piMicActive) return;
        const host = cleanHost(hostInput.value);
        piAudio.src = `http://${host}:8080/pi-mic?t=${Date.now()}`;
        piAudio.muted = false;
        piAudio.volume = 1.0;
        piAudio.preload = 'none';
        await piAudio.play();
        piMicActive = true;
    }

    function stopPiMicToProjector() {
        piMicActive = false;
        if (piMicReconnectTimer) clearTimeout(piMicReconnectTimer);
        piMicReconnectTimer = null;
        piAudio.pause();
        piAudio.removeAttribute('src');
        piAudio.load();
    }

    function schedulePiMicReconnect(reason) {
        if (!audioLinkActive) return;
        console.warn('[listen] reconnecting Pi mic:', reason);
        setStatus('Pi mic reconnecting...');
        piMicActive = false;
        if (piMicReconnectTimer) clearTimeout(piMicReconnectTimer);
        piMicReconnectTimer = setTimeout(() => {
            stopPiMicToProjector();
            startPiMicToProjector()
                .then(() => setStatus('constant 2-way audio running'))
                .catch(err => schedulePiMicReconnect(err.message || err.name || 'play failed'));
        }, RECONNECT_MS);
    }

    function startWatchdog() {
        if (watchdogTimer) clearInterval(watchdogTimer);
        watchdogTimer = setInterval(() => {
            if (!audioLinkActive) return;
            if (projectorMicActive && Date.now() - lastChunkOkAt > STALE_MS) {
                scheduleProjectorMicRestart('no successful chunks recently');
            }
            if (piMicActive && piAudio.paused) {
                schedulePiMicReconnect('audio element paused');
            }
        }, 1000);
    }

    function stopWatchdog() {
        if (watchdogTimer) clearInterval(watchdogTimer);
        watchdogTimer = null;
    }

    async function startAudioLink() {
        if (audioLinkActive) return;
        audioLinkActive = true;
        setAudioUi(true);
        try {
            await startProjectorMicToPi();
            await startPiMicToProjector();
            startWatchdog();
            setStatus('constant 2-way audio running');
        } catch (err) {
            console.error(err);
            setStatus('2-way audio failed: ' + (err.name || err.message));
            await stopAudioLink();
        }
    }

    async function stopAudioLink() {
        audioLinkActive = false;
        stopWatchdog();
        await stopProjectorMicToPi(true);
        stopPiMicToProjector();
        setAudioUi(false);
        setStatus(`camera ← ${cleanHost(hostInput.value)}:8082`);
    }

    talkBtn.addEventListener('click', startAudioLink);
    listenBtn.addEventListener('click', stopAudioLink);
    window.addEventListener('keydown', (e) => {
        if (e.code === 'Space' && !e.repeat) {
            e.preventDefault();
            audioLinkActive ? stopAudioLink() : startAudioLink();
        }
    });
    piAudio.addEventListener('error', () => schedulePiMicReconnect('audio element error'));
    piAudio.addEventListener('ended', () => schedulePiMicReconnect('audio stream ended'));
    piAudio.addEventListener('stalled', () => schedulePiMicReconnect('audio stream stalled'));
    piAudio.addEventListener('waiting', () => {
        if (audioLinkActive) setStatus('Pi mic buffering...');
    });
    piAudio.addEventListener('playing', () => {
        if (audioLinkActive) setStatus('constant 2-way audio running');
    });

    setAudioUi(false);
    connectCamera();
})();
