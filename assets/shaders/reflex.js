'use strict';
// REFLEX — DDR-style 3D rhythm game played against your own silhouette reflection.
// Syncs to TidalCycles via C++ server SSE spawns and /orb triggers.

(function () {
    // 1. Ensure importmap exists in head
    if (!document.querySelector('script[type="importmap"]')) {
        const importMap = document.createElement('script');
        importMap.type = 'importmap';
        importMap.textContent = JSON.stringify({
            imports: {
                "three": "https://cdn.jsdelivr.net/npm/three@0.165.0/build/three.module.js",
                "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.165.0/examples/jsm/"
            }
        });
        document.head.appendChild(importMap);
    }

    // 2. Inject CSS for game UI
    const styleEl = document.createElement('style');
    styleEl.id = 'reflex-styles';
    styleEl.textContent = `
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;600;900&display=swap');
        #reflex-ui {
            position: fixed; inset: 0; pointer-events: none;
            font-family: 'Outfit', sans-serif; color: #fff; z-index: 10;
        }
        .reflex-interactive { pointer-events: auto; }
        
        /* Song Selection Menu */
        #reflex-menu {
            position: absolute; inset: 0;
            display: flex; flex-direction: column; align-items: center; justify-content: center;
            background: rgba(5, 2, 15, 0.85); backdrop-filter: blur(15px);
            transition: opacity 0.5s ease;
        }
        .menu-title {
            font-size: 3.5rem; font-weight: 900; letter-spacing: 2px;
            margin-bottom: 2rem; text-transform: uppercase;
            background: linear-gradient(45deg, #ff007f, #00f0ff);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
            text-shadow: 0 0 30px rgba(255, 0, 127, 0.3);
        }
        .song-list {
            display: flex; flex-direction: column; gap: 15px; width: 380px;
        }
        .song-card {
            background: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 12px; padding: 15px; cursor: pointer; text-align: center;
            transition: all 0.25s ease;
        }
        .song-card:hover {
            background: rgba(255, 0, 127, 0.15); border-color: #ff007f;
            transform: translateY(-2px); box-shadow: 0 5px 15px rgba(255, 0, 127, 0.2);
        }
        .song-title { font-size: 1.4rem; font-weight: 600; margin-bottom: 4px; }
        .song-meta { font-size: 0.9rem; opacity: 0.6; font-family: monospace; }

        /* Gameplay HUD */
        #reflex-hud {
            position: absolute; top: 20px; left: 20px; right: 20px;
            display: flex; justify-content: space-between; align-items: flex-start;
            opacity: 0; transition: opacity 0.5s ease;
        }
        .hud-panel {
            background: rgba(0, 0, 0, 0.6); border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 10px; padding: 10px 20px; backdrop-filter: blur(5px);
        }
        .score-val { font-size: 2rem; font-weight: 900; color: #00f0ff; }
        .combo-val { font-size: 1.6rem; font-weight: 900; color: #ff007f; text-align: right; }
        
        /* Hit Rating Popups */
        #reflex-feedback {
            position: absolute; top: 35%; left: 50%; transform: translate(-50%, -50%);
            font-size: 3rem; font-weight: 900; text-transform: uppercase;
            letter-spacing: 4px; pointer-events: none; opacity: 0;
            transition: transform 0.1s ease, opacity 0.1s ease;
        }
        .rating-perfect { color: #00f0ff; text-shadow: 0 0 20px rgba(0, 240, 255, 0.8); }
        .rating-great { color: #00ff7f; text-shadow: 0 0 20px rgba(0, 255, 127, 0.8); }
        .rating-good { color: #ffeb3b; text-shadow: 0 0 20px rgba(255, 235, 59, 0.8); }
        .rating-miss { color: #f44336; text-shadow: 0 0 20px rgba(244, 67, 54, 0.8); }

        /* End Screen */
        #reflex-end {
            position: absolute; inset: 0;
            display: flex; flex-direction: column; align-items: center; justify-content: center;
            background: rgba(5, 2, 15, 0.9); backdrop-filter: blur(15px);
            opacity: 0; pointer-events: none; transition: opacity 0.5s ease;
        }
        .end-rank {
            font-size: 8rem; font-weight: 900; line-height: 1;
            background: linear-gradient(135deg, #ffd700, #ff8800);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
            margin-bottom: 1rem;
        }
        .end-score { font-size: 2.2rem; font-weight: 600; margin-bottom: 2rem; color: #00f0ff; }
        .btn-restart {
            background: linear-gradient(45deg, #ff007f, #7f00ff);
            border: none; border-radius: 30px; color: #fff;
            padding: 12px 36px; font-size: 1.2rem; font-weight: 600;
            cursor: pointer; transition: all 0.25s ease; box-shadow: 0 5px 20px rgba(255, 0, 127, 0.3);
        }
        .btn-restart:hover {
            transform: scale(1.05); box-shadow: 0 8px 25px rgba(255, 0, 127, 0.5);
        }
    `;
    document.head.appendChild(styleEl);

    // 3. Create UI container
    const uiContainer = document.createElement('div');
    uiContainer.id = 'reflex-ui';
    uiContainer.innerHTML = `
        <div id="reflex-menu" class="reflex-interactive">
            <div class="menu-title">Reflex Rhythm</div>
            <div class="song-list">
                <div class="song-card" data-song="1" data-bpm="120" data-cps="0.5">
                    <div class="song-title">Seven Nation Army</div>
                    <div class="song-meta">The White Stripes • 120 BPM • Tier 1-4</div>
                </div>
                <div class="song-card" data-song="2" data-bpm="130" data-cps="0.5417">
                    <div class="song-title">Blue Monday</div>
                    <div class="song-meta">New Order • 130 BPM • High Speed Synth</div>
                </div>
                <div class="song-card" data-song="3" data-bpm="121" data-cps="0.5042">
                    <div class="song-title">Around the World</div>
                    <div class="song-meta">Daft Punk • 121 BPM • Funky House Beat</div>
                </div>
                <div class="song-card" data-song="4" data-bpm="111" data-cps="0.4625">
                    <div class="song-title">Da Funk</div>
                    <div class="song-meta">Daft Punk • 111 BPM • Heavy G-Funk</div>
                </div>
            </div>
        </div>
        <div id="reflex-hud">
            <div class="hud-panel">
                <div style="font-size:0.8rem;opacity:0.6;text-transform:uppercase;">Score</div>
                <div id="hud-score" class="score-val">000,000</div>
            </div>
            <div id="hud-song-info" class="hud-panel" style="text-align:center;">
                <div id="hud-title" style="font-size:1.1rem;font-weight:600;">Seven Nation Army</div>
                <div id="hud-bpm" style="font-size:0.8rem;opacity:0.6;">120 BPM</div>
            </div>
            <div class="hud-panel">
                <div style="font-size:0.8rem;opacity:0.6;text-transform:uppercase;text-align:right;">Combo</div>
                <div id="hud-combo" class="combo-val">0</div>
            </div>
        </div>
        <div id="reflex-feedback">PERFECT</div>
        <div id="reflex-end">
            <div style="font-size:1.5rem;opacity:0.7;margin-bottom:0.5rem;text-transform:uppercase;">Reflex Rank</div>
            <div id="end-rank-val" class="end-rank">S</div>
            <div id="end-score-val" class="end-score">Score: 120,400</div>
            <button id="end-restart-btn" class="btn-restart reflex-interactive">Play Again</button>
        </div>
    `;
    document.body.appendChild(uiContainer);

    // 4. Inject module script for 3D logic
    const moduleScript = document.createElement('script');
    moduleScript.id = 'reflex-module-script';
    moduleScript.type = 'module';
    moduleScript.textContent = `
import * as THREE from 'three';

const canvas = document.getElementById('c');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(1.0);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.1;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x020005);

const camera = new THREE.PerspectiveCamera(50, canvas.clientWidth / canvas.clientHeight, 0.01, 100);
camera.position.set(0, 0, 3.2);

// ---- Lights ----
const keyLight = new THREE.DirectionalLight(0x00f0ff, 2.5);
keyLight.position.set(1, 2, 2); scene.add(keyLight);

const rimLight = new THREE.DirectionalLight(0xff007f, 2.0);
rimLight.position.set(-2, -1, -2); scene.add(rimLight);

const ambientLight = new THREE.AmbientLight(0x110822, 1.5);
scene.add(ambientLight);

// ---- Camera Texture ----
const camTex = new THREE.Texture();
camTex.minFilter = THREE.LinearFilter;
camTex.magFilter = THREE.LinearFilter;
camTex.wrapS = THREE.ClampToEdgeWrapping;
camTex.wrapT = THREE.ClampToEdgeWrapping;

// Background mirror plane with custom neon silhouette shader
// Restricts X to the middle 1/3 of the projection (0.33 to 0.66)
const bgVert = \`
    varying vec2 vUv;
    void main() {
        vUv = uv;
        gl_Position = vec4(position, 1.0);
    }
\`;

const bgFrag = \`
    uniform sampler2D uCam;
    uniform float uTime;
    uniform float uBeatPulse;
    uniform float uHitFlash;
    varying vec2 vUv;

    void main() {
        // Horizontal crop to middle 1/3 of screen width
        float minX = 0.33;
        float maxX = 0.66;
        float fade = smoothstep(0.0, 0.02, vUv.x - minX) * (1.0 - smoothstep(0.0, 0.02, vUv.x - maxX));

        // Background color wash (violet gradient)
        vec3 bgCol = mix(vec3(0.02, 0.01, 0.05), vec3(0.08, 0.02, 0.15), vUv.y);
        bgCol += vec3(0.03, 0.0, 0.08) * uBeatPulse;

        // Silhouette processing
        float rawLuma = texture(uCam, vUv).r;

        // Edge detection on camera bitmap
        float val = rawLuma;
        float edge = 0.0;
        float delta = 1.0 / 256.0;
        float val_l = texture(uCam, vUv + vec2(-delta, 0.0)).r;
        float val_r = texture(uCam, vUv + vec2(delta, 0.0)).r;
        float val_u = texture(uCam, vUv + vec2(0.0, -delta)).r;
        float val_d = texture(uCam, vUv + vec2(0.0, delta)).r;
        edge = max(edge, abs(val - val_l));
        edge = max(edge, abs(val - val_r));
        edge = max(edge, abs(val - val_u));
        edge = max(edge, abs(val - val_d));
        edge = smoothstep(0.04, 0.12, edge) * fade;

        // Colors
        vec3 neonColor = mix(vec3(0.8, 0.0, 0.5), vec3(0.0, 0.9, 1.0), 0.5 + 0.5 * sin(uTime * 1.5));
        vec3 edgeCol = edge * neonColor * (1.5 + uHitFlash * 3.0);
        
        // Translucent silhouette fill
        vec3 silFill = vec3(rawLuma * 0.1, rawLuma * 0.02, rawLuma * 0.18) * fade;
        silFill += vec3(rawLuma * 0.1) * neonColor * uHitFlash;

        vec3 finalCol = mix(bgCol, edgeCol + silFill, rawLuma * 0.8 * fade);
        gl_FragColor = vec4(finalCol, 1.0);
    }
\`;

const bgMat = new THREE.ShaderMaterial({
    vertexShader: bgVert,
    fragmentShader: bgFrag,
    uniforms: {
        uCam: { value: camTex },
        uTime: { value: 0 },
        uBeatPulse: { value: 0 },
        uHitFlash: { value: 0 }
    },
    depthWrite: false
});

const bgMesh = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), bgMat);
scene.add(bgMesh);

// ---- Hand visualizers (Holographic Orbs) ----
const handGroup = new THREE.Group();
scene.add(handGroup);

const handMaterial = new THREE.MeshBasicMaterial({
    color: 0x00f0ff,
    wireframe: true,
    transparent: true,
    opacity: 0.8
});
const handGeo = new THREE.SphereGeometry(0.12, 16, 12);
const leftHandVisual = new THREE.Mesh(handGeo, handMaterial.clone());
const rightHandVisual = new THREE.Mesh(handGeo, handMaterial.clone());
leftHandVisual.material.color.setHex(0x00f0ff);
rightHandVisual.material.color.setHex(0x00ff88);
leftHandVisual.visible = false;
rightHandVisual.visible = false;
handGroup.add(leftHandVisual);
handGroup.add(rightHandVisual);

// ---- Particle system for hits ----
const particleCount = 150;
const particleGeometry = new THREE.BufferGeometry();
const particlePos = new Float32Array(particleCount * 3);
const particleVel = new Float32Array(particleCount * 3);
const particleCol = new Float32Array(particleCount * 3);

particleGeometry.setAttribute('position', new THREE.BufferAttribute(particlePos, 3));
const particleMaterial = new THREE.PointsMaterial({
    size: 0.05,
    vertexColors: true,
    transparent: true,
    opacity: 1.0,
    blending: THREE.AdditiveBlending
});
const particleSystem = new THREE.Points(particleGeometry, particleMaterial);
scene.add(particleSystem);

const particles = [];
function spawnBurst(pos, colorHex) {
    const col = new THREE.Color(colorHex);
    for (let i = 0; i < 25; i++) {
        particles.push({
            x: pos.x, y: pos.y, z: pos.z,
            vx: (Math.random() - 0.5) * 2.5,
            vy: (Math.random() - 0.5) * 2.5,
            vz: (Math.random() - 0.5) * 2.5,
            r: col.r, g: col.g, b: col.b,
            life: 1.0,
            decay: 1.2 + Math.random() * 1.5
        });
    }
}

function updateParticles(dt) {
    const posArr = [];
    const colArr = [];
    for (let i = particles.length - 1; i >= 0; i--) {
        const p = particles[i];
        p.life -= dt * p.decay;
        if (p.life <= 0) {
            particles.splice(i, 1);
            continue;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        // decelerate
        p.vx *= 0.95; p.vy *= 0.95; p.vz *= 0.95;
        posArr.push(p.x, p.y, p.z);
        colArr.push(p.r * p.life, p.g * p.life, p.b * p.life);
    }
    const emptyCount = particleCount - posArr.length / 3;
    for (let i = 0; i < emptyCount; i++) {
        posArr.push(999, 999, 999);
        colArr.push(0, 0, 0);
    }
    particleGeometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(posArr), 3));
    particleGeometry.setAttribute('color', new THREE.BufferAttribute(new Float32Array(colArr), 3));
    particleGeometry.attributes.position.needsUpdate = true;
    particleGeometry.attributes.color.needsUpdate = true;
}

// ---- Targets ----
const targetGroup = new THREE.Group();
scene.add(targetGroup);

const ringGeo = new THREE.RingGeometry(0.12, 0.14, 32);
const targetState = []; // active gameplay targets

function createTargetMesh(type, hand) {
    const group = new THREE.Group();

    // Base target ring
    let color = 0x00f0ff; // single left
    if (hand === 1) color = 0x00ff88; // single right
    if (hand === 2) color = 0xd500f9; // both (dual)
    if (type === 2) color = 0xffea00; // push (yellow)
    if (type === 3) color = 0xff6d00; // pull (orange)
    if (type === 4) color = 0xff0055; // pose (pink)

    const baseRing = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({ color, side: THREE.DoubleSide, transparent: true, opacity: 0.4 }));
    group.add(baseRing);

    // Inner core sphere
    const core = new THREE.Mesh(new THREE.SphereGeometry(0.04, 16, 12), new THREE.MeshBasicMaterial({ color }));
    group.add(core);

    // Dynamic approach ring
    const approachRing = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({ color, side: THREE.DoubleSide, transparent: true, opacity: 0.9 }));
    group.add(approachRing);
    group.userData = { baseRing, core, approachRing, color, type, hand };

    // Extra decoration for Push/Pull/Pose
    if (type === 2) {
        // Push arrow indicators pointing forward (Z negative)
        const arrow = new THREE.Mesh(new THREE.ConeGeometry(0.03, 0.1, 8), new THREE.MeshBasicMaterial({ color }));
        arrow.rotation.x = -Math.PI / 2;
        arrow.position.z = 0.08;
        group.add(arrow);
    } else if (type === 3) {
        // Pull arrow indicators pointing backward (Z positive)
        const arrow = new THREE.Mesh(new THREE.ConeGeometry(0.03, 0.1, 8), new THREE.MeshBasicMaterial({ color }));
        arrow.rotation.x = Math.PI / 2;
        arrow.position.z = -0.08;
        group.add(arrow);
    } else if (type === 4) {
        // Pose stick figure guide
        const lineMat = new THREE.LineBasicMaterial({ color, linewidth: 2 });
        const lineGeo = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(-0.15, 0.1, 0), new THREE.Vector3(0.15, 0.1, 0), // arms
            new THREE.Vector3(0, 0.15, 0), new THREE.Vector3(0, -0.15, 0), // torso
            new THREE.Vector3(-0.1, -0.25, 0), new THREE.Vector3(0, -0.15, 0),
            new THREE.Vector3(0.1, -0.25, 0), new THREE.Vector3(0, -0.15, 0) // legs
        ]);
        const skeleton = new THREE.LineSegments(lineGeo, lineMat);
        group.add(skeleton);
    }

    targetGroup.add(group);
    return group;
}

// Map from 0-1 screen coordinates to 3D world coordinates
// Centers inside the middle 1/3 of the projection screen
function toWorldCoords(x, y, z) {
    // Map tracking X [0.33, 0.66] into Three.js bounds
    // Center of mirror is X=0.5. Map 0.33 to -0.6 and 0.66 to 0.6
    const wx = (x - 0.5) * 3.6; 
    const wy = -(y * 2.0 - 1.0) * 1.3;
    const wz = -z * 1.5; 
    return new THREE.Vector3(wx, wy, wz);
}

// ---- Game state variables ----
let gameActive = false;
let score = 0;
let combo = 0;
let maxCombo = 0;
let totalHits = 0;
let hitSuccess = 0;
let activeSong = null;
let songStartT = 0;
let currentBpm = 120;
let currentCps = 0.5;
let hitPulseScale = 0;
let hitFlashAmount = 0;

// Set up UI elements
const reflexMenu = document.getElementById('reflex-menu');
const reflexHud = document.getElementById('reflex-hud');
const reflexFeedback = document.getElementById('reflex-feedback');
const reflexEnd = document.getElementById('reflex-end');

const hudScore = document.getElementById('hud-score');
const hudCombo = document.getElementById('hud-combo');
const hudTitle = document.getElementById('hud-title');
const hudBpm = document.getElementById('hud-bpm');
const endRankVal = document.getElementById('end-rank-val');
const endScoreVal = document.getElementById('end-score-val');

function showFeedback(text, ratingClass) {
    reflexFeedback.textContent = text;
    reflexFeedback.className = ratingClass;
    reflexFeedback.style.opacity = 1;
    reflexFeedback.style.transform = 'translate(-50%, -50%) scale(1.2)';
    
    setTimeout(() => {
        reflexFeedback.style.opacity = 0;
        reflexFeedback.style.transform = 'translate(-50%, -50%) scale(1.0)';
    }, 400);
}

async function sendOrb(data) {
    try {
        await fetch(\`http://\${window.apiHost}:8080/orb\`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    } catch (_) {}
}

async function playHitNote(midi) {
    try {
        await fetch(\`http://\${window.apiHost}:8080/note\`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ midi: midi, amp: 1.0, decay: 0.5 })
        });
    } catch (_) {}
}

function startSong(songId, bpm, cps) {
    console.log('[reflex] starting song', songId, bpm, cps);
    activeSong = songId;
    currentBpm = bpm;
    currentCps = cps;
    score = 0;
    combo = 0;
    maxCombo = 0;
    totalHits = 0;
    hitSuccess = 0;

    hudScore.textContent = '000,000';
    hudCombo.textContent = '0';
    
    const songNames = {
        '1': 'Seven Nation Army',
        '2': 'Blue Monday',
        '3': 'Around the World',
        '4': 'Da Funk'
    };
    hudTitle.textContent = songNames[songId] || 'Reflex Song';
    hudBpm.textContent = bpm + ' BPM';

    // Clear previous targets
    targetState.forEach(t => targetGroup.remove(t.mesh));
    targetState.length = 0;

    // Transition UI
    reflexMenu.style.opacity = 0;
    setTimeout(() => { reflexMenu.style.display = 'none'; }, 500);
    reflexHud.style.opacity = 1;
    reflexEnd.style.opacity = 0;
    reflexEnd.style.pointerEvents = 'none';

    // Send OSC starting triggers to Tidal
    // First clear transformation if active
    sendOrb({ ctrl: 'transformation_active', value: 0.0 });
    
    // Set reflex parameters
    sendOrb({ ctrl: 'reflex_active', value: 1.0 });
    sendOrb({ ctrl: 'reflex_cps', value: parseFloat(cps) });
    for (let i = 1; i <= 4; i++) {
        sendOrb({ ctrl: 'reflex_song_' + i, value: (i == songId) ? 1.0 : 0.0 });
    }

    songStartT = performance.now();
    gameActive = true;
}

function stopGame() {
    console.log('[reflex] stopping game');
    gameActive = false;
    
    // Send mutes to Tidal
    sendOrb({ ctrl: 'reflex_active', value: 0.0 });
    for (let i = 1; i <= 4; i++) {
        sendOrb({ ctrl: 'reflex_song_' + i, value: 0.0 });
    }

    // Clear all targets
    targetState.forEach(t => targetGroup.remove(t.mesh));
    targetState.length = 0;

    // Transition UI
    reflexHud.style.opacity = 0;
    
    // Calculate final rank
    let accuracy = totalHits > 0 ? (hitSuccess / totalHits) : 0;
    let rank = 'D';
    if (accuracy >= 0.95) rank = 'S';
    else if (accuracy >= 0.85) rank = 'A';
    else if (accuracy >= 0.72) rank = 'B';
    else if (accuracy >= 0.58) rank = 'C';

    endRankVal.textContent = rank;
    endScoreVal.textContent = 'Score: ' + score.toLocaleString() + ' (Max Combo: ' + maxCombo + ')';
    
    // Full-body flash at end of round
    hitFlashAmount = 1.8;

    reflexEnd.style.display = 'flex';
    setTimeout(() => {
        reflexEnd.style.opacity = 1;
        reflexEnd.style.pointerEvents = 'auto';
    }, 100);
}

// Bind UI click events
document.querySelectorAll('.song-card').forEach(card => {
    card.addEventListener('click', () => {
        const songId = card.dataset.song;
        const bpm = parseInt(card.dataset.bpm, 10);
        const cps = parseFloat(card.dataset.cps);
        startSong(songId, bpm, cps);
    });
});

document.getElementById('end-restart-btn').addEventListener('click', () => {
    reflexEnd.style.opacity = 0;
    reflexEnd.style.pointerEvents = 'none';
    setTimeout(() => {
        reflexEnd.style.display = 'none';
        reflexMenu.style.display = 'flex';
        reflexMenu.style.opacity = 1;
    }, 500);
});

// ---- Gameplay hit checking ----
// Check if hands match a target in 3D coordinate space
function checkTargetHit(t, hands, now) {
    if (hands.length === 0) return false;

    // Tolerance values
    const xyTol = 0.22;
    const zTol = 0.20;

    // 1. Single target check
    if (t.type === 0) {
        for (const h of hands) {
            const hWorld = toWorldCoords(h.x, h.y, h.z);
            const distXY = Math.sqrt(Math.pow(hWorld.x - t.pos.x, 2) + Math.pow(hWorld.y - t.pos.y, 2));
            const distZ = Math.abs(hWorld.z - t.pos.z);
            if (distXY < xyTol && distZ < zTol) return true;
        }
    }
    // 2. Dual target check (requires both hands at targets)
    else if (t.type === 1) {
        if (hands.length < 2) return false;
        let hit1 = false, hit2 = false;
        for (const h of hands) {
            const hWorld = toWorldCoords(h.x, h.y, h.z);
            const d1XY = Math.sqrt(Math.pow(hWorld.x - t.pos1.x, 2) + Math.pow(hWorld.y - t.pos1.y, 2));
            const d1Z = Math.abs(hWorld.z - t.pos1.z);
            const d2XY = Math.sqrt(Math.pow(hWorld.x - t.pos2.x, 2) + Math.pow(hWorld.y - t.pos2.y, 2));
            const d2Z = Math.abs(hWorld.z - t.pos2.z);
            
            if (d1XY < xyTol && d1Z < zTol) hit1 = true;
            if (d2XY < xyTol && d2Z < zTol) hit2 = true;
        }
        return hit1 && hit2;
    }
    // 3. Push target check (requires deep Z)
    else if (t.type === 2) {
        for (const h of hands) {
            const hWorld = toWorldCoords(h.x, h.y, h.z);
            const distXY = Math.sqrt(Math.pow(hWorld.x - t.pos.x, 2) + Math.pow(hWorld.y - t.pos.y, 2));
            // Push must reach deep (near mirror: z is small)
            if (distXY < xyTol && h.z < 0.32) return true;
        }
    }
    // 4. Pull target check (requires shallow Z)
    else if (t.type === 3) {
        for (const h of hands) {
            const hWorld = toWorldCoords(h.x, h.y, h.z);
            const distXY = Math.sqrt(Math.pow(hWorld.x - t.pos.x, 2) + Math.pow(hWorld.y - t.pos.y, 2));
            // Pull must stay back (z is large)
            if (distXY < xyTol && h.z > 0.58) return true;
        }
    }
    // 5. Pose target check (requires specific arm alignments)
    else if (t.type === 4) {
        // Wide arms: left hand left (X < 0.38), right hand right (X > 0.62)
        if (hands.length >= 2) {
            let leftOk = false, rightOk = false;
            for (const h of hands) {
                if (h.x < 0.38) leftOk = true;
                if (h.x > 0.62) rightOk = true;
            }
            if (leftOk && rightOk) return true;
        }
    }

    return false;
}

// Map note values from Tidal to MIDI note offsets for direct hit feedback
const REFLEX_MIDI_NOTES = [60, 64, 67, 72, 76]; // C chord notes

// ---- Main Render Loop ----
const clock = new THREE.Clock();
let lastTime = 0;

function animate() {
    requestAnimationFrame(animate);
    const dt = Math.min(clock.getDelta(), 0.05);
    const now = performance.now();

    // 1. Upload camera feed bitmap to background texture
    if (window.camBitmap) {
        camTex.image = window.camBitmap;
        camTex.needsUpdate = true;
    }

    // 2. Read WebSocket state
    const st = window.instrumentState;
    const hands = (st && st.hands) || [];
    
    // Update camera Edge Shader Uniforms
    bgMat.uniforms.uTime.value = clock.getElapsedTime();
    hitPulseScale = Math.max(0, hitPulseScale - dt * 2.5);
    bgMat.uniforms.uBeatPulse.value = hitPulseScale;
    hitFlashAmount = Math.max(0, hitFlashAmount - dt * 2.0);
    bgMat.uniforms.uHitFlash.value = hitFlashAmount;

    // Render hand guides
    if (hands[0]) {
        const hp = toWorldCoords(hands[0].x, hands[0].y, hands[0].z);
        leftHandVisual.position.copy(hp);
        leftHandVisual.visible = true;
    } else { leftHandVisual.visible = false; }

    if (hands[1]) {
        const hp = toWorldCoords(hands[1].x, hands[1].y, hands[1].z);
        rightHandVisual.position.copy(hp);
        rightHandVisual.visible = true;
    } else { rightHandVisual.visible = false; }

    // 3. Spawns listening from C++ OSC receiver
    if (gameActive && st && st.spawns && st.spawns.length > 0) {
        st.spawns.forEach(s => {
            const beatDurationMs = (60 / currentBpm) * 1000;
            // Target approaches hit plane over 2 beats (constant)
            const target = {
                type: s.type,
                hand: s.hand,
                spawnTime: now,
                hitTime: now + beatDurationMs * 2,
                hit: false,
                missed: false,
                mesh: createTargetMesh(s.type, s.hand)
            };

            if (s.type === 1) {
                // Dual
                target.pos1 = toWorldCoords(s.x - 0.15, s.y, s.z);
                target.pos2 = toWorldCoords(s.x + 0.15, s.y, s.z);
            } else {
                target.pos = toWorldCoords(s.x, s.y, s.z);
            }
            targetState.push(target);
        });
        // Clear consumed spawns on client side
        st.spawns = [];
    }

    // 4. Update targets approach and hit planes
    if (gameActive) {
        // Song duration check (80 seconds max)
        if (now - songStartT > 80000) {
            stopGame();
        }

        for (let i = targetState.length - 1; i >= 0; i--) {
            const t = targetState[i];
            const elapsed = now - t.spawnTime;
            const duration = t.hitTime - t.spawnTime;
            const progress = Math.min(1.0, elapsed / duration);

            if (t.hit) {
                // target already hit, fading out
                t.mesh.scale.addScalar(dt * 3.0);
                t.mesh.traverse(child => { if (child.material) child.material.opacity -= dt * 5.0; });
                if (t.mesh.userData.approachRing.material.opacity <= 0) {
                    targetGroup.remove(t.mesh);
                    targetState.splice(i, 1);
                }
                continue;
            }

            if (t.missed) {
                // target missed, fading out slowly
                t.mesh.traverse(child => { if (child.material) child.material.opacity -= dt * 2.5; });
                if (t.mesh.userData.approachRing.material.opacity <= 0) {
                    targetGroup.remove(t.mesh);
                    targetState.splice(i, 1);
                }
                continue;
            }

            // Position and scale approach
            if (t.type === 1) {
                // Dual targets interpolate both nodes
                const startPos1 = t.pos1.clone().setZ(t.pos1.z - 0.8);
                const startPos2 = t.pos2.clone().setZ(t.pos2.z - 0.8);
                const curPos1 = new THREE.Vector3().lerpVectors(startPos1, t.pos1, progress);
                // Draw link line
                t.mesh.position.lerpVectors(startPos1, t.pos1, progress); // mesh root
            } else {
                const startPos = t.pos.clone().setZ(t.pos.z - 0.8);
                t.mesh.position.lerpVectors(startPos, t.pos, progress);
            }

            // Outer approach ring shrinks to meet inner ring at hit time (progress=1.0)
            const approachScale = 3.0 - (progress * 2.0);
            t.mesh.userData.approachRing.scale.setScalar(approachScale);
            t.mesh.userData.approachRing.material.opacity = progress * 0.9;

            // Pulsing beat effect on core
            const pulse = 1.0 + 0.15 * Math.sin(progress * Math.PI * 4);
            t.mesh.userData.core.scale.setScalar(pulse);

            // Check hit window (±180ms)
            const timeDiff = now - t.hitTime; // negative = early, positive = late
            
            if (Math.abs(timeDiff) <= 180) {
                if (checkTargetHit(t, hands, now)) {
                    t.hit = true;
                    totalHits++;
                    hitSuccess++;
                    combo++;
                    if (combo > maxCombo) maxCombo = combo;
                    
                    // Score based on accuracy
                    let scoreAdd = 50;
                    let ratingText = 'GOOD';
                    let ratingClass = 'rating-good';
                    if (Math.abs(timeDiff) < 80) {
                        scoreAdd = 100;
                        ratingText = 'PERFECT';
                        ratingClass = 'rating-perfect';
                    } else if (Math.abs(timeDiff) < 130) {
                        scoreAdd = 80;
                        ratingText = 'GREAT';
                        ratingClass = 'rating-great';
                    }
                    
                    score += scoreAdd;
                    hudScore.textContent = score.toLocaleString().padStart(7, '0');
                    hudCombo.textContent = combo;

                    showFeedback(ratingText, ratingClass);

                    // Immediate direct MIDI synth trigger for audio response
                    const noteMidi = REFLEX_MIDI_NOTES[Math.floor(Math.random() * REFLEX_MIDI_NOTES.length)];
                    playHitNote(noteMidi);

                    // Visual sparks
                    const targetCenter = t.pos || t.pos1;
                    spawnBurst(targetCenter, t.mesh.userData.color);
                    
                    // Edge flare
                    hitPulseScale = 1.2;
                    hitFlashAmount = 0.5;
                }
            } else if (timeDiff > 180) {
                // Missed!
                t.missed = true;
                totalHits++;
                combo = 0;
                hudCombo.textContent = '0';
                showFeedback('MISS', 'rating-miss');
            }
        }
    }

    // 5. Update systems
    updateParticles(dt);
    
    // Resize checks
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (canvas.width !== w || canvas.height !== h) {
        renderer.setSize(w, h, false);
        camera.aspect = w / h;
        camera.updateProjectionMatrix();
    }

    // Raycaster for mouse testing
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    window.addEventListener('mousedown', (event) => {
        if (!gameActive) return;
        
        // Convert client coordinates to normalized device coordinates (-1 to +1)
        mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
        mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;
        
        raycaster.setFromCamera(mouse, camera);
        
        // Check intersections against targets
        const intersects = raycaster.intersectObjects(targetGroup.children, true);
        if (intersects.length > 0) {
            // Find the top-level group for this target
            let root = intersects[0].object;
            while (root.parent && root.parent !== targetGroup) {
                root = root.parent;
            }
            
            const target = targetState.find(t => t.mesh === root);
            if (target && !target.hit && !target.missed) {
                target.hit = true;
                totalHits++;
                hitSuccess++;
                combo++;
                if (combo > maxCombo) maxCombo = combo;
                
                score += 100;
                hudScore.textContent = score.toLocaleString().padStart(7, '0');
                hudCombo.textContent = combo;
                
                showFeedback('PERFECT', 'rating-perfect');
                
                const noteMidi = REFLEX_MIDI_NOTES[Math.floor(Math.random() * REFLEX_MIDI_NOTES.length)];
                playHitNote(noteMidi);
                
                const center = target.pos || target.pos1;
                spawnBurst(center, target.mesh.userData.color);
                
                hitPulseScale = 1.2;
                hitFlashAmount = 0.5;
            }
        }
    });

    renderer.render(scene, camera);
}

// Start animate loop
animate();
    `;
    document.body.appendChild(moduleScript);

    // 5. Register cleanup handler to destroy UI on shader change
    window.addEventListener('unload', cleanupReflex);
    
    // Custom function injected into window so switcher can clean up
    window.cleanupReflex = cleanupReflex;
    
    function cleanupReflex() {
        console.log('[reflex] cleaning up');
        // Stop any active song mutes
        sendOrb({ ctrl: 'reflex_active', value: 0.0 });
        for (let i = 1; i <= 4; i++) {
            sendOrb({ ctrl: 'reflex_song_' + i, value: 0.0 });
        }

        const ui = document.getElementById('reflex-ui');
        if (ui) ui.remove();
        const styles = document.getElementById('reflex-styles');
        if (styles) styles.remove();
        const modScript = document.getElementById('reflex-module-script');
        if (modScript) modScript.remove();
        
        // Clean up imported module map if needed
        window.removeEventListener('unload', cleanupReflex);
    }

    // Hack into index.html switcher loadShader to trigger cleanup first
    const switcherBtns = document.querySelectorAll('#shader-switcher button[data-shader]');
    switcherBtns.forEach(b => {
        b.addEventListener('click', () => {
            if (window.cleanupReflex) window.cleanupReflex();
        });
    });
})();
