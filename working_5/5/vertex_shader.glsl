#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aSize;

out vec3 vColor;

uniform float uZoom;

void main() {
    vColor = aColor;
    gl_PointSize = aSize;
    gl_Position = vec4(aPos * uZoom, 0.0, 1.0);
}