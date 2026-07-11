'use strict';
// Three.js hand renderer — loads hand.glb, positions at detected hands
// No rig: position + scale + tilt driven by centroid + z depth + velocity

(function () {
    // Inject import map so bare 'three' specifier resolves
    const importMap = document.createElement('script');
    importMap.type = 'importmap';
    importMap.textContent = JSON.stringify({
        imports: {
            "three": "https://cdn.jsdelivr.net/npm/three@0.165.0/build/three.module.js",
            "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.165.0/examples/jsm/"
        }
    });
    document.head.appendChild(importMap);

    const moduleScript = document.createElement('script');
    moduleScript.type = 'module';
    moduleScript.textContent = `
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const canvas = document.getElementById('c');

// Three.js scene
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
renderer.setPixelRatio(1.0);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.2;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x000008);

const camera = new THREE.PerspectiveCamera(50, 1, 0.01, 100);
camera.position.set(0, 0, 3);

// Starfield background particles
const starGeo = new THREE.BufferGeometry();
const starCount = 3000;
const starPos = new Float32Array(starCount * 3);
for (let i = 0; i < starCount * 3; i++) starPos[i] = (Math.random() - 0.5) * 20;
starGeo.setAttribute('position', new THREE.BufferAttribute(starPos, 3));
const starMat = new THREE.PointsMaterial({ color: 0x9955ff, size: 0.02, sizeAttenuation: true });
scene.add(new THREE.Points(starGeo, starMat));

// Lighting — Fantasia glove: bright key, blue rim, soft fill
const keyLight = new THREE.DirectionalLight(0xfff5e0, 2.5);
keyLight.position.set(1, 2, 2);
scene.add(keyLight);

const rimLight = new THREE.DirectionalLight(0x4477ff, 1.8);
rimLight.position.set(-2, -1, -2);
scene.add(rimLight);

const fillLight = new THREE.DirectionalLight(0xaaccff, 0.6);
fillLight.position.set(0, 1, 1);
scene.add(fillLight);

scene.add(new THREE.AmbientLight(0x223344, 0.8));

// Glove material — pearl white
const gloveMat = new THREE.MeshStandardMaterial({
    color: 0xf0f4ff,
    roughness: 0.25,
    metalness: 0.05,
    envMapIntensity: 1.0,
});

// Load hand model
const loader = new GLTFLoader();
const MAX_HANDS = 4;
const handMeshes = [];
const handState = Array.from({length: MAX_HANDS}, () => ({
    active: false,
    pos: new THREE.Vector3(0, 0, 0),
    targetPos: new THREE.Vector3(0, 0, 0),
    vel: new THREE.Vector3(0, 0, 0),
    rot: new THREE.Euler(0, 0, 0),
    targetRot: new THREE.Euler(0, 0, 0),
    scale: 0.5,
    targetScale: 0.5,
    opacity: 0,
    gesture: 0,
}));

loader.load('models/hand.glb', (gltf) => {
    // Create one mesh per possible hand
    for (let i = 0; i < MAX_HANDS; i++) {
        const mesh = gltf.scene.clone(true);
        // Apply glove material to all mesh children
        mesh.traverse(child => {
            if (child.isMesh) {
                child.material = gloveMat.clone();
                child.castShadow = false;
            }
        });
        mesh.visible = false;

        // Normalise model scale — check bounding box first
        const box = new THREE.Box3().setFromObject(mesh);
        const size = box.getSize(new THREE.Vector3());
        const maxDim = Math.max(size.x, size.y, size.z);
        mesh.scale.setScalar(1.0 / maxDim); // normalise to unit size
        mesh.userData.normalScale = 1.0 / maxDim;

        scene.add(mesh);
        handMeshes.push(mesh);
    }
    console.log('[hand.js] model loaded, meshes:', MAX_HANDS);
}, undefined, (err) => {
    console.error('[hand.js] load error:', err);
});

function resize() {
    const scale = window.webglRenderScale || 1.0;
    const w = canvas.clientWidth * scale, h = canvas.clientHeight * scale;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);
resize();

// Map normalised hand coords (0-1) to Three.js world space
function handToWorld(x, y, z, aspect) {
    // x,y: 0-1 screen coords → world
    const wx = (x * 2 - 1) * camera.aspect * 1.4;
    const wy = -(y * 2 - 1) * 1.4;
    const wz = -z * 1.5; // depth: closer = more toward camera
    return new THREE.Vector3(wx, wy, wz);
}

const clock = new THREE.Clock();
let frame = 0;

function lerp(a, b, t) { return a + (b - a) * t; }
function lerpAngle(a, b, t) {
    let d = b - a;
    while (d > Math.PI) d -= Math.PI * 2;
    while (d < -Math.PI) d += Math.PI * 2;
    return a + d * t;
}

// Track previous hand positions for velocity-based tilt
const prevPos = Array.from({length: MAX_HANDS}, () => new THREE.Vector3());

function animate() {
    requestAnimationFrame(animate);
    const dt = Math.min(clock.getDelta(), 0.05);
    const t = clock.getElapsedTime();
    frame++;

    const st = window.instrumentState;
    const hands = (st && st.hands) || [];

    // Gently rotate stars
    scene.children.forEach(c => { if (c.isPoints) c.rotation.y += 0.0003; });

    for (let i = 0; i < MAX_HANDS; i++) {
        const mesh = handMeshes[i];
        if (!mesh) continue;

        const h = hands[i];
        const s = handState[i];

        if (h) {
            s.active = true;
            const wp = handToWorld(h.x || 0.5, h.y || 0.5, h.z || 0.5, camera.aspect);
            s.targetPos.copy(wp);
            s.targetScale = lerp(0.35, 0.22, h.z || 0.5); // closer = bigger

            // Velocity from position delta → tilt
            const vel = wp.clone().sub(prevPos[i]);
            prevPos[i].copy(wp);

            // Tilt based on movement direction
            s.targetRot.x = lerpAngle(s.targetRot.x, -vel.y * 8 + Math.sin(t * 0.4 + i) * 0.08, 0.15);
            s.targetRot.z = lerpAngle(s.targetRot.z,  vel.x * 8 + Math.cos(t * 0.3 + i) * 0.06, 0.15);
            s.targetRot.y = lerpAngle(s.targetRot.y,  vel.x * 4, 0.1);

            s.gesture = h.g_id || 0;
        } else {
            s.active = false;
        }

        // Smooth position, scale, opacity
        s.pos.lerp(s.targetPos, 0.6);   // fast position — nearly instant
        s.scale = lerp(s.scale, s.active ? s.targetScale : 0, 0.25);
        s.opacity = lerp(s.opacity, s.active ? 1.0 : 0.0, 0.15);
        s.rot.x = lerpAngle(s.rot.x, s.targetRot.x, 0.2);  // rotation stays smooth
        s.rot.z = lerpAngle(s.rot.z, s.targetRot.z, 0.2);
        s.rot.y = lerpAngle(s.rot.y, s.targetRot.y, 0.15);

        if (mesh) {
            mesh.visible = s.opacity > 0.02;
            mesh.position.copy(s.pos);

            // Scale = normalised model scale × display scale
            const ns = mesh.userData.normalScale || 1;
            mesh.scale.setScalar(ns * s.scale);

            mesh.rotation.set(s.rot.x, s.rot.y, s.rot.z);

            // Fade in/out via material opacity
            mesh.traverse(child => {
                if (child.isMesh && child.material) {
                    child.material.transparent = true;
                    child.material.opacity = Math.min(s.opacity, 1.0);

                    // Level-driven colour tint
                    const lf = ((st && st.level) || 0) / 5.0;
                    const r = lerp(0.94, lerp(1.0, 0.4, lf * lf), lf * 0.5);
                    const g = lerp(0.96, lerp(0.88, 0.8, lf), lf * 0.4);
                    const b = lerp(1.0, lerp(0.3, 1.0, lf), lf * 0.3);
                    child.material.color.setRGB(r, g, b);
                }
            });
        }
    }

    renderer.render(scene, camera);
}
animate();
`;
    document.body.appendChild(moduleScript);
})();
