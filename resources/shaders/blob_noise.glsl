// Discalimer: These are AI generated


// --------------------------------------------------------
// NOISE BASICS
// --------------------------------------------------------
float hash(vec3 p) {
    p  = fract( p*0.3183099+.1 );
    p *= 17.0;
    return fract( p.x*p.y*p.z*(p.x+p.y+p.z) );
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
    return mix(mix(mix( hash(i+vec3(0,0,0)), hash(i+vec3(1,0,0)),f.x),
    mix( hash(i+vec3(0,1,0)), hash(i+vec3(1,1,0)),f.x),f.y),
    mix(mix( hash(i+vec3(0,0,1)), hash(i+vec3(1,0,1)),f.x),
    mix( hash(i+vec3(0,1,1)), hash(i+vec3(1,1,1)),f.x),f.y),f.z);
}

// Helper for Flux style
float simpleFbm(vec3 p) {
    float f = 0.0;
    f += 0.5000 * noise(p); p *= 2.0;
    f += 0.2500 * noise(p); p *= 2.0;
    return f;
}

// --------------------------------------------------------
// STYLE 0: OILY GLOOP
// Swirling liquid.
// --------------------------------------------------------
float slimeGloop(vec3 p, float time) {
    p *= 2.0;
    vec3 offset = vec3(
    noise(p + vec3(time * 0.5, 0.0, 0.0)),
    noise(p + vec3(0.0, time * 0.5, 0.0)),
    noise(p + vec3(0.0, 0.0, time * 0.5))
    );
    float n = noise(p + offset * 2.0);
    return smoothstep(0.2, 0.8, n);
}

// --------------------------------------------------------
// STYLE 1: CRYSTAL
// Sharp geometric ridges (Voronoi-ish).
// --------------------------------------------------------
float slimeCrystal(vec3 p, float time) {
    p *= 8.0;
    vec3 i = floor(p);
    vec3 f = fract(p);
    float minStart = 1.0;

    for(int z=-1; z<=1; z++)
    for(int y=-1; y<=1; y++)
    for(int x=-1; x<=1; x++) {
        vec3 neighbor = vec3(float(x),float(y),float(z));
        vec3 offset = vec3(
        sin(time * 2.0 + dot(i + neighbor, vec3(12.9, 78.2, 45.1))),
        cos(time * 1.5 + dot(i + neighbor, vec3(39.3, 11.1, 87.4))),
        sin(time * 1.0 + dot(i + neighbor, vec3(73.1, 52.2, 19.1)))
        ) * 0.4;
        vec3 diff = neighbor + 0.5 + offset - f;
        float dist = length(diff);
        minStart = min(minStart, dist);
    }
    return 1.0 - (minStart * minStart);
}

// --------------------------------------------------------
// STYLE 2: COSMIC FLUX
// Double domain warping (Gas giant storms).
// --------------------------------------------------------
float slimeFlux(vec3 p, float time) {
    p *= 2.5;

    // Warp 1
    vec3 q = vec3(
    simpleFbm(p),
    simpleFbm(p + vec3(5.2, 1.3, 2.8)),
    simpleFbm(p + vec3(1.1, 6.2, 3.9))
    );

    // Warp 2
    vec3 r = vec3(
    simpleFbm(p + 4.0 * q + vec3(time, time * 0.5, 0.0)),
    simpleFbm(p + 4.0 * q + vec3(0.0, time * 0.2, time)),
    simpleFbm(p + 4.0 * q + vec3(time * 0.1, 0.0, 0.0))
    );

    return simpleFbm(p + 4.0 * r);
}

// --------------------------------------------------------
// STYLE 4: ALIEN BRAIN (Optional extra)
// --------------------------------------------------------
float slimeBrain(vec3 pos, float time) {
    vec3 p = pos * 4.0;
    p.y += time * 0.2;
    float val = 0.0;
    float amp = 1.0;
    for(int i=0; i<3; i++) {
        val += abs(noise(p) * 2.0 - 1.0) * amp;
        p *= 2.0;
        amp *= 0.5;
    }
    return pow(1.0 - val, 2.0);
}

// --------------------------------------------------------
// HELPER: Generate Normal
// --------------------------------------------------------
vec3 perturbNormal(vec3 pos, float time, int styleIdx, float strength) {
    vec2 e = vec2(0.01, 0.0);
    float h = 0.0;
    float hx = 0.0;
    float hy = 0.0;

    if (styleIdx == 0) {
        h  = slimeGloop(pos, time);
        hx = slimeGloop(pos + e.xyy, time);
        hy = slimeGloop(pos + e.yxy, time);
    } else if (styleIdx == 1) {
        h  = slimeCrystal(pos, time);
        hx = slimeCrystal(pos + e.xyy, time);
        hy = slimeCrystal(pos + e.yxy, time);
    } else if (styleIdx == 2) {
        h  = -slimeCrystal(pos, time);
        hx = -slimeCrystal(pos + e.xyy, time);
        hy = -slimeCrystal(pos + e.yxy, time);
    } else if (styleIdx == 3) {
        h  = slimeFlux(pos, time);
        hx = slimeFlux(pos + e.xyy, time);
        hy = slimeFlux(pos + e.yxy, time);
    } else {
        return vec3(0.0, 0.0, 1.0);
    }

    vec2 d = (vec2(hx, hy) - h) / e.x;
    return normalize(vec3(-d * strength, 1.0));
}