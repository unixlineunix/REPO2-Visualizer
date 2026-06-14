#version 460 core

in vec3 vColor;
out vec4 FragColor;

uniform float uAlpha;
uniform int uIsPoint;
uniform int uBloomRings;

void main() {
    if (uIsPoint == 1) {
        vec2 coord = gl_PointCoord - vec2(0.5);
        float d = length(coord);
        if (d > 0.5) discard;

        if (uBloomRings > 0) {
            float ringIdx = floor(d * 2.0 * float(uBloomRings));
            float rings = float(uBloomRings);
            float stepAlpha = 1.0 - ringIdx / rings;
            float stepBright = 1.0 - ringIdx * 0.5 / rings;
            FragColor = vec4(vColor * max(stepBright, 0.0), uAlpha * stepAlpha);
        } else {
            FragColor = vec4(vColor, uAlpha);
        }
    } else {
        FragColor = vec4(vColor, uAlpha);
    }
}