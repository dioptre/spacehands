'use strict';
// STELLAR OFFERING — Wormhole ritual game
// Orbs drift through 3D space. Match hand position in x,y,z to touch them.
// Each orb owns a musical layer. Hold = sustain. All held = wormhole opens.

(function () {
    const canvas = document.getElementById('c');
    const gl = canvas.getContext('webgl2', { antialias: true });
    if (!gl) { console.error('WebGL2 not available'); return; }

    // ---- Orb definitions ----
    const ORB_DEFS = [
        { id: 0, name: 'bass',  zBucket: 0.10, color: [0.2, 0.4, 1.0],  radius: 0.06, speed: 0.18 },
        { id: 1, name: 'pad',   zBucket: 0.35, color: [0.6, 0.2, 1.0],  radius: 0.055,speed: 0.12 },
        { id: 2, name: 'lead',  zBucket: 0.60, color: [0.2, 0.9, 0.7],  radius: 0.050,speed: 0.22 },
        { id: 3, name: 'arp',   zBucket: 0.82, color: [1.0, 0.7, 0.15], radius: 0.045,speed: 0.30 },
    ];

    // Runtime orb state
    const orbs = ORB_DEFS.map(def => ({
        ...def,
        x: 0.3 + Math.random() * 0.4,
        y: 0.25 + Math.random() * 0.5,
        z: def.zBucket,           // depth (0=close,1=far) — maps to z_mm bucket
        vx: (Math.random()-0.5) * def.speed,
        vy: (Math.random()-0.5) * def.speed,
        // z oscillates around its bucket zone
        zPhase: Math.random() * Math.PI * 2,
        zAmp:   0.06,
        touched:  false,
        touchTime: 0,
        heldBy: -1,      // which hand id is holding it
        holdDuration: 0, // seconds held
        activated: false,
        flashT: 0,       // flash timer on touch
    }));

    // Game state
    const game = {
        wormholeProgress: 0,   // 0-1
        allActivated: false,
        climaxTimer: 0,
        activeCount: 0,
    };

    // ---- GLSL ----
    const VERT = `#version 300 es
    in vec2 a_pos; out vec2 v_uv;
    void main() { v_uv=a_pos*0.5+0.5; gl_Position=vec4(a_pos,0,1); }`;

    const FRAG = `#version 300 es
    precision highp float;
    in vec2 v_uv; out vec4 fragColor;

    uniform float u_time;
    uniform vec2  u_resolution;
    uniform float u_progress;     // 0-1 wormhole open amount
    uniform int   u_num_hands;
    uniform vec4  u_hands[8];

    // Per-orb data: x,y,z,radius  +  r,g,b,flash
    uniform vec4  u_orbs[4];      // position + radius
    uniform vec4  u_orbCols[4];   // rgb + flash amount
    uniform int   u_num_orbs;

    // ---- Voronoi starfield ----
    vec3 hash3(vec2 p){
        vec3 q=vec3(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3)),dot(p,vec2(419.2,371.9)));
        return fract(sin(q)*43758.5453);
    }
    float vnoise(vec2 p){
        float k=1.0+63.0*pow(0.4,6.0);
        vec2 i=floor(p),f=fract(p),a=vec2(0.0);
        for(int y=-2;y<=2;y++) for(int x=-2;x<=2;x++){
            vec2 g=vec2(x,y); vec3 o=hash3(i+g)*vec3(1.0,1.0,1.0);
            vec2 d=g-f+o.xy; float w=pow(1.0-smoothstep(0.0,1.414,length(d)),k);
            a+=vec2(o.z*w,w);
        } return a.x/a.y;
    }
    vec3 starfield(vec2 p, float t){
        p*=1.2*(1.0+u_progress*0.5);
        float r=0.1/max(abs(p.x),0.001);
        p=p*r-vec2(r+2.5*mod(t*(1.0+u_progress*2.0),60.0),0.0);
        return vec3(0.4,0.15,0.7)*(vnoise(5.0*p)+0.2*r);
    }

    // ---- Wormhole ring ----
    vec3 wormhole(vec2 uv, float t) {
        float r = length(uv);
        float tunnel = smoothstep(0.05 + u_progress*0.4, 0.0, r);
        float ang = atan(uv.y, uv.x);
        float spin = sin(ang*6.0 + r*15.0 - t*(2.0+u_progress*4.0));
        vec3 innerCol = mix(vec3(0.3,0.1,0.8), vec3(1.0,0.6,0.1), u_progress);
        return innerCol * tunnel * (0.4 + spin*0.6);
    }

    // ---- Orb rendering ----
    vec4 renderOrb(vec2 uv, vec3 orbPos, float orbR, vec3 col, float flash, float t) {
        // Project orb: z controls apparent size (close=big)
        float apparentR = orbR * mix(1.8, 0.6, orbPos.z);
        vec2 orbUV = vec2(orbPos.x*2.0-1.0, -(orbPos.y*2.0-1.0));
        orbUV.x *= u_resolution.x/u_resolution.y;
        float d = length(uv - orbUV);

        float body   = smoothstep(apparentR, apparentR*0.6, d);
        float glow   = exp(-d*6.0/apparentR) * 0.6;
        float halo   = exp(-d*2.5/apparentR) * 0.3;

        // Inner shimmer
        float shimmer = sin(d*40.0 - t*3.0)*0.5+0.5;
        shimmer = pow(shimmer, 4.0) * body * 0.4;

        // Flash on touch
        vec3 flashCol = vec3(1.0);
        vec3 orbCol = mix(col, flashCol, flash);

        float alpha = body + glow + halo;
        vec3  rgb   = orbCol * (body*1.2 + glow*0.8 + halo*0.4 + shimmer);
        return vec4(rgb, alpha);
    }

    void main() {
        vec2 uv = v_uv*2.0-1.0;
        uv.x *= u_resolution.x/u_resolution.y;

        float t = u_time;

        // Background: stars + wormhole
        vec3 col = starfield(uv, t)*2.0;
        col += wormhole(uv, t);

        // Depth indicator rings — faint rings showing z zones
        for(int i=0;i<4;i++){
            float zRing = mix(0.05, 0.55, float(i)/3.0);
            float ringR = mix(0.85, 0.15, zRing);
            float ring = abs(length(uv) - ringR);
            ring = smoothstep(0.012, 0.0, ring);
            col += vec3(0.15,0.1,0.3) * ring * 0.3;
        }

        // Orbs
        for(int i=0;i<4;i++){
            if(i >= u_num_orbs) break;
            vec4 op = u_orbs[i];
            vec4 oc = u_orbCols[i];
            vec4 orbColor = renderOrb(uv, op.xyz, op.w, oc.rgb, oc.a, t);
            col = mix(col, col + orbColor.rgb, min(orbColor.a, 1.0));
        }

        // Hand positions — ghost markers
        for(int i=0;i<8;i++){
            if(i>=u_num_hands) break;
            vec4 h = u_hands[i];
            vec2 hUV = vec2(h.x*2.0-1.0, -(h.y*2.0-1.0));
            hUV.x *= u_resolution.x/u_resolution.y;
            float hSize = mix(0.06, 0.02, h.z);
            float hd = length(uv - hUV);
            // Crosshair ring
            float ring = abs(hd - hSize);
            ring = smoothstep(0.008, 0.0, ring) * 0.6;
            col += vec3(0.8,0.9,1.0)*ring;
            // Z depth shown as ring fill
            float fill = smoothstep(hSize*0.9, 0.0, hd) * 0.08;
            col += vec3(0.5,0.7,1.0)*fill;
        }

        // Wormhole climax — bloom when all orbs held
        if(u_progress > 0.95){
            float bloom = (u_progress-0.95)*20.0;
            col += vec3(0.6,0.3,1.0)*bloom*exp(-length(uv)*2.0);
        }

        col = col/(col+0.5);
        col = pow(max(col,0.0), vec3(0.85));
        fragColor = vec4(col, 1.0);
    }`;

    function compile(src, type) {
        const s = gl.createShader(type);
        gl.shaderSource(s,src); gl.compileShader(s);
        if (!gl.getShaderParameter(s,gl.COMPILE_STATUS))
            console.error('[offering]',gl.getShaderInfoLog(s));
        return s;
    }
    const prog = (()=>{
        const p=gl.createProgram();
        gl.attachShader(p,compile(VERT,gl.VERTEX_SHADER));
        gl.attachShader(p,compile(FRAG,gl.FRAGMENT_SHADER));
        gl.linkProgram(p);
        if(!gl.getProgramParameter(p,gl.LINK_STATUS))
            console.error('[offering link]',gl.getProgramInfoLog(p));
        return p;
    })();

    const buf=gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,buf);
    gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,1,-1,1,1,-1,1]),gl.STATIC_DRAW);

    let W=0,H=0;
    function resize(){
        const scale = window.webglRenderScale || 1.0;
        W=canvas.clientWidth*scale; H=canvas.clientHeight*scale;
        canvas.width=W; canvas.height=H;
    }
    window.addEventListener('resize',resize); resize();

    function u(n){return gl.getUniformLocation(prog,n);}
    const t0=performance.now();

    // ---- OSC via state broadcast ----
    // We piggyback on the existing state mechanism by posting game events
    // to a dedicated endpoint that the C++ binary forwards as OSC
    function sendOrbEvent(orbId, held) {
        const host = window.apiHost || 'localhost';
        fetch(`http://${host}:8080/orb`, {
            method: 'POST',
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify({ orb: orbId, held: held ? 1 : 0 })
        }).catch(()=>{});
    }

    // ---- Touch detection ----
    const TOUCH_RADIUS_XY = 0.12; // normalised screen coords
    const TOUCH_RADIUS_Z  = 0.15; // normalised depth
    const HOLD_TIME = 0.8;        // seconds to activate orb

    function checkTouch(dt) {
        const st  = window.instrumentState;
        const hands = (st && st.hands) || [];
        game.activeCount = 0;

        orbs.forEach(orb => {
            let touching = false;
            let touchingHandId = -1;

            hands.forEach(h => {
                const dx = Math.abs((window.mirrorX ? 1-(h.x||0.5) : (h.x||0.5)) - orb.x);
                const dy = Math.abs((h.y||0.5) - orb.y);
                const dz = Math.abs((h.z||0.5) - orb.z);
                if (dx < TOUCH_RADIUS_XY && dy < TOUCH_RADIUS_XY && dz < TOUCH_RADIUS_Z) {
                    touching = true;
                    touchingHandId = h.id || 0;
                }
            });

            const wasHeld = orb.heldBy >= 0;

            if (touching) {
                if (orb.heldBy < 0) {
                    orb.heldBy = touchingHandId;
                    orb.holdDuration = 0;
                    orb.flashT = 0.3;
                }
                orb.holdDuration += dt;
                if (!orb.activated && orb.holdDuration >= HOLD_TIME) {
                    orb.activated = true;
                    orb.flashT = 1.0;
                    sendOrbEvent(orb.id, true);
                }
            } else {
                if (wasHeld) {
                    if (orb.activated) sendOrbEvent(orb.id, false);
                    orb.activated = false;
                }
                orb.heldBy = -1;
                orb.holdDuration = 0;
            }

            if (orb.activated) game.activeCount++;
            orb.flashT = Math.max(0, orb.flashT - dt * 2);
        });

        // Wormhole progress
        const targetProgress = game.activeCount / orbs.length;
        game.wormholeProgress += (targetProgress - game.wormholeProgress) * dt * 0.8;
        game.wormholeProgress = Math.max(0, Math.min(1, game.wormholeProgress));

        // Climax
        if (game.wormholeProgress > 0.98 && !game.allActivated) {
            game.allActivated = true;
            game.climaxTimer = 8.0;
            sendOrbEvent(-1, true); // signal climax
        }
        if (game.allActivated) {
            game.climaxTimer -= dt;
            if (game.climaxTimer <= 0) {
                game.allActivated = false;
                // Reset all orbs to new positions
                orbs.forEach(orb => {
                    orb.x = 0.2 + Math.random() * 0.6;
                    orb.y = 0.2 + Math.random() * 0.6;
                    orb.activated = false;
                    orb.heldBy = -1;
                });
            }
        }
    }

    // ---- Orb movement ----
    function moveOrbs(t, dt) {
        orbs.forEach(orb => {
            // Drift in XY
            orb.x += orb.vx * dt;
            orb.y += orb.vy * dt;

            // Bounce off walls with padding
            const pad = 0.12;
            if (orb.x < pad  || orb.x > 1-pad)  orb.vx *= -1;
            if (orb.y < 0.1  || orb.y > 0.9)     orb.vy *= -1;
            orb.x = Math.max(pad, Math.min(1-pad, orb.x));
            orb.y = Math.max(0.1, Math.min(0.9, orb.y));

            // Z oscillates slowly around bucket zone
            orb.z = orb.zBucket + Math.sin(t * 0.4 + orb.zPhase) * orb.zAmp;
        });
    }

    // ---- Render ----
    let lastT = 0;

    function render() {
        requestAnimationFrame(render);
        const t   = (performance.now()-t0)/1000;
        const dt  = Math.min(t - lastT, 0.05);
        lastT = t;

        moveOrbs(t, dt);
        checkTouch(dt);

        gl.viewport(0,0,W,H);
        gl.useProgram(prog);

        const loc=gl.getAttribLocation(prog,'a_pos');
        gl.bindBuffer(gl.ARRAY_BUFFER,buf);
        gl.enableVertexAttribArray(loc);
        gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);

        gl.uniform1f(u('u_time'), t);
        gl.uniform2f(u('u_resolution'), W, H);
        gl.uniform1f(u('u_progress'), game.wormholeProgress);
        gl.uniform1i(u('u_num_orbs'), orbs.length);

        const st = window.instrumentState;
        const hands = (st && st.hands) || [];
        gl.uniform1i(u('u_num_hands'), hands.length);

        const hd = new Float32Array(32);
        hands.slice(0,8).forEach((h,i) => {
            hd[i*4]   = window.mirrorX ? 1-(h.x||0.5) : (h.x||0.5);
            hd[i*4+1] = h.y||0.5;
            hd[i*4+2] = h.z||0.5;
            hd[i*4+3] = h.g_id||0;
        });
        gl.uniform4fv(u('u_hands'), hd);

        // Pack orb uniforms
        const orbPos  = new Float32Array(16);
        const orbCols = new Float32Array(16);
        orbs.forEach((orb, i) => {
            orbPos[i*4]   = orb.x;
            orbPos[i*4+1] = orb.y;
            orbPos[i*4+2] = orb.z;
            orbPos[i*4+3] = orb.radius;

            const brightness = orb.activated ? 1.5 : (orb.holdDuration > 0 ? 1.0 + orb.holdDuration/HOLD_TIME*0.5 : 1.0);
            orbCols[i*4]   = orb.color[0] * brightness;
            orbCols[i*4+1] = orb.color[1] * brightness;
            orbCols[i*4+2] = orb.color[2] * brightness;
            orbCols[i*4+3] = orb.flashT;
        });
        gl.uniform4fv(u('u_orbs'),    orbPos);
        gl.uniform4fv(u('u_orbCols'), orbCols);

        gl.drawArrays(gl.TRIANGLES, 0, 6);
    }
    render();
})();
