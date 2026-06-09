'use strict';
// TRANSFORMATION STATION — 3D grid note collection game
// Players hold their hands at glowing cells to collect notes
// Building a melody that opens the wormhole

(function () {
    // Inject Three.js import map
    const im = document.createElement('script');
    im.type = 'importmap';
    im.textContent = JSON.stringify({
        imports: {
            "three": "https://cdn.jsdelivr.net/npm/three@0.165.0/build/three.module.js",
            "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.165.0/examples/jsm/"
        }
    });
    document.head.appendChild(im);

    const mod = document.createElement('script');
    mod.type = 'module';
    mod.textContent = `
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const canvas = document.getElementById('c');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.0;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x000008);
scene.fog = new THREE.FogExp2(0x000008, 0.08);

const camera = new THREE.PerspectiveCamera(55, 1, 0.01, 100);
camera.position.set(0, 0.5, 4.5);

// ---- Lights ----
scene.add(new THREE.AmbientLight(0x223344, 1.2));
const key = new THREE.DirectionalLight(0x8866ff, 3);
key.position.set(2, 4, 3); scene.add(key);
const fill = new THREE.DirectionalLight(0x4499ff, 1.5);
fill.position.set(-3, 1, 2); scene.add(fill);

// ---- Starfield ----
const starGeo = new THREE.BufferGeometry();
const sp = new Float32Array(4000 * 3);
for (let i = 0; i < sp.length; i++) sp[i] = (Math.random() - 0.5) * 40;
starGeo.setAttribute('position', new THREE.BufferAttribute(sp, 3));
scene.add(new THREE.Points(starGeo, new THREE.PointsMaterial({ color: 0x8855ff, size: 0.025 })));

// ---- Pentatonic notes (MIDI) — 12 notes across 2+ octaves ----
const PENTA = [60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86];
const NOTE_NAMES = ['c4','d4','e4','g4','a4','c5','d5','e5','g5','a5','c6','d6'];

// ---- Sphere of 12 nodes ----
// Nodes distributed on a sphere using Fibonacci lattice for even coverage
const NUM_NODES = 12;
const SPHERE_RADIUS = 1.2;
const SPHERE_BASE_POSITIONS = []; // unit sphere positions, fixed
const GRID_POSITIONS = [];        // world positions after rotation, updated each frame
let sphereRotY = 0;               // current rotation angle
const SPHERE_ROT_SPEED = 0.04;    // radians per second

// Fibonacci sphere distribution — evenly spaced nodes
for (let i = 0; i < NUM_NODES; i++) {
    const phi = Math.acos(1 - 2 * (i + 0.5) / NUM_NODES);
    const theta = Math.PI * (1 + Math.sqrt(5)) * i;
    SPHERE_BASE_POSITIONS.push(new THREE.Vector3(
        Math.sin(phi) * Math.cos(theta),
        Math.cos(phi),
        Math.sin(phi) * Math.sin(theta)
    ));
    GRID_POSITIONS.push(new THREE.Vector3());
}

function updateGridPositions(dt) {
    sphereRotY += SPHERE_ROT_SPEED * (dt || 0);
    const cosR = Math.cos(sphereRotY);
    const sinR = Math.sin(sphereRotY);
    for (let i = 0; i < NUM_NODES; i++) {
        const b = SPHERE_BASE_POSITIONS[i];
        // Rotate around Y axis
        GRID_POSITIONS[i].set(
            b.x * cosR + b.z * sinR,
            b.y,
            -b.x * sinR + b.z * cosR
        ).multiplyScalar(SPHERE_RADIUS);
    }
}

// Map normalised hand coords (0-1) to world space
// Map to full visible screen area at z=0 plane
function handWorldPos(hx, hy, hz) {
    const fovY = camera.fov * Math.PI / 180;
    const halfH = Math.tan(fovY / 2) * camera.position.z;
    const halfW = halfH * camera.aspect;

    // Account for projector L/R crop — shift hand x so cropped canvas edges = hand edges
    const cropL = parseInt(document.getElementById('crop-left')?.value  || 0);
    const cropR = parseInt(document.getElementById('crop-right')?.value || 0);
    const totalW = canvas.clientWidth + cropL + cropR; // full uncropped width
    const hxAdj = totalW > 0 ? (hx * totalW - cropL) / canvas.clientWidth : hx;

    const mx = (hxAdj - 0.5) * halfW * 2;
    const my = -(hy - 0.5) * halfH * 2;
    // Z maps linearly through sphere depth range
    const wz = (hz - 0.5) * SPHERE_RADIUS * 2.5;
    return new THREE.Vector3(mx, my, wz);
}

// ---- Game state ----
const States = { IDLE: 0, SEQUENCE_PREVIEW: 4, PLAYING: 1, CLIMAX: 2, CONGRATULATIONS: 3 };
let state = States.IDLE;
let sequence = [];
let collected = 0;
let holdTimer = 0;
let idleTimer = 0;
let climaxTimer = 0;
let congratsTimer = 0;
let frozenHandPos = null;
let hasSeenPreview = false; // whether preview shown this cycle
let previewStep = 0;
let previewTimer = 0;
let lastPreviewTime = -999; // clock time of last preview (so first one always shows)
let sequenceAge = 0;        // how long current sequence has been active
const NUM_NOTES = 6;  // notes to collect out of NUM_NODES
const PREVIEW_STEP_TIME = 0.7;
const PREVIEW_COOLDOWN = 40;     // seconds between previews
const SEQUENCE_RESHUFFLE = 180;  // seconds before new sequence (3 min)
const PLAYING_IDLE_RESET = 8.0;  // seconds without hands during PLAYING before reset
const IDLE_PREVIEW_DELAY = 0.5;  // seconds after hand enters IDLE before starting preview

function shuffle(arr) {
    const a = [...arr];
    for (let i = a.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [a[i], a[j]] = [a[j], a[i]];
    }
    return a;
}

function resetGame() {
    // Pick 6 random nodes from 12 to be note targets
sequence = shuffle([0,1,2,3,4,5,6,7,8,9,10,11]).slice(0, NUM_NOTES);
    collected = 0;
    holdTimer = 0;
    idleTimer = 0;
    frozenHandPos = null;
    hasSeenPreview = false;
    lastPreviewTime = -999; // force preview on next hand
    previewStep = 0;
    previewTimer = 0;
    sequenceAge = 0;
    updateCellVisuals();
    updateMelody();
    state = States.IDLE;
    if (wormholeRing) { wormholeRing.scale.setScalar(0.01); wormholeRing.material.opacity = 0; }
    if (congratsOverlay) { congratsOverlay.style.opacity = 0; congratsOverlay.style.display = 'none'; }
}

// ---- OSC: send to Tidal via C++ HTTP endpoint ----
function sendOsc(key, val) {
    fetch('http://localhost:8080/orb', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({ ctrl: key, value: val })
    }).catch(() => {});
}

function updateMelody() {
    // Send collected notes as space-separated note string to Tidal
    if (collected === 0) { sendOsc('transformation_active', 0); return; }
    const notes = sequence.slice(0, collected).map(idx => NOTE_NAMES[idx]).join(' ');
    sendOsc('transformation_notes', notes);
    sendOsc('transformation_active', 1);
    sendOsc('transformation_count', collected);
}

// ---- Cell meshes ----
const cellGroup = new THREE.Group();
scene.add(cellGroup);
const cells = [];
const HOLD_TIME = 0.6;
const TOUCH_XY = 0.9;
const TOUCH_Z  = 0.8;

for (let i = 0; i < NUM_NODES; i++) {
    const geo = new THREE.BoxGeometry(0.8, 0.8, 0.8);
    const mat = new THREE.MeshStandardMaterial({
        color: 0x2244aa,
        emissive: 0x111133,
        roughness: 0.3,
        metalness: 0.6,
        transparent: true,
        opacity: 0.7,
        wireframe: false,
    });
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.copy(GRID_POSITIONS[i]);
    cellGroup.add(mesh);

    // Wireframe overlay
    const wf = new THREE.LineSegments(
        new THREE.EdgesGeometry(geo),
        new THREE.LineBasicMaterial({ color: 0x4466ff, transparent: true, opacity: 0.6 })
    );
    wf.position.copy(GRID_POSITIONS[i]);
    cellGroup.add(wf);

    cells.push({ mesh, wf, collected: false, noteIdx: i });
}

// ---- Note label sprites ----
function makeLabel(text) {
    const cv = document.createElement('canvas');
    cv.width = 128; cv.height = 64;
    const ctx = cv.getContext('2d');
    ctx.fillStyle = 'rgba(0,0,0,0)';
    ctx.fillRect(0,0,128,64);
    ctx.font = 'bold 32px monospace';
    ctx.fillStyle = '#aaddff';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, 64, 32);
    const tex = new THREE.CanvasTexture(cv);
    const mat = new THREE.SpriteMaterial({ map: tex, transparent: true, opacity: 0.9 });
    const sprite = new THREE.Sprite(mat);
    sprite.scale.set(0.6, 0.3, 1);
    return sprite;
}

const labels = [];
for (let i = 0; i < NUM_NODES; i++) {
    const lbl = makeLabel(NOTE_NAMES[sequence[i] || i]);
    lbl.position.copy(GRID_POSITIONS[i]);
    lbl.position.y += 0.7;
    scene.add(lbl);
    labels.push(lbl);
}

// ---- Alien GLB ----
const loader = new GLTFLoader();
const aliens = [];
loader.load('models/alien.glb', (gltf) => {
    for (let i = 0; i < NUM_NODES; i++) {
        const a = gltf.scene.clone(true);
        const box = new THREE.Box3().setFromObject(a);
        const size = box.getSize(new THREE.Vector3());
        a.scale.setScalar(0.35 / Math.max(size.x, size.y, size.z));
        a.position.copy(GRID_POSITIONS[i]);
        a.position.y += 0.8;
        scene.add(a);
        aliens.push(a);
    }
}, undefined, () => { console.log('[transformation] alien.glb not found'); });

// ---- Hand cursors — use hand.glb model ----
const handMeshes = [];
const handColors = [0xf0f4ff, 0xffe0aa, 0xaaffdd, 0xffaaff];
const handLoader = new GLTFLoader();
handLoader.load('models/hand.glb', (gltf) => {
    for (let i = 0; i < 4; i++) {
        const h = gltf.scene.clone(true);
        const box = new THREE.Box3().setFromObject(h);
        const size = box.getSize(new THREE.Vector3());
        const ns = 0.56 / Math.max(size.x, size.y, size.z); // 2x bigger
        h.scale.setScalar(ns);
        h.traverse(child => {
            if (child.isMesh) {
                child.material = new THREE.MeshStandardMaterial({
                    color: handColors[i],
                    emissive: new THREE.Color(handColors[i]).multiplyScalar(0.3),
                    roughness: 0.3, metalness: 0.1,
                    transparent: true, opacity: 0.9
                });
            }
        });
        h.visible = false;
        scene.add(h);
        handMeshes.push(h);
    }
}, undefined, () => {
    // Fallback to spheres if model not found
    for (let i = 0; i < 4; i++) {
        const m = new THREE.Mesh(
            new THREE.SphereGeometry(0.12, 16, 16),
            new THREE.MeshStandardMaterial({ color: handColors[i], emissive: handColors[i], emissiveIntensity: 0.8, transparent: true, opacity: 0.85 })
        );
        m.visible = false;
        scene.add(m);
        handMeshes.push(m);
    }
});

// ---- Hold ring (shows progress) ----
const holdRingGeo = new THREE.TorusGeometry(0.55, 0.04, 8, 48);
const holdRingMat = new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0 });
const holdRing = new THREE.Mesh(holdRingGeo, holdRingMat);
holdRing.position.set(0, -999, 0); // park off-screen
scene.add(holdRing);

// ---- Beam line (PINCH gesture preview) ----
const beamGeo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]);
const beamLine = new THREE.Line(beamGeo, new THREE.LineBasicMaterial({ color: 0x88ffff, transparent: true, opacity: 0 }));
scene.add(beamLine);

// ---- Wormhole climax ring ----
const wormholeRing = new THREE.Mesh(
    new THREE.TorusGeometry(1, 0.15, 16, 80),
    new THREE.MeshBasicMaterial({ color: 0x8844ff, transparent: true, opacity: 0, side: THREE.DoubleSide })
);
wormholeRing.scale.setScalar(0.01);
scene.add(wormholeRing);

// ---- Progress bar (HTML overlay) ----
const progressBar = document.createElement('div');
progressBar.style.cssText = 'position:fixed;bottom:20px;left:50%;transform:translateX(-50%);display:flex;gap:8px;pointer-events:none;';
document.body.appendChild(progressBar);
const progSegments = [];
for (let i = 0; i < NUM_NOTES; i++) {
    const seg = document.createElement('div');
    seg.style.cssText = 'width:32px;height:12px;border:1px solid rgba(100,150,255,0.5);border-radius:3px;background:rgba(20,30,80,0.7);transition:background 0.3s;';
    progressBar.appendChild(seg);
    progSegments.push(seg);
}

// ---- Congratulations overlay ----
const congratsOverlay = document.createElement('div');
// Respect L/R crop — position within the visible canvas area
function getCropStyle() {
    const l = parseInt(document.getElementById('crop-left')?.value || 0);
    const r = parseInt(document.getElementById('crop-right')?.value || 0);
    return \`left:\${l}px;right:\${r}px;width:auto;\`;
}
congratsOverlay.style.cssText = \`
    position:fixed;top:0;height:100%;
    \${getCropStyle()}
    display:none;align-items:center;justify-content:center;flex-direction:column;
    background:rgba(0,0,0,0);pointer-events:none;z-index:100;
    font-family:monospace;text-align:center;transition:opacity 1s;opacity:0;
\`;
congratsOverlay.innerHTML = \`
    <div style="font-size:clamp(24px,5vw,64px);color:#ffffff;text-shadow:0 0 40px #8844ff,0 0 80px #4422ff;margin-bottom:20px;letter-spacing:0.1em;">
        TRANSFORMATION COMPLETE
    </div>
    <div style="font-size:clamp(14px,3vw,32px);color:#aabbff;text-shadow:0 0 20px #6644ff;margin-bottom:10px;">
        The wormhole opens.
    </div>
    <div style="font-size:clamp(12px,2.5vw,28px);color:#8899ee;text-shadow:0 0 15px #4422ff;">
        The demigod arrives.
    </div>
\`;
document.body.appendChild(congratsOverlay);

// ---- Frequency canvas overlay for ending sequence ----
const freqCanvas = document.createElement('canvas');
freqCanvas.style.cssText = \`
    position:fixed;top:0;left:0;right:0;width:100%;height:100%;
    display:none;pointer-events:none;z-index:90;opacity:0;
\`;
document.body.appendChild(freqCanvas);

const freqGL = freqCanvas.getContext('webgl2');
let freqProg = null;
let freqBuf = null;

if (freqGL) {
    const fVERT = \`#version 300 es
    in vec2 a_pos; out vec2 v_uv;
    void main() { v_uv=a_pos*0.5+0.5; gl_Position=vec4(a_pos,0,1); }\`;

    const fFRAG = \`#version 300 es
    precision highp float;
    in vec2 v_uv; out vec4 fragColor;
    uniform float u_time;
    uniform vec2 u_resolution;
    uniform vec2 u_offset; // crop offset in pixels

    void main() {
        fragColor = vec4(0.0);
        vec2 fragCoord = v_uv * u_resolution;
        // Expanding circle from centre, grows over 7s then stays full
        float ramp = clamp(u_time / 7.0, 0.0, 1.0);
        float expand = ramp * ramp * (3.0 - 2.0 * ramp);
        vec2 centre = u_resolution * 0.5;
        float maxRadius = length(u_resolution) * 0.8;
        float dist = length(fragCoord - centre);
        float edgeR = expand * maxRadius;
        float edge = smoothstep(edgeR, edgeR - 60.0, dist);
        if (edge < 0.01) { fragColor = vec4(0.0); return; }

        // Stable ring density — never drops to zero
        float norm = 0.5 + 0.15 * sin(u_time * 0.3) + 0.08 * sin(u_time * 0.71);
        int count = int(120.0 * norm);
        count = min(count, 160);
        for (int s = 0; s < count; s++) {
            vec2 R = u_resolution;
            vec2 u2 = (fragCoord * 2.0 - R + vec2(s % 8, s / 8) / 4.0 - 2.0) / R.x;
            u2 = floor((6.0 - vec2(atan(u2.y, u2.x) / 3.0, length(u2))) * R) + 0.5;
            fragColor += max(
                1.0 - fract(vec4(7,6,4,0) * 0.02
                    + (u2.y * 0.02 + u2.x * 0.4) * fract(u2.x * 0.61)
                    + u_time) * 5.0,
                0.0) / 64.0;
        }
        fragColor *= edge;
    }\`;

    const compileF = (src, type) => {
        const s = freqGL.createShader(type);
        freqGL.shaderSource(s, src); freqGL.compileShader(s);
        if (!freqGL.getShaderParameter(s, freqGL.COMPILE_STATUS)) console.error(freqGL.getShaderInfoLog(s));
        return s;
    };
    freqProg = freqGL.createProgram();
    freqGL.attachShader(freqProg, compileF(fVERT, freqGL.VERTEX_SHADER));
    freqGL.attachShader(freqProg, compileF(fFRAG, freqGL.FRAGMENT_SHADER));
    freqGL.linkProgram(freqProg);

    freqBuf = freqGL.createBuffer();
    freqGL.bindBuffer(freqGL.ARRAY_BUFFER, freqBuf);
    freqGL.bufferData(freqGL.ARRAY_BUFFER, new Float32Array([-1,-1,1,-1,-1,1,1,-1,1,1,-1,1]), freqGL.STATIC_DRAW);
}

function renderFreq(t, congratsAge) {
    if (!freqGL || !freqProg) return;
    const w = freqCanvas.clientWidth, h = freqCanvas.clientHeight;
    if (freqCanvas.width !== w || freqCanvas.height !== h) {
        freqCanvas.width = w; freqCanvas.height = h;
    }
    freqGL.viewport(0, 0, w, h);
    freqGL.useProgram(freqProg);
    const loc = freqGL.getAttribLocation(freqProg, 'a_pos');
    freqGL.bindBuffer(freqGL.ARRAY_BUFFER, freqBuf);
    freqGL.enableVertexAttribArray(loc);
    freqGL.vertexAttribPointer(loc, 2, freqGL.FLOAT, false, 0, 0);
    freqGL.uniform1f(freqGL.getUniformLocation(freqProg, 'u_time'), congratsAge);
    freqGL.uniform2f(freqGL.getUniformLocation(freqProg, 'u_resolution'), w, h);
    // Centre = middle of visible (cropped) area
    const cropL = parseInt(document.getElementById('crop-left')?.value || 0);
    const cropR = parseInt(document.getElementById('crop-right')?.value || 0);
    const visW = w - cropL - cropR;
    // Shift: visible centre is at cropL + visW/2, canvas centre is w/2
    // offset = (cropL + visW/2) - w/2
    const offsetX = (cropL + visW * 0.5) - w * 0.5;
    freqGL.uniform2f(freqGL.getUniformLocation(freqProg, 'u_offset'), offsetX, 0);
    freqGL.drawArrays(freqGL.TRIANGLES, 0, 6);
}

function updateCellVisuals() {
    // sequence = array of NUM_NOTES node indices (e.g. [3,7,1,9,5,11])
    // collected = how many have been collected so far
    // Active target = sequence[collected] = the node index to hit next

    for (let i = 0; i < NUM_NODES; i++) {
        const c = cells[i];
        const seqPos = sequence.indexOf(i); // -1 if not in sequence (decoy)
        const isDecoy    = seqPos === -1;
        const isCollected = seqPos !== -1 && seqPos < collected;
        const isActive   = seqPos === collected && state === States.PLAYING;

        c.mesh.scale.setScalar(1.0);

        if (isCollected) {
            // Already collected — dark, no alien
            c.mesh.material.color.setHex(0x001122);
            c.mesh.material.emissive.setHex(0x000811);
            c.mesh.material.opacity = 0.2;
            c.wf.material.opacity = 0.1;
            c.mesh.scale.setScalar(0.8);
            if (aliens[i]) aliens[i].visible = false;
            if (labels[i]) labels[i].material.opacity = 0.0;
        } else if (isActive) {
            // Current target — bright when hints on, otherwise same as future nodes
            const hintBright = window.showHints !== false;
            c.mesh.material.color.setHex(hintBright ? 0xffffff : 0x1a2266);
            c.mesh.material.emissive.setHex(hintBright ? 0x4422aa : 0x0a0f44);
            c.mesh.material.opacity = hintBright ? 0.9 : 0.5;
            c.wf.material.opacity = hintBright ? 1.0 : 0.35;
            c.wf.material.color.setHex(hintBright ? 0xffffff : 0x3355aa);
            if (aliens[i]) aliens[i].visible = true;
            if (labels[i]) labels[i].material.opacity = (window.showLabels && hintBright) ? 0.9 : 0.0;
        } else if (isDecoy) {
            // Decoy — dimmer blue, alien visible but smaller
            c.mesh.material.color.setHex(0x0a1033);
            c.mesh.material.emissive.setHex(0x050818);
            c.mesh.material.opacity = 0.35;
            c.wf.material.opacity = 0.2;
            c.wf.material.color.setHex(0x223355);
            if (aliens[i]) aliens[i].visible = true;
            if (labels[i]) labels[i].material.opacity = 0.0; // no label on decoys
        } else {
            // Future note in sequence — medium blue
            c.mesh.material.color.setHex(0x1a2266);
            c.mesh.material.emissive.setHex(0x0a0f44);
            c.mesh.material.opacity = 0.5;
            c.wf.material.opacity = 0.35;
            c.wf.material.color.setHex(0x3355aa);
            if (aliens[i]) aliens[i].visible = true;
            if (labels[i]) labels[i].material.opacity = 0.0;
        }
    }

    // Update progress bar (only NUM_NOTES segments)
    for (let i = 0; i < NUM_NOTES; i++) {
        if (i < collected) {
            progSegments[i].style.background = 'rgba(80,120,255,0.9)';
        } else if (i === collected) {
            progSegments[i].style.background = 'rgba(180,180,255,0.5)';
        } else {
            progSegments[i].style.background = 'rgba(20,30,80,0.7)';
        }
    }
}

// ---- Particle burst on collection ----
function burstParticles(pos) {
    const count = 30;
    const geo = new THREE.BufferGeometry();
    const positions = new Float32Array(count * 3);
    const velocities = [];
    for (let i = 0; i < count; i++) {
        positions[i*3] = pos.x; positions[i*3+1] = pos.y; positions[i*3+2] = pos.z;
        velocities.push(new THREE.Vector3((Math.random()-0.5)*4, (Math.random()-0.5)*4, (Math.random()-0.5)*4));
    }
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    const pts = new THREE.Points(geo, new THREE.PointsMaterial({ color: 0xffffff, size: 0.08, transparent: true }));
    scene.add(pts);
    let life = 1.0;
    const tick = () => {
        life -= 0.04;
        if (life <= 0) { scene.remove(pts); return; }
        const p = geo.attributes.position.array;
        for (let i = 0; i < count; i++) {
            p[i*3]   += velocities[i].x * 0.03;
            p[i*3+1] += velocities[i].y * 0.03;
            p[i*3+2] += velocities[i].z * 0.03;
        }
        geo.attributes.position.needsUpdate = true;
        pts.material.opacity = life;
        requestAnimationFrame(tick);
    };
    tick();
}

// ---- Resize ----
function resize() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    // Positions update each frame via animate() — no static update needed on resize
}
window.addEventListener('resize', resize); resize();

// ---- Main loop ----
const clock = new THREE.Clock();
let frameCount = 0;

function animate() {
    requestAnimationFrame(animate);
    const dt = Math.min(clock.getDelta(), 0.05);
    const t = clock.getElapsedTime();
    frameCount++;

    const st = window.instrumentState;
    const hands = (st && st.hands) || [];
    const numHands = hands.length;

    // Rotate stars
    scene.children.forEach(c => { if (c.isPoints) c.rotation.y += 0.0001; });

    // Update sphere node positions (rotation)
    updateGridPositions(dt);

    // Update cell/alien/label world positions to match rotating sphere
    cells.forEach((c, i) => {
        c.mesh.position.copy(GRID_POSITIONS[i]);
        c.wf.position.copy(GRID_POSITIONS[i]);
    });
    labels.forEach((l, i) => {
        if (l) { l.position.copy(GRID_POSITIONS[i]); l.position.y += 0.55; }
    });

    // Bob aliens
    aliens.forEach((a, i) => {
        if (a && a.visible) {
            // Follow sphere node + bob above it
            a.position.set(
                GRID_POSITIONS[i].x,
                GRID_POSITIONS[i].y + 0.55 + Math.sin(t * 1.2 + i * 0.8) * 0.08,
                GRID_POSITIONS[i].z
            );
            a.rotation.y += 0.008;
        }
    });

    // ---- Hand cursors — always update regardless of state ----
    hands.slice(0, 4).forEach((h, i) => {
        if (i >= handMeshes.length) return;
        const wp = handWorldPos(
            window.mirrorX ? 1-(h.x||0.5) : (h.x||0.5),
            h.y||0.5, h.z||0.5
        );
        const prev = handMeshes[i].position.clone();
        handMeshes[i].visible = true;
        handMeshes[i].position.lerp(wp, 0.3);
        const vel = handMeshes[i].position.clone().sub(prev);
        // Base tilt from position (outward lean) + velocity tilt
        const posX = handMeshes[i].position.x;
        const posY = handMeshes[i].position.y;
        handMeshes[i].rotation.z = -posX * 0.3 - vel.x * 6;
        handMeshes[i].rotation.x =  posY * 0.2 + vel.y * 4;
        handMeshes[i].rotation.y =  posX * 0.2; // slight Y rotation toward viewer
    });
    for (let i = numHands; i < 4; i++) {
        if (i < handMeshes.length) handMeshes[i].visible = false;
    }

    // ---- State machine ----
    // Reshuffle sequence periodically (resets collected progress)
    sequenceAge += dt;
    if (sequenceAge >= SEQUENCE_RESHUFFLE && state === States.IDLE && numHands === 0) {
        // Pick 6 random nodes from 12 to be note targets
sequence = shuffle([0,1,2,3,4,5,6,7,8,9,10,11]).slice(0, NUM_NOTES);
        sequenceAge = 0;
        hasSeenPreview = false; // allow preview of new sequence
        updateCellVisuals();
        console.log('[transform] new sequence generated');
    }

    if (state === States.IDLE) {
        if (numHands > 0) {
            const now = clock.getElapsedTime();
            const canPreview = (now - lastPreviewTime) >= PREVIEW_COOLDOWN;
            if (canPreview) {
                // Show preview — either first time or cooldown elapsed
                console.log('[transform] starting sequence preview');
                state = States.SEQUENCE_PREVIEW;
                previewStep = 0;
                previewTimer = 0;
                // Dim all cells for preview
                cells.forEach(c => {
                    c.mesh.material.color.setHex(0x001122);
                    c.mesh.material.opacity = 0.2;
                    c.wf.material.opacity = 0.1;
                });
            } else {
                state = States.PLAYING;
            }
            idleTimer = 0;
        }
        idleTimer += dt;
    }

    else if (state === States.SEQUENCE_PREVIEW) {
        previewTimer += dt;
        // Light up current preview cell
        cells.forEach((c, i) => {
            if (i === sequence[previewStep]) {
                c.mesh.material.color.setHex(0xffffff);
                c.mesh.material.emissive.setHex(0x6633ff);
                c.mesh.material.opacity = 1.0;
                c.wf.material.color.setHex(0xffffff);
                c.wf.material.opacity = 1.0;
                c.mesh.scale.setScalar(1.2 + Math.sin(t * 8) * 0.08);
            } else if (i < previewStep) {
                // Already shown — dim but leave visible
                c.mesh.material.color.setHex(0x112244);
                c.mesh.material.opacity = 0.35;
                c.wf.material.opacity = 0.2;
            } else {
                c.mesh.material.color.setHex(0x001122);
                c.mesh.material.opacity = 0.15;
                c.wf.material.opacity = 0.08;
            }
        });

        if (previewTimer >= PREVIEW_STEP_TIME) {
            previewTimer = 0;
            console.log('[transform] preview step', previewStep, 'of 8');
            // Play this cell's note as a clean one-shot directly in SC
            const noteIdx = sequence[previewStep];
            const MIDI = [60, 62, 64, 67, 69, 72, 74, 76]; // C4 D4 E4 G4 A4 C5 D5 E5
            fetch('http://localhost:8080/note', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({ midi: MIDI[noteIdx], amp: 0.75, decay: 0.4 })
            }).catch(() => {});
            previewStep++;
            if (previewStep >= NUM_NOTES) {
                // Preview complete — start game
                hasSeenPreview = true;
                lastPreviewTime = clock.getElapsedTime();
                updateCellVisuals();
                state = States.PLAYING;
            }
        }
    }

    else if (state === States.PLAYING) {
        if (numHands === 0) {
            idleTimer += dt;
            if (idleTimer > PLAYING_IDLE_RESET) resetGame();
        } else {
            idleTimer = 0;
        }


        // Touch detection
        if (collected < 8) {
            const targetNodeIdx = sequence[collected]; // which sphere node is the target
            const targetPos = GRID_POSITIONS[targetNodeIdx];
            let touching = false;
            let gesture = 0;

            hands.forEach(h => {
                const mx = window.mirrorX ? 1-(h.x||0.5) : (h.x||0.5);
                gesture = h.g_id || 0;

                // THUMBS_DOWN = skip
                if (gesture === 7) { collected++; updateCellVisuals(); updateMelody(); return; }

                let handPos = frozenHandPos;
                if (gesture === 2 && frozenHandPos === null) {
                    // FIST: freeze current position
                    frozenHandPos = handWorldPos(mx, h.y||0.5, h.z||0.5);
                }
                if (gesture !== 2) frozenHandPos = null;
                handPos = handPos || handWorldPos(mx, h.y||0.5, h.z||0.5);

                let radiusMult = 1.0;
                if (gesture === 3) radiusMult = 0.5;   // POINT: smaller radius, instant
                if (gesture === 5) radiusMult = 1.5;   // SPREAD: larger radius

                const dx = Math.abs(handPos.x - targetPos.x);
                const dy = Math.abs(handPos.y - targetPos.y);
                const dz = Math.abs(handPos.z - targetPos.z);

                if (dx < TOUCH_XY * radiusMult && dy < TOUCH_XY * radiusMult && dz < TOUCH_Z * 2) {
                    touching = true;
                    // POINT: instant collect
                    if (gesture === 3) { holdTimer = 1.0; }
                }

                // Hints removed — no beam preview, no THUMBS_UP emissive
                beamLine.material.opacity = 0;
            });

            if (touching) {
                holdTimer += dt / HOLD_TIME;
                // Pulse only when enabled (default off = hard mode)
                if (window.showPulse === true) {
                    const activeNode = sequence[collected];
                    if (activeNode !== undefined && cells[activeNode]) {
                        const s = 1.1 + Math.sin(t * 8) * 0.08;
                        cells[activeNode].mesh.scale.setScalar(s);
                    }
                }
                if (window.showPulse === true) {
                    holdRing.position.copy(targetPos);
                    holdRingMat.opacity = 0.8;
                    holdRing.scale.setScalar(holdTimer * 1.2 + 0.8);
                }
            } else {
                holdTimer = Math.max(0, holdTimer - dt * 2);
                holdRingMat.opacity = window.showPulse === true ? holdTimer * 0.5 : 0;
            }

            if (holdTimer >= 1.0) {
                // COLLECTED! Play same note as preview via /note
                burstParticles(GRID_POSITIONS[sequence[collected]]);
                const MIDI = [60, 62, 64, 67, 69, 72, 74, 76];
                const collectedMidi = PENTA[sequence[collected]];
                fetch('http://localhost:8080/note', {
                    method: 'POST',
                    headers: {'Content-Type':'application/json'},
                    body: JSON.stringify({ midi: collectedMidi, amp: 0.8, decay: 0.4 })
                }).catch(() => {});
                sendOsc('transformation_note', collectedMidi); // also update Tidal loop
                collected++;
                holdTimer = 0;
                holdRingMat.opacity = 0;
                updateCellVisuals();
                updateMelody();

                if (collected >= NUM_NOTES) {
                    state = States.CLIMAX;
                    climaxTimer = 0;
                    sendOsc('transformation_climax', 1);
                }
            }
        }
    }

    else if (state === States.CLIMAX) {
        climaxTimer += dt;
        const p = Math.min(climaxTimer / 3.5, 1);
        // Wormhole ring expands
        wormholeRing.scale.setScalar(0.01 + p * 8);
        wormholeRing.material.opacity = Math.sin(p * Math.PI) * 0.9;
        wormholeRing.rotation.z += 0.03;
        wormholeRing.rotation.x += 0.015;
        // Scene fades to black as ring passes over
        renderer.setClearColor(0x000000, p);
        // Frequency starts expanding from first frame of CLIMAX (show once)
        if (climaxTimer < 0.05) {
            // Match freqCanvas exactly to the main canvas (same crop)
            const mainCanvas = document.getElementById('c');
            freqCanvas.style.left   = mainCanvas.style.marginLeft || '0px';
            freqCanvas.style.right  = 'auto';
            freqCanvas.style.width  = mainCanvas.clientWidth + 'px';
            freqCanvas.style.top    = '0px';
            freqCanvas.style.height = '100%';
            freqCanvas.style.display = 'block';
            freqCanvas.style.opacity = 0;
            freqCanvas.style.transition = 'opacity 1s';
            setTimeout(() => { freqCanvas.style.opacity = 1; }, 50);
        }
        renderFreq(t, climaxTimer);
        if (climaxTimer > 3.5) {
            state = States.CONGRATULATIONS;
            congratsTimer = 0;
            // Re-apply crop to overlay
            const l = parseInt(document.getElementById('crop-left')?.value || 0);
            const r = parseInt(document.getElementById('crop-right')?.value || 0);
            congratsOverlay.style.left = l + 'px';
            congratsOverlay.style.right = r + 'px';
            congratsOverlay.style.display = 'flex';
            setTimeout(() => { congratsOverlay.style.opacity = 1; }, 0);
        }
    }

    else if (state === States.CONGRATULATIONS) {
        congratsTimer += dt;
        // Continue frequency animation — time starts from CLIMAX begin (3.5 + congratsTimer)
        renderFreq(t, 3.5 + congratsTimer);

        if (congratsTimer > 15.0) {
            // Fade out frequency canvas and text
            freqCanvas.style.transition = 'opacity 1.5s';
            freqCanvas.style.opacity = 0;
            congratsOverlay.style.opacity = 0;
            setTimeout(() => {
                congratsOverlay.style.display = 'none';
                freqCanvas.style.display = 'none';
                renderer.setClearColor(0x000008, 1);
                resetGame();
            }, 1500);
        }
    }

    renderer.render(scene, camera);
}

// Init
resetGame();
animate();

// Expose ending trigger for test button
window.triggerEnding = () => {
    state = States.CLIMAX;
    climaxTimer = 0;
    sendOsc('transformation_climax', 1);
    console.log('[transformation] ending triggered manually');
};
`;
    document.body.appendChild(mod);
})();
