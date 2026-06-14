#version 460 core

out vec4 FragColor;

in vec3 vColor;

uniform int uIsPoint;
uniform float uAlpha;

void main() {
    if (uIsPoint == 1) {
        // Simple circular point for Oscilloscope/Lissajous points
        vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
        if (dot(circCoord, circCoord) > 1.0) {
            discard;
        }
    }
    FragColor = vec4(vColor, uAlpha);
}
