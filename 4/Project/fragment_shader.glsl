#version 460 core

in vec3 vColor;
out vec4 FragColor;

uniform float uAlpha;
uniform int uIsPoint;

void main() {
    if (uIsPoint == 1) {
        vec2 coord = gl_PointCoord - vec2(0.5);
        if (dot(coord, coord) > 0.25) {
            discard;
        }
    }
    FragColor = vec4(vColor, uAlpha);
}