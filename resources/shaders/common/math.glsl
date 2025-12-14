#ifndef MATH_H
#define MATH_H

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float PI_HALF = 1.57079632679;
const float HALF_PI = 1.57079632679;
const float INV_PI = 1.0 / 3.14159265359;

float saturate(in float x) {
    return clamp(x, 0.0, 1.0);
}

vec2 saturate(in vec2 x) {
    return clamp(x, vec2(0.0), vec2(1.0));
}

vec3 saturate(in vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}

vec4 saturate(in vec4 x) {
    return clamp(x, vec4(0.0), vec4(1.0));
}

// For Octahedron Encoding check out
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/

// Encodes a normalized 3D vector into a vec2.
vec2 octahedronEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    // Branchless hemisphere fold
    vec2 fold = (1.0 - abs(n.yx)) * sign(n.xy);
    n.xy = mix(n.xy, fold, step(n.z, 0.0));
    return n.xy * 0.5 + 0.5;
}

// Decodes a vec2 (in the range [-1, 1]) back into a normalized 3D vector.
vec3 octahedronDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.xy += sign(n.xy) * t;
    return normalize(n);
}
#endif