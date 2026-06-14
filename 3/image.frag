#version 460 core
in vec2 texCoord;
out vec4 outColor;

uniform sampler2D uTexture;
uniform float uAlpha;
uniform bool uCircular;

void main() {
    if (uCircular) {
        vec2 c = texCoord - vec2(0.5);
        if (dot(c, c) > 0.25) discard;
    }
    outColor = texture(uTexture, texCoord);
    outColor.a *= uAlpha;
}
