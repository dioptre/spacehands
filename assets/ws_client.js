'use strict';

window.instrumentState = {
    level: 0, progress: 0, phase: 0, hint: 'UNKNOWN',
    tempo: 120, instrument: 0, num_hands: 0, hands: [],
    fx: { reverb: 0.2, delay: 0 },
};
window.camBitmap = null;

(function () {
    const host = window.apiHost || 'localhost';
    const SSE_URL  = `http://${host}:8081/state`;
    const MJPEG_URL = `http://${host}:8082/stream.mjpeg`;

    // --- Feature toggles (default off) ---
    window.showLabels = false;
    window.showHints  = false;

    function wireToggle(id, key, label) {
        const btn = document.getElementById(id);
        if (!btn) return;
        btn.addEventListener('click', () => {
            window[key] = !window[key];
            btn.textContent = label + ': ' + (window[key] ? 'on' : 'off');
            btn.style.color = window[key] ? 'rgba(200,200,255,1)' : 'rgba(200,200,255,0.4)';
            btn.style.borderColor = window[key] ? 'rgba(200,200,255,0.9)' : 'rgba(200,200,255,0.3)';
        });
    }
    wireToggle('toggle-labels', 'showLabels', 'labels');
    wireToggle('toggle-hints',  'showHints',  'hints');
    window.showPulse = false;
    wireToggle('toggle-pulse',  'showPulse',  'pulse');

    // --- Mirror toggle ---
    window.mirrorX = false;
    const mirrorBtn = document.getElementById('mirror-btn');
    if (mirrorBtn) {
        mirrorBtn.addEventListener('click', () => {
            window.mirrorX = !window.mirrorX;
            mirrorBtn.textContent = 'mirror: ' + (window.mirrorX ? 'on' : 'off');
            mirrorBtn.style.borderColor = window.mirrorX ? 'rgba(255,255,0,0.9)' : 'rgba(255,255,0,0.4)';
            mirrorBtn.style.color = window.mirrorX ? 'rgba(255,255,0,1)' : 'rgba(255,255,0,0.7)';
        });
    }

    // --- True SSE push stream at 30Hz from C++ ---
    function connectSSE() {
        const es = new EventSource(SSE_URL);
        es.onmessage = (e) => {
            try {
                const d = JSON.parse(e.data);
                // Apply mirror flip to hand X coords if enabled
                if (window.mirrorX && d.hands) {
                    d.hands = d.hands.map(h => Object.assign({}, h, { x: 1.0 - (h.x || 0) }));
                    if (d.cx !== undefined) d.cx = 1.0 - d.cx;
                }
                Object.assign(window.instrumentState, d);
            } catch (_) {}
        };
        es.onerror = () => { es.close(); setTimeout(connectSSE, 1000); };
    }
    connectSSE();

    // --- Camera source selection ---
    // Strategy:
    //   1. Always try browser getUserMedia first (works on Mac dev, has permission UI)
    //   2. If getUserMedia fails/denied, fall back to MJPEG polling (Pi with ToF camera)
    // On Pi, getUserMedia won't be available in kiosk mode, so MJPEG takes over.

    const offscreen = document.createElement('canvas');
    offscreen.width = 320; offscreen.height = 240;
    const ctx = offscreen.getContext('2d');

    function grayscale(imageData) {
        const d = imageData.data;
        for (let i = 0; i < d.length; i += 4) {
            const g = d[i] * 0.299 + d[i+1] * 0.587 + d[i+2] * 0.114;
            d[i] = d[i+1] = d[i+2] = g;
        }
        return imageData;
    }

    function bitmapFrom(source) {
        ctx.drawImage(source, 0, 0, 320, 240);
        const id = ctx.getImageData(0, 0, 320, 240);
        ctx.putImageData(grayscale(id), 0, 0);
        createImageBitmap(offscreen).then(bmp => {
            if (window.camBitmap) window.camBitmap.close();
            window.camBitmap = bmp;
        });
    }

    function startMjpegPolling() {
        console.log('[cam] MJPEG polling started');
        const CAM_POLL = 66;
        function poll() {
            if (!mjpegActive) return; // stop if disabled
            const img = new Image();
            img.crossOrigin = 'anonymous';
            img.onload  = () => { bitmapFrom(img); setTimeout(poll, CAM_POLL); };
            img.onerror = () => setTimeout(poll, CAM_POLL * 4);
            img.src = MJPEG_URL + '?t=' + Date.now();
        }
        poll();
    }

    function startBrowserWebcam() {
        console.log('[cam] requesting webcam...');
        navigator.mediaDevices.getUserMedia({ video: true })
        .then(stream => {
            const video = document.createElement('video');
            video.srcObject = stream;
            video.setAttribute('playsinline', '');
            video.muted = true;
            video.play();
            console.log('[cam] webcam stream granted, waiting for video...');
            function grabFrame() {
                if (video.readyState >= 2 && video.videoWidth > 0) {
                    bitmapFrom(video);
                    console.log('[cam] first frame captured: ' + video.videoWidth + 'x' + video.videoHeight);
                }
                setTimeout(grabFrame, 66);
            }
            // Start grabbing immediately, don't wait for 'playing' event
            setTimeout(grabFrame, 500);
        }).catch(err => {
            console.warn('[cam] getUserMedia failed:', err.name, err.message);
            console.warn('[cam] falling back to MJPEG');
            startMjpegPolling();
        });
    }

    // Expose MJPEG toggle so index.html can start/stop on shader switch
    let mjpegActive = false;
    window.startMjpeg = function(enable) {
        if (enable && !mjpegActive) { mjpegActive = true; startMjpegPolling(); }
        if (!enable) { mjpegActive = false; window.camBitmap = null; }
    };

    // Start on load if frequency shader active
    const params = new URLSearchParams(window.location.search);
    if ((params.get('shader') || 'wormhole') === 'frequency') {
        window.startMjpeg(true);
    }

    // --- Debug overlay ---
    const dbg = document.getElementById('debug');
    if (dbg) {
        setInterval(() => {
            const s = window.instrumentState;
            dbg.textContent =
                `level:${s.level}  progress:${s.progress.toFixed(2)}\n` +
                `hands:${s.num_hands}  tempo:${s.tempo.toFixed(0)}bpm\n` +
                `hint:${s.hint}`;
        }, 200);
    }
})();
