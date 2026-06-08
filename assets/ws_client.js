'use strict';

window.instrumentState = {
    level: 0, progress: 0, phase: 0, hint: 'UNKNOWN',
    tempo: 120, instrument: 0, num_hands: 0, hands: [],
    fx: { reverb: 0.2, delay: 0 },
};
window.camBitmap = null;

(function () {
    const SSE_URL  = 'http://localhost:8081/state';
    const MJPEG_URL = 'http://localhost:8082/stream.mjpeg';

    // --- True SSE push stream at 30Hz from C++ ---
    function connectSSE() {
        const es = new EventSource('http://localhost:8081/state');
        es.onmessage = (e) => {
            try { Object.assign(window.instrumentState, JSON.parse(e.data)); } catch (_) {}
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
        console.log('[cam] using MJPEG stream from C++ binary');
        const CAM_POLL = 66;
        function poll() {
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

    // MJPEG first (hand-masked, processed by C++ binary)
    // Falls back to browser webcam only if MJPEG unavailable
    startMjpegPolling();

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
