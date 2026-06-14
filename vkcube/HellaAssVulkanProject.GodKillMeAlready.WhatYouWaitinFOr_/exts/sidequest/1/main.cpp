// Image rotation with no aliasing using GLFW + OpenGL + stb_image + Dear ImGui
// Set rotation angle and output resolution, then export

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"

#include <cmath>
#include <cstring>
#include <iostream>

static GLuint texture = 0;
static int tex_width = 0, tex_height = 0;
static float rotation_degrees = 0.0f;
static int out_width = 512, out_height = 512;
static char out_path[256] = "rotated.png";

bool load_texture(const char* path) {
    int channels;
    unsigned char* data = stbi_load(path, &tex_width, &tex_height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load image: " << path << "\n";
        return false;
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    out_width = tex_width;
    out_height = tex_height;
    std::cout << "Loaded " << path << " (" << tex_width << "x" << tex_height << ")\n";
    return true;
}

void render_texture_rotated(float angle) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPushMatrix();
    glRotatef(angle, 0.0f, 0.0f, 1.0f);

    float aspect = (float)tex_width / (float)tex_height;
    float size = 0.8f;
    float w = size * aspect;
    float h = size;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-w, -h);
    glTexCoord2f(1, 1); glVertex2f( w, -h);
    glTexCoord2f(1, 0); glVertex2f( w,  h);
    glTexCoord2f(0, 0); glVertex2f(-w,  h);
    glEnd();

    glPopMatrix();
}

void export_rotated() {
    GLuint fbo, fbo_tex;
    glGenFramebuffersEXT(1, &fbo);
    glGenTextures(1, &fbo_tex);

    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_width, out_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbo_tex, 0);

    glViewport(0, 0, out_width, out_height);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    render_texture_rotated(rotation_degrees);

    unsigned char* pixels = new unsigned char[out_width * out_height * 4];
    glReadPixels(0, 0, out_width, out_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Flip vertically (OpenGL Y is bottom-up)
    for (int y = 0; y < out_height / 2; ++y) {
        unsigned char* row1 = pixels + y * out_width * 4;
        unsigned char* row2 = pixels + (out_height - 1 - y) * out_width * 4;
        for (int x = 0; x < out_width * 4; ++x) {
            unsigned char tmp = row1[x];
            row1[x] = row2[x];
            row2[x] = tmp;
        }
    }

    stbi_write_png(out_path, out_width, out_height, 4, pixels, out_width * 4);
    delete[] pixels;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDeleteFramebuffersEXT(1, &fbo);
    glDeleteTextures(1, &fbo_tex);

    std::cout << "Exported to " << out_path << "\n";
}

int main(int argc, char** argv) {
    const char* image_path = (argc > 1) ? argv[1] : "image.png";

    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Image Rotator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsDark();

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!load_texture(image_path)) {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Rotation Controls");
        ImGui::SliderFloat("Degrees", &rotation_degrees, 0.0f, 360.0f);
        ImGui::InputInt("Output Width", &out_width);
        ImGui::InputInt("Output Height", &out_height);
        ImGui::InputText("Output Path", out_path, sizeof(out_path));
        if (ImGui::Button("Export")) {
            export_rotated();
        }
        ImGui::End();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        render_texture_rotated(rotation_degrees);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &texture);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
