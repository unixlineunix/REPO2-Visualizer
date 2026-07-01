CXX      := g++
CXXFLAGS := -std=c++20 -I.
LDFLAGS  := -lglfw -lGL -lasound -limgui

OBJS := main.o modes.o audio.o fft.o shader.o vis_state.o glad.o imgui_impl_glfw.o imgui_impl_opengl3.o

# Windows cross-compilation with MinGW
MINGW_CXX      := x86_64-w64-mingw32-g++
MINGW_CXXFLAGS := -std=c++20 -I. -D_GLFW_WIN32
MINGW_LDFLAGS  := -L/tmp/glfw_mingw_build -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++ -static
MINGW_OBJS     := main_mingw.o modes_mingw.o audio_mingw.o fft_mingw.o shader_mingw.o vis_state_mingw.o glad_mingw.o

.PHONY: all clean win32

all: visualizer

win32: visualizer.exe

visualizer: $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

visualizer.exe: $(MINGW_OBJS)
	$(MINGW_CXX) $^ -o $@ $(MINGW_LDFLAGS)

main_mingw.o: main.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

modes_mingw.o: modes.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

audio_mingw.o: audio.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

fft_mingw.o: fft.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

shader_mingw.o: shader.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

vis_state_mingw.o: vis_state.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

glad_mingw.o: glad.c
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

glad.o: glad.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_impl_glfw.o: imgui_impl_glfw.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_impl_opengl3.o: imgui_impl_opengl3.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(MINGW_OBJS) visualizer visualizer.exe
