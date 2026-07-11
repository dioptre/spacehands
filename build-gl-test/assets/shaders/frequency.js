'use strict';
// Frequency spiral — brightness-driven concentric ring pattern.
// Direct port of user's Shadertoy snippet #5.
// Hand count scales the ring density (norm driven by cam brightness).

(function () {
    const canvas = document.getElementById('c');
    const gl = canvas.getContext('webgl2', { antialias: false });
    if (!gl) return;

    const VERT = `#version 300 es
    in vec2 a_pos; out vec2 v_uv;
    void main() { v_uv = a_pos*0.5+0.5; gl_Position=vec4(a_pos,0,1); }`;

    // User's snippet: norm from cam brightness, count = floor(120*norm),
    // inner loop generates concentric hex-ish rings with iTime animation.
    const FRAG = `#version 300 es
    precision highp float;
    in vec2 v_uv; out vec4 fragColor;
    uniform float u_time;
    uniform vec2  u_resolution;
    uniform int   u_num_hands;
    uniform vec4  u_hands[8];
    uniform float u_progress;
    uniform sampler2D u_cam;

    void main() {
        fragColor = vec4(0.0);
        vec2 fragCoord = v_uv * u_resolution;

        vec3 camRgb = 1.0 - texture(u_cam, v_uv).rgb; // invert
        float norm = length(camRgb) / sqrt(3.0);

        // Scale max rings with hand count and progress
        float scale = 1.0 + float(u_num_hands) * 0.4 + u_progress * 0.8;
        int count = int(120.0 * norm * scale);
        count = min(count, 180); // cap for performance

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

        // Blend hand glow on top
        for (int i = 0; i < 8; i++) {
            if (i >= u_num_hands) break;
            float d = distance(v_uv, u_hands[i].xy);
            fragColor.rgb += exp(-d * 12.0) * vec3(0.15, 0.3, 0.6) * 0.5;
        }
    }`;

    function compile(src, type) {
        const s = gl.createShader(type);
        gl.shaderSource(s,src); gl.compileShader(s);
        if (!gl.getShaderParameter(s,gl.COMPILE_STATUS)) console.error(gl.getShaderInfoLog(s));
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
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1,-1,1,-1,-1,1,1,-1,1,1,-1,1]), gl.STATIC_DRAW);

    const camTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, camTex);
    gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,1,1,0,gl.RGBA,gl.UNSIGNED_BYTE,new Uint8Array([0,0,0,255]));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    let W=0,H=0;
    function resize(){
        const scale = window.webglRenderScale || 1.0;
        W=canvas.clientWidth*scale;H=canvas.clientHeight*scale;canvas.width=W;canvas.height=H;
    }
    window.addEventListener('resize',resize); resize();

    function u(n){return gl.getUniformLocation(prog,n);}
    const t0 = performance.now();

    function render() {
        requestAnimationFrame(render);
        if (window.camBitmap){
            gl.bindTexture(gl.TEXTURE_2D,camTex);
            gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,window.camBitmap);
        }
        gl.viewport(0,0,W,H);
        gl.useProgram(prog);
        const loc=gl.getAttribLocation(prog,'a_pos');
        gl.bindBuffer(gl.ARRAY_BUFFER,buf);
        gl.enableVertexAttribArray(loc);
        gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);
        const t=(performance.now()-t0)/1000;
        const st=window.instrumentState;
        const hands=(st&&st.hands)||[];
        gl.uniform1f(u('u_time'),t);
        gl.uniform2f(u('u_resolution'),W,H);
        gl.uniform1i(u('u_num_hands'),hands.length);
        gl.uniform1f(u('u_progress'),(st&&st.progress)||0);
        const hd=new Float32Array(8*4);
        hands.slice(0,8).forEach((h,i)=>{hd[i*4]=h.x||0;hd[i*4+1]=h.y||0;hd[i*4+2]=h.z||0;hd[i*4+3]=h.g_id||0;});
        gl.uniform4fv(u('u_hands'),hd);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D,camTex);
        gl.uniform1i(u('u_cam'),0);
        gl.drawArrays(gl.TRIANGLES,0,6);
    }
    render();
})();
