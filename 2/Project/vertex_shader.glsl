#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aSize;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_PointSize = aSize;
    gl_Position = vec4(aPos*(1920/1080), 0.0, 1.0);
}
// g++ -std=c++20 -O2 -Wall -Wextra \
//                                  main.cpp audio.cpp shader.cpp fft.cpp glad.c \
//                                  -I. \
//                                  -o visualizer \
//                                  -lglfw -lGL -ldl -lpthread -lm