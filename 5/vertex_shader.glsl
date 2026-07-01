#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aSize;

out vec3 vColor;
flat out vec2 vWinCenter;
flat out float vPointSize;

uniform float uZoom;
uniform float uPointScale = 1.0;
uniform vec2 uViewportSize;

void main() {
    vColor = aColor;
    gl_PointSize = aSize * uPointScale;
    gl_Position = vec4(aPos * uZoom, 0.0, 1.0);
    vec2 ndc = aPos * uZoom;
    vWinCenter = (ndc * 0.5 + 0.5) * uViewportSize;
    vPointSize = gl_PointSize;
}
