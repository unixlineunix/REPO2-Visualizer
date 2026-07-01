#version 460 core

out vec4 FragColor;

in vec3 vColor;

uniform int uIsPoint;
uniform float uAlpha;
uniform float uBloomIntensity;
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
