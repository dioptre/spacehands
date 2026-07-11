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

    // Constant two-way audio:
    //   projector mic → Pi earphones  via MediaRecorder POST chunks → ffplay
    //   Pi mic        → projector     via /pi-mic ffmpeg stream → <audio>
    // Browsers require a user gesture for mic permission/audio playback, so the
    // operator clicks "start 2-way audio" once; after that both directions stream
    // continuously until "stop audio" is clicked.
    let micStream = null;
    let recorder = null;
    let projectorMicActive = false;
    let piMicActive = false;
    let audioLinkActive = false;
    let piMicReconnectTimer = null;

    function talkUrl(path) {
        return `http://${cleanHost(hostInput.value)}:8080/talk/${path}`;
    }

    async function postTalk(path, body, contentType) {
        const opts = { method: 'POST', mode: 'cors' };
        if (body) opts.body = body;
        if (contentType) opts.headers = { 'Content-Type': contentType };
        await fetch(talkUrl(path), opts);
    }

    function setAudioUi(active) {
        talkBtn.classList.toggle('active', active);
        listenBtn.classList.toggle('active', active);
        talkBtn.textContent = active ? '2-way audio running' : 'start 2-way audio';
        listenBtn.textContent = active ? 'stop audio' : 'audio stopped';
    }

    async function startProjectorMicToPi() {
        if (projectorMicActive) return;
        if (!window.MediaRecorder) throw new Error('MediaRecorder unavailable in this browser');
        micStream = micStream || await navigator.mediaDevices.getUserMedia({
            audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true },
            video: false
        });
        await postTalk('start');

        const mime = MediaRecorder.isTypeSupported('audio/webm;codecs=opus')
            ? 'audio/webm;codecs=opus'
            : 'audio/webm';
        recorder = new MediaRecorder(micStream, { mimeType: mime, audioBitsPerSecond: 48000 });
        recorder.ondataavailable = async (e) => {
            if (!projectorMicActive || !e.data || e.data.size === 0) return;
            try { await postTalk('chunk', e.data, mime); }
            catch (err) { console.warn('[talk] chunk failed', err); }
        };
        recorder.onerror = (e) => console.warn('[talk] recorder error', e);
        projectorMicActive = true;
        recorder.start(120); // continuous low-ish-latency chunks
    }

    async function stopProjectorMicToPi() {
        if (!projectorMicActive) return;
        projectorMicActive = false;
        try {
            if (recorder && recorder.state !== 'inactive') recorder.stop();
        } catch (_) {}
        recorder = null;
        try { await postTalk('stop'); } catch (_) {}
    }

    async function startPiMicToProjector() {
        if (piMicActive) return;
        const host = cleanHost(hostInput.value);
        piAudio.src = `http://${host}:8080/pi-mic?t=${Date.now()}`;
        piAudio.muted = false;
        piAudio.volume = 1.0;
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

    async function startAudioLink() {
        if (audioLinkActive) return;
        try {
            await startProjectorMicToPi();
            await startPiMicToProjector();
            audioLinkActive = true;
            setAudioUi(true);
            setStatus('constant 2-way audio running');
        } catch (err) {
            console.error(err);
            setStatus('2-way audio failed: ' + (err.name || err.message));
            await stopAudioLink();
        }
    }

    async function stopAudioLink() {
        audioLinkActive = false;
        await stopProjectorMicToPi();
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
    piAudio.addEventListener('error', () => {
        if (!audioLinkActive) return;
        setStatus('Pi mic reconnecting...');
        piMicActive = false;
        if (piMicReconnectTimer) clearTimeout(piMicReconnectTimer);
        piMicReconnectTimer = setTimeout(() => startPiMicToProjector().catch(() => {}), 1000);
    });

    setAudioUi(false);
    connectCamera();
})();
