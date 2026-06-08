'use strict';
// Voronoi scatter — random points mapped to webcam pixels.
// Direct port of user's Shadertoy snippet #2.
// Hands warp the seed distribution.

(function () {
    const canvas = document.getElementById('c');
    const gl = canvas.getContext('webgl2', { antialias: false });
    if (!gl) return;

    const VERT = `#version 300 es
    in vec2 a_pos; out vec2 v_uv;
    void main() { v_uv = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos,0,1); }`;

    const FRAG = `#version 300 es
    precision highp float;
    in vec2 v_uv; out vec4 fragColor;
    uniform float u_time;
    uniform vec2  u_resolution;
    uniform int   u_num_hands;
    uniform vec4  u_hands[8];
    uniform float u_progress;
    uniform sampler2D u_cam;

    #define NUM_POINTS 512
    #define SEED 3

    void main() {
        int random = SEED;
        int a = 1103515245, c = 12345, m = 2147483648;
        vec2 fragCoord = v_uv * u_resolution;
        vec2 o;
        float minDist = 1e7;

        // Hand positions perturb the seed distribution
        float hand_offset = 0.0;
        for (int i = 0; i < 8; i++) {
            if (i >= u_num_hands) break;
            hand_offset += sin(u_hands[i].x * 6.28 + u_time) * 0.1;
        }

        for (int i = 0; i < NUM_POINTS; i++) {
            random = a * random + c;
            o.x = (float(random) / float(m)) * u_resolution.x;
            random = a * random + c;
            o.y = (float(random) / float(m)) * u_resolution.y;

            // Animate points with hand-influenced drift
            o.x += sin(float(i) * 0.1 + u_time * 0.3 + hand_offset) * 20.0 * u_progress;
            o.y += cos(float(i) * 0.07 + u_time * 0.2) * 15.0 * u_progress;

            float d = distance(fragCoord, o);
            if (d < minDist) {
                minDist = d;
                vec2 uv = o / u_resolution;
                uv.x = 1.0 - uv.x;
                fragColor = texture(u_cam, uv) * (1.0 - minDist / 220.0);
            }
        }
        // Add glow around hands
        for (int i = 0; i < 8; i++) {
            if (i >= u_num_hands) break;
            vec2 hp = u_hands[i].xy * u_resolution;
            float d = distance(fragCoord, hp);
            fragColor.rgb += exp(-d * 0.008) * vec3(0.1, 0.2, 0.5) * 0.4;
        }
    }`;

    function compile(src, type) {
        const s = gl.createShader(type);
        gl.shaderSource(s, src); gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS))
            console.error(gl.getShaderInfoLog(s));
        return s;
    }
    const prog = (() => {
        const p = gl.createProgram();
        gl.attachShader(p, compile(VERT, gl.VERTEX_SHADER));
        gl.attachShader(p, compile(FRAG, gl.FRAGMENT_SHADER));
        gl.linkProgram(p); return p;
    })();

    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER,
        new Float32Array([-1,-1,1,-1,-1,1,1,-1,1,1,-1,1]), gl.STATIC_DRAW);

    const camTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, camTex);
    gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,1,1,0,gl.RGBA,gl.UNSIGNED_BYTE,new Uint8Array([0,0,0,255]));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    let W=0, H=0;
    function resize() {
        W = canvas.clientWidth; H = canvas.clientHeight;
        canvas.width = W; canvas.height = H;
    }
    window.addEventListener('resize', resize); resize();

    function u(n) { return gl.getUniformLocation(prog, n); }
    const t0 = performance.now();

    function render() {
        requestAnimationFrame(render);
        if (window.camBitmap) {
            gl.bindTexture(gl.TEXTURE_2D, camTex);
            gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,window.camBitmap);
        }
        gl.viewport(0,0,W,H);
        gl.useProgram(prog);
        const loc = gl.getAttribLocation(prog,'a_pos');
        gl.bindBuffer(gl.ARRAY_BUFFER,buf);
        gl.enableVertexAttribArray(loc);
        gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);
        const t = (performance.now()-t0)/1000;
        const st = window.instrumentState;
        const hands = (st&&st.hands)||[];
        gl.uniform1f(u('u_time'),t);
        gl.uniform2f(u('u_resolution'),W,H);
        gl.uniform1i(u('u_num_hands'),hands.length);
        gl.uniform1f(u('u_progress'),(st&&st.progress)||0);
        const hd = new Float32Array(8*4);
        hands.slice(0,8).forEach((h,i)=>{hd[i*4]=h.x||0;hd[i*4+1]=h.y||0;hd[i*4+2]=h.z||0;hd[i*4+3]=h.g_id||0;});
        gl.uniform4fv(u('u_hands'),hd);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D,camTex);
        gl.uniform1i(u('u_cam'),0);
        gl.drawArrays(gl.TRIANGLES,0,6);
    }
    render();
})();
