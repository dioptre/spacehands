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

    // WebRTC two-way audio:
    //   projector mic → Pi earphones
    //   Pi mic        → projector speaker
    // Signaling goes to the Pi sidecar on :8091. Media goes peer-to-peer over WebRTC.
    let audioLinkActive = false;
    let pc = null;
    let localStream = null;
    let remoteStream = null;
    let reconnectTimer = null;
    let watchdogTimer = null;
    let reconnectAttempts = 0;

    const RECONNECT_BASE_MS = 500;
    const RECONNECT_MAX_MS = 4000;

    function setAudioUi(active) {
        talkBtn.classList.toggle('active', active);
        listenBtn.classList.toggle('active', active);
        talkBtn.textContent = active ? 'WebRTC audio running' : 'start 2-way audio';
        listenBtn.textContent = active ? 'stop audio' : 'audio stopped';
    }

    function webrtcUrl(path) {
        return `http://${cleanHost(hostInput.value)}:8091${path}`;
    }

    function waitForIceGatheringComplete(peer) {
        if (peer.iceGatheringState === 'complete') return Promise.resolve();
        return new Promise(resolve => {
            const timeout = setTimeout(resolve, 2500);
            function check() {
                if (peer.iceGatheringState === 'complete') {
                    clearTimeout(timeout);
                    peer.removeEventListener('icegatheringstatechange', check);
                    resolve();
                }
            }
            peer.addEventListener('icegatheringstatechange', check);
        });
    }

    async function postOffer(offer) {
        const controller = new AbortController();
        const timer = setTimeout(() => controller.abort(), 5000);
        try {
            const res = await fetch(webrtcUrl('/offer'), {
                method: 'POST',
                mode: 'cors',
                cache: 'no-store',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(offer),
                signal: controller.signal,
            });
            if (!res.ok) throw new Error(`offer HTTP ${res.status}`);
            return await res.json();
        } finally {
            clearTimeout(timer);
        }
    }

    async function startWebrtcAudio() {
        if (pc) await stopWebrtcAudio(false);

        localStream = localStream || await navigator.mediaDevices.getUserMedia({
            audio: {
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true,
                channelCount: 1,
                sampleRate: 48000,
            },
            video: false,
        });

        remoteStream = new MediaStream();
        piAudio.srcObject = remoteStream;
        piAudio.autoplay = true;
        piAudio.playsInline = true;
        piAudio.muted = false;
        piAudio.volume = 1.0;

        pc = new RTCPeerConnection({
            // LAN-only. No STUN/TURN needed; avoids depending on internet access.
            iceServers: [],
            bundlePolicy: 'max-bundle',
        });

        localStream.getAudioTracks().forEach(track => pc.addTrack(track, localStream));

        pc.ontrack = (event) => {
            event.streams[0]?.getTracks().forEach(track => remoteStream.addTrack(track));
            piAudio.play().catch(err => console.warn('[webrtc] audio play failed', err));
        };

        pc.onconnectionstatechange = () => {
            console.log('[webrtc] connection state', pc.connectionState);
            if (!audioLinkActive) return;
            if (['failed', 'disconnected', 'closed'].includes(pc.connectionState)) {
                scheduleReconnect(`connection ${pc.connectionState}`);
            } else if (pc.connectionState === 'connected') {
                reconnectAttempts = 0;
                setStatus('WebRTC 2-way audio connected');
            }
        };

        pc.oniceconnectionstatechange = () => {
            console.log('[webrtc] ice state', pc.iceConnectionState);
            if (!audioLinkActive) return;
            if (['failed', 'disconnected', 'closed'].includes(pc.iceConnectionState)) {
                scheduleReconnect(`ice ${pc.iceConnectionState}`);
            }
        };

        pc.addTransceiver('audio', { direction: 'sendrecv' });

        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        await waitForIceGatheringComplete(pc);

        const answer = await postOffer({
            sdp: pc.localDescription.sdp,
            type: pc.localDescription.type,
        });
        await pc.setRemoteDescription(answer);
        await piAudio.play();
    }

    function scheduleReconnect(reason) {
        if (!audioLinkActive) return;
        if (reconnectTimer) return;
        reconnectAttempts += 1;
        const delay = Math.min(RECONNECT_MAX_MS, RECONNECT_BASE_MS * reconnectAttempts);
        console.warn('[webrtc] reconnecting:', reason, `in ${delay}ms`);
        setStatus(`WebRTC reconnecting (${reason})...`);
        reconnectTimer = setTimeout(async () => {
            reconnectTimer = null;
            if (!audioLinkActive) return;
            try {
                await startWebrtcAudio();
            } catch (err) {
                console.warn('[webrtc] reconnect failed', err);
                scheduleReconnect(err.message || err.name || 'reconnect failed');
            }
        }, delay);
    }

    function startWatchdog() {
        if (watchdogTimer) clearInterval(watchdogTimer);
        watchdogTimer = setInterval(() => {
            if (!audioLinkActive) return;
            if (!pc || ['failed', 'disconnected', 'closed'].includes(pc.connectionState)) {
                scheduleReconnect('watchdog');
            }
            if (piAudio.paused && remoteStream && remoteStream.getAudioTracks().length > 0) {
                piAudio.play().catch(() => scheduleReconnect('remote audio paused'));
            }
        }, 1500);
    }

    function stopWatchdog() {
        if (watchdogTimer) clearInterval(watchdogTimer);
        watchdogTimer = null;
    }

    async function startAudioLink() {
        if (audioLinkActive) return;
        audioLinkActive = true;
        reconnectAttempts = 0;
        setAudioUi(true);
        setStatus('starting WebRTC audio...');
        try {
            await startWebrtcAudio();
            startWatchdog();
            setStatus('WebRTC 2-way audio connected');
        } catch (err) {
            console.error(err);
            setStatus('WebRTC audio failed: ' + (err.name || err.message));
            scheduleReconnect(err.message || err.name || 'initial connect failed');
        }
    }

    async function stopWebrtcAudio(stopTracks) {
        if (reconnectTimer) clearTimeout(reconnectTimer);
        reconnectTimer = null;
        if (pc) {
            pc.ontrack = null;
            pc.onconnectionstatechange = null;
            pc.oniceconnectionstatechange = null;
            try { pc.getSenders().forEach(s => { try { pc.removeTrack(s); } catch (_) {} }); } catch (_) {}
            try { pc.close(); } catch (_) {}
            pc = null;
        }
        if (remoteStream) {
            remoteStream.getTracks().forEach(t => t.stop());
            remoteStream = null;
        }
        piAudio.pause();
        piAudio.srcObject = null;
        if (stopTracks && localStream) {
            localStream.getTracks().forEach(t => t.stop());
            localStream = null;
        }
    }

    async function stopAudioLink() {
        audioLinkActive = false;
        stopWatchdog();
        await stopWebrtcAudio(true);
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

    setAudioUi(false);
    connectCamera();
})();
