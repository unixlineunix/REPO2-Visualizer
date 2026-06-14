#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 texCoord;

uniform mat4 uProj;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    texCoord = aTexCoord;
}
