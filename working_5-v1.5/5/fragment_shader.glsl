#version 460 core

out vec4 FragColor;

in vec3 vColor;

uniform int uIsPoint;
uniform float uAlpha;
uniform float uBloomIntensity;
uniform int uBloomRings;
uniform int uBloomShape; // 0=ball, 1=square, 2=triangle

void main() {
    if (uIsPoint == 1) {
        vec2 coord = gl_PointCoord - vec2(0.5);
        float d = length(coord);

        // Shape mask
        if (uBloomShape == 0) {
            if (d > 0.5) discard;
        } else if (uBloomShape == 2) {
            vec2 tc = gl_PointCoord;
            vec2 tri0 = vec2(0.5, 0.0);
            vec2 tri1 = vec2(0.0, 1.0);
            vec2 tri2 = vec2(1.0, 1.0);
            vec2 e0 = tri1 - tri0, e1 = tri2 - tri1, e2 = tri0 - tri2;
            vec2 p0 = tc - tri0, p1 = tc - tri1, p2 = tc - tri2;
            float c0 = e0.x * p0.y - e0.y * p0.x;
            float c1 = e1.x * p1.y - e1.y * p1.x;
            float c2 = e2.x * p2.y - e2.y * p2.x;
            if ((c0 < 0.0) != (c1 < 0.0) || (c1 < 0.0) != (c2 < 0.0)) discard;
        }
        // square: no discard

        if (uBloomRings > 0) {
            float ringIdx = floor(d * 2.0 * float(uBloomRings));
            float rings = float(uBloomRings);
            float stepAlpha = 1.0 - ringIdx / rings;
            float stepBright = 1.0 - ringIdx * 0.5 / rings;
            vec3 ringColor = vColor * max(stepBright, 0.0);
            FragColor = vec4(ringColor * (1.0 + uBloomIntensity * 0.5), uAlpha * stepAlpha);
        } else {
            vec3 pointColor = vColor * (1.0 + uBloomIntensity * 0.5);
            FragColor = vec4(pointColor, uAlpha);
        }
    } else {
        vec3 color = vColor * (1.0 + uBloomIntensity * 0.5);
        FragColor = vec4(color, uAlpha);
    }
}
