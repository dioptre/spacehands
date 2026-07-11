'use strict';

(function () {
    const img = document.getElementById('cam');
    const hostInput = document.getElementById('host');
    const connectBtn = document.getElementById('connect');
    const talkBtn = document.getElementById('talk');
    const listenBtn = document.getElementById('listen');
    const autoAudioBtn = document.getElementById('auto-audio');
    const micSelect = document.getElementById('mic-select');
    const speakerSelect = document.getElementById('speaker-select');
    const testSpeakerBtn = document.getElementById('test-speaker');
    const cropLeftInput = document.getElementById('crop-left');
    const cropRightInput = document.getElementById('crop-right');
    const flipImageBtn = document.getElementById('flip-image');
    const softenInput = document.getElementById('soften');
    const piAudio = document.getElementById('pi-audio');
    const status = document.getElementById('status');

    const params = new URLSearchParams(window.location.search);
    hostInput.value = params.get('pi') || localStorage.getItem('spacehandsPiHost') || 'raspberrypi.local';

    function cleanHost(raw) {
        return raw.trim().replace(/^https?:\/\//, '').replace(/\/$/, '');
    }

    function setStatus(text) { status.textContent = text; }

    // --- UI auto-hide and projector crop ---
    let hideTimer = null;
    function showUiSoon() {
        document.body.classList.remove('ui-hidden');
        if (hideTimer) clearTimeout(hideTimer);
        hideTimer = setTimeout(() => document.body.classList.add('ui-hidden'), 3000);
    }
    ['mousemove', 'mousedown', 'touchstart', 'keydown', 'wheel'].forEach(ev => {
        window.addEventListener(ev, showUiSoon, { passive: true });
    });

    function applyCrop() {
        const left = Math.max(0, parseInt(cropLeftInput.value || '0', 10));
        const right = Math.max(0, parseInt(cropRightInput.value || '0', 10));
        document.documentElement.style.setProperty('--crop-left', left + 'px');
        document.documentElement.style.setProperty('--crop-right', right + 'px');
        localStorage.setItem('spacehandsCropLeft', String(left));
        localStorage.setItem('spacehandsCropRight', String(right));
    }

    function applySoften() {
        const soften = Math.max(0, Math.min(6, parseFloat(softenInput.value || '0')));
        document.documentElement.style.setProperty('--soften', soften + 'px');
        localStorage.setItem('spacehandsSoften', String(soften));
    }

    cropLeftInput.value = localStorage.getItem('spacehandsCropLeft') || '0';
    cropRightInput.value = localStorage.getItem('spacehandsCropRight') || '0';
    softenInput.value = localStorage.getItem('spacehandsSoften') || '1';
    cropLeftInput.addEventListener('input', applyCrop);
    cropRightInput.addEventListener('input', applyCrop);
    softenInput.addEventListener('input', applySoften);

    let imageFlipped = localStorage.getItem('spacehandsFlipX') === '1';
    function applyFlip() {
        document.documentElement.style.setProperty('--flip-x', imageFlipped ? '-1' : '1');
        flipImageBtn.textContent = 'flip: ' + (imageFlipped ? 'on' : 'off');
        flipImageBtn.classList.toggle('active', imageFlipped);
        localStorage.setItem('spacehandsFlipX', imageFlipped ? '1' : '0');
    }
    flipImageBtn.addEventListener('click', () => {
        imageFlipped = !imageFlipped;
        applyFlip();
    });

    applyCrop();
    applySoften();
    applyFlip();
    showUiSoon();

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
    let audioCtx = null;
    let remoteSource = null;
    let remoteGain = null;
    let reconnectTimer = null;
    let watchdogTimer = null;
    let reconnectAttempts = 0;
    let disconnectedSince = 0;
    let autoAudio = localStorage.getItem('spacehandsAutoAudio') !== '0';

    const RECONNECT_BASE_MS = 500;
    const RECONNECT_MAX_MS = 4000;

    function setAudioUi(active) {
        talkBtn.classList.toggle('active', active);
        listenBtn.classList.toggle('active', active);
        talkBtn.textContent = active ? 'WebRTC audio running' : 'start 2-way audio';
        listenBtn.textContent = active ? 'stop audio' : 'audio stopped';
        autoAudioBtn.classList.toggle('active', autoAudio);
        autoAudioBtn.textContent = 'auto audio: ' + (autoAudio ? 'on' : 'off');
    }

    function webrtcUrl(path) {
        return `http://${cleanHost(hostInput.value)}:8091${path}`;
    }

    function selectedMicConstraint() {
        const id = micSelect.value;
        return id ? { exact: id } : undefined;
    }

    async function applySpeakerDevice() {
        const id = speakerSelect.value;
        localStorage.setItem('spacehandsSpeakerDeviceId', id || '');
        // Chrome supports setSinkId on <audio>; Safari does not. If unsupported,
        // choose the Bluetooth speaker as the macOS system output instead.
        if (typeof piAudio.setSinkId === 'function') {
            try {
                await piAudio.setSinkId(id || '');
            } catch (err) {
                console.warn('[audio] setSinkId failed', err);
                setStatus('speaker select failed: ' + (err.name || err.message));
            }
        } else if (id) {
            setStatus('speaker selection unsupported; use Chrome or macOS Sound output');
        }
    }

    async function ensureAudioContext() {
        audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
        if (audioCtx.state !== 'running') await audioCtx.resume();
        return audioCtx;
    }

    async function testSelectedSpeaker() {
        await applySpeakerDevice();
        const ctx = await ensureAudioContext();
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.frequency.value = 880;
        gain.gain.setValueAtTime(0.0001, ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.2, ctx.currentTime + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + 0.35);
        osc.connect(gain).connect(ctx.destination);
        osc.start();
        osc.stop(ctx.currentTime + 0.38);
        setStatus('speaker test beep');
    }

    async function populateAudioDevices() {
        if (!navigator.mediaDevices?.enumerateDevices) return;
        try {
            const devices = await navigator.mediaDevices.enumerateDevices();
            const savedMic = localStorage.getItem('spacehandsMicDeviceId') || '';
            const savedSpeaker = localStorage.getItem('spacehandsSpeakerDeviceId') || '';
            const curMic = micSelect.value || savedMic;
            const curSpeaker = speakerSelect.value || savedSpeaker;

            micSelect.innerHTML = '<option value="">default</option>';
            speakerSelect.innerHTML = '<option value="">default</option>';
            let micCount = 0, speakerCount = 0;
            devices.forEach(d => {
                if (d.kind === 'audioinput') {
                    micCount++;
                    const opt = document.createElement('option');
                    opt.value = d.deviceId;
                    opt.textContent = d.label || `microphone ${micCount}`;
                    micSelect.appendChild(opt);
                } else if (d.kind === 'audiooutput') {
                    speakerCount++;
                    const opt = document.createElement('option');
                    opt.value = d.deviceId;
                    opt.textContent = d.label || `speaker ${speakerCount}`;
                    speakerSelect.appendChild(opt);
                }
            });
            micSelect.value = Array.from(micSelect.options).some(o => o.value === curMic) ? curMic : '';
            speakerSelect.value = Array.from(speakerSelect.options).some(o => o.value === curSpeaker) ? curSpeaker : '';
            await applySpeakerDevice();
        } catch (err) {
            console.warn('[audio] enumerateDevices failed', err);
        }
    }

    micSelect.addEventListener('change', async () => {
        localStorage.setItem('spacehandsMicDeviceId', micSelect.value || '');
        if (localStream) {
            localStream.getTracks().forEach(t => t.stop());
            localStream = null;
        }
        if (audioLinkActive) scheduleReconnect('microphone changed');
    });
    speakerSelect.addEventListener('change', async () => {
        await applySpeakerDevice();
        if (audioLinkActive) {
            // Rebuild the remote audio element path after output-device change.
            piAudio.play().catch(() => {});
            await ensureAudioContext();
        }
    });
    testSpeakerBtn.addEventListener('click', testSelectedSpeaker);
    navigator.mediaDevices?.addEventListener?.('devicechange', populateAudioDevices);
    populateAudioDevices();

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
                deviceId: selectedMicConstraint(),
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true,
                channelCount: 1,
                sampleRate: 48000,
            },
            video: false,
        });
        await populateAudioDevices();

        remoteStream = new MediaStream();
        piAudio.srcObject = remoteStream;
        piAudio.autoplay = true;
        piAudio.playsInline = true;
        piAudio.muted = false;
        piAudio.volume = 1.0;
        await applySpeakerDevice();
        await ensureAudioContext();

        pc = new RTCPeerConnection({
            // LAN-only. No STUN/TURN needed; avoids depending on internet access.
            iceServers: [],
            bundlePolicy: 'max-bundle',
        });

        localStream.getAudioTracks().forEach(track => pc.addTrack(track, localStream));

        pc.ontrack = async (event) => {
            event.streams[0]?.getTracks().forEach(track => remoteStream.addTrack(track));
            try {
                await ensureAudioContext();
                // Route remote stream through Web Audio as well as the <audio> element.
                // This helps Chrome/macOS Bluetooth sinks that sometimes fail to start
                // a hidden media element but do play Web Audio reliably.
                if (!remoteSource && remoteStream.getAudioTracks().length > 0) {
                    remoteSource = audioCtx.createMediaStreamSource(remoteStream);
                    remoteGain = audioCtx.createGain();
                    remoteGain.gain.value = 1.0;
                    remoteSource.connect(remoteGain).connect(audioCtx.destination);
                }
                await piAudio.play();
            } catch (err) {
                console.warn('[webrtc] remote audio start failed', err);
                setStatus('remote audio failed: ' + (err.name || err.message));
            }
        };

        pc.onconnectionstatechange = () => {
            console.log('[webrtc] connection state', pc.connectionState);
            if (!audioLinkActive) return;
            if (['failed', 'closed'].includes(pc.connectionState)) {
                scheduleReconnect(`connection ${pc.connectionState}`);
            } else if (pc.connectionState === 'disconnected') {
                disconnectedSince = disconnectedSince || Date.now();
                setStatus('WebRTC briefly disconnected...');
            } else if (pc.connectionState === 'connected') {
                disconnectedSince = 0;
                reconnectAttempts = 0;
                setStatus('WebRTC 2-way audio connected');
            }
        };

        pc.oniceconnectionstatechange = () => {
            console.log('[webrtc] ice state', pc.iceConnectionState);
            if (!audioLinkActive) return;
            if (['failed', 'closed'].includes(pc.iceConnectionState)) {
                scheduleReconnect(`ice ${pc.iceConnectionState}`);
            } else if (pc.iceConnectionState === 'disconnected') {
                disconnectedSince = disconnectedSince || Date.now();
                setStatus('WebRTC briefly disconnected...');
            } else if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
                disconnectedSince = 0;
            }
        };

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
        if (!audioLinkActive && !autoAudio) return;
        audioLinkActive = true;
        setAudioUi(true);
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
            if (!pc || ['failed', 'closed'].includes(pc.connectionState)) {
                scheduleReconnect('watchdog');
            }
            if (pc && pc.connectionState === 'disconnected') {
                disconnectedSince = disconnectedSince || Date.now();
                if (Date.now() - disconnectedSince > 6000) {
                    scheduleReconnect('disconnected for 6s');
                }
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
        // If a previous auto-start failed before creating a peer, allow a manual
        // click to try again instead of getting stuck in "active" state.
        if (audioLinkActive && pc) return;
        audioLinkActive = true;
        reconnectAttempts = 0;
        disconnectedSince = 0;
        setAudioUi(true);
        setStatus('starting WebRTC audio...');
        try {
            await startWebrtcAudio();
            startWatchdog();
            setStatus('WebRTC 2-way audio connected');
        } catch (err) {
            console.error(err);
            stopWatchdog();
            await stopWebrtcAudio(false);
            audioLinkActive = false;
            setAudioUi(false);
            const name = err.name || '';
            const msg = err.message || name;
            setStatus('WebRTC audio failed: ' + msg);
            // Browsers often block mic/audio auto-start until a user gesture. Do
            // not spin reconnects for permission/gesture failures; let the Start
            // button recover it.
            if (!['NotAllowedError', 'NotFoundError', 'SecurityError', 'AbortError'].includes(name) && autoAudio) {
                scheduleReconnect(msg || 'initial connect failed');
            } else if (autoAudio) {
                setStatus('click start 2-way audio to allow mic/speaker');
            }
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
        if (remoteSource) {
            try { remoteSource.disconnect(); } catch (_) {}
            remoteSource = null;
        }
        if (remoteGain) {
            try { remoteGain.disconnect(); } catch (_) {}
            remoteGain = null;
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

    async function stopAudioLink(disableAuto = false) {
        if (disableAuto) {
            autoAudio = false;
            localStorage.setItem('spacehandsAutoAudio', '0');
        }
        audioLinkActive = false;
        stopWatchdog();
        await stopWebrtcAudio(true);
        setAudioUi(false);
        setStatus(`camera ← ${cleanHost(hostInput.value)}:8082`);
    }

    talkBtn.addEventListener('click', () => {
        autoAudio = true;
        localStorage.setItem('spacehandsAutoAudio', '1');
        setAudioUi(audioLinkActive);
        startAudioLink();
    });
    listenBtn.addEventListener('click', () => stopAudioLink(true));
    autoAudioBtn.addEventListener('click', () => {
        autoAudio = !autoAudio;
        localStorage.setItem('spacehandsAutoAudio', autoAudio ? '1' : '0');
        setAudioUi(audioLinkActive);
        if (autoAudio) startAudioLink();
        else stopAudioLink(true);
    });
    window.addEventListener('keydown', (e) => {
        if (e.code === 'Space' && !e.repeat) {
            e.preventDefault();
            audioLinkActive ? stopAudioLink(true) : startAudioLink();
        }
    });

    window.addEventListener('online', () => {
        if (autoAudio) scheduleReconnect('network online');
    });
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && autoAudio && !audioLinkActive) startAudioLink();
    });

    setAudioUi(false);
    connectCamera();

    // Default audio to on. Browsers may require one click for microphone/audio
    // permission; if autoplay is blocked the status will ask the operator to click
    // start once, then reconnects are automatic after that.
    setTimeout(() => {
        if (autoAudio) {
            startAudioLink().catch(err => {
                console.warn('[webrtc] auto-start failed', err);
                setStatus('click start 2-way audio to allow mic/speaker');
            });
        }
    }, 600);
})();
