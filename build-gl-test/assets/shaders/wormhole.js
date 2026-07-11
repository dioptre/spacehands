'use strict';
// Frequency shader — fast hand-reactive concentric rings.
// Each hand creates a ripple centre. Background always animates.

(function () {
    const canvas = document.getElementById('c');
    const gl = canvas.getContext('webgl2', { antialias: false });
    if (!gl) return;

    const VERT = `#version 300 es
    in vec2 a_pos; out vec2 v_uv;
    void main() { v_uv = a_pos*0.5+0.5; gl_Position=vec4(a_pos,0,1); }`;

    const FRAG = `#version 300 es
    precision highp float;
    in vec2 v_uv; out vec4 fragColor;
    uniform float u_time;
    uniform vec2  u_resolution;
    uniform int   u_num_hands;
    uniform vec4  u_hands[8];
    uniform float u_progress;
    uniform int   u_level;

    // Voronoi starfield (from user's Shadertoy snippet)
    vec3 hash3(vec2 p) {
        vec3 q = vec3(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3)),dot(p,vec2(419.2,371.9)));
        return fract(sin(q)*43758.5453);
    }
    float voronoise(vec2 p, float u, float v) {
        float k = 1.0+63.0*pow(1.0-v,6.0);
        vec2 i=floor(p), f=fract(p), a=vec2(0.0);
        for(int y=-2;y<=2;y++) for(int x=-2;x<=2;x++) {
            vec2 g=vec2(float(x),float(y));
            vec3 o=hash3(i+g)*vec3(u,u,1.0);
            vec2 d=g-f+o.xy;
            float w=pow(1.0-smoothstep(0.0,1.414,length(d)),k);
            a+=vec2(o.z*w,w);
        }
        return a.x/a.y;
    }
    float slitDist(vec2 pt) { return abs(pt.x); }
    vec3 starfield(vec2 pt, float time) {
        pt = pt * 1.2;
        float rInv = 0.125 / slitDist(pt);
        pt = pt * rInv - vec2(rInv + 2.5*mod(time,60.0), 0.0);
        return vec3(0.506,0.239,0.612) * (voronoise(5.0*pt,1.0,1.0) + 0.24*rInv);
    }

    float rings(vec2 uv, vec2 centre, float t, float strength) {
        float d = distance(uv, centre);
        float r = sin(d * 20.0 - t * 5.0) * 0.5 + 0.5;
        r = pow(r, 4.0);
        return r * exp(-d * 5.0) * strength;
    }

    void main() {
        vec2 uv = v_uv * 2.0 - 1.0;
        uv.x *= u_resolution.x / u_resolution.y;

        // Starfield background — always on
        vec3 col = starfield(uv, u_time) * 1.5;

        // Hand rings at hand positions
        for (int i = 0; i < 8; i++) {
            if (i >= u_num_hands) break;
            vec2 hpos = (u_hands[i].xy * 2.0 - 1.0);
            hpos.x *= u_resolution.x / u_resolution.y;
            float r = rings(uv, hpos, u_time, 3.0);
            float lf = float(u_level) / 5.0;
            vec3 ringCol = mix(vec3(0.3,0.6,1.0), vec3(1.0,0.5,0.1), lf);
            col += ringCol * r;
            // Bright core
            col += ringCol * exp(-distance(uv,hpos)*15.0) * 2.0;
        }

        // Tonemap
        col = col / (col + 0.5);
        col = pow(max(col,0.0), vec3(0.8));
        fragColor = vec4(col, 1.0);
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
        gl.uniform1i(u('u_level'),(st&&st.level)||0);
        const hd=new Float32Array(8*4);
        hands.slice(0,8).forEach((h,i)=>{hd[i*4]=h.x||0;hd[i*4+1]=h.y||0;hd[i*4+2]=h.z||0;hd[i*4+3]=h.g_id||0;});
        gl.uniform4fv(u('u_hands'),hd);
        gl.drawArrays(gl.TRIANGLES,0,6);
    }
    render();
})();
