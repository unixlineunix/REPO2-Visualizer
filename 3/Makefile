CXX      := g++
CXXFLAGS := -std=c++20 -I.
LDFLAGS  := -lglfw -lGL -lasound -lwebp

OBJS := main.o modes.o shader.o fft.o audio.o glad.o \
        imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o \
        imgui_impl_glfw.o imgui_impl_opengl3.o

MINGW_CXX      := x86_64-w64-mingw32-g++
MINGW_CXXFLAGS := -std=c++20 -I. -I/tmp/glfw_mingw_build -D_GLFW_WIN32
MINGW_LDFLAGS  := -L/tmp/glfw_mingw_build -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++ -static
MINGW_OBJS     := main_mingw.o modes_mingw.o shader_mingw.o fft_mingw.o audio_mingw.o glad_mingw.o \
                  imgui_mingw.o imgui_draw_mingw.o imgui_tables_mingw.o imgui_widgets_mingw.o \
                  imgui_impl_glfw_mingw.o imgui_impl_opengl3_mingw.o

.PHONY: all clean win32

all: vizl

win32: vizl.exe

vizl: $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

vizl.exe: $(MINGW_OBJS)
	$(MINGW_CXX) $^ -o $@ $(MINGW_LDFLAGS)

main_mingw.o: main.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

modes_mingw.o: modes.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

shader_mingw.o: shader.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

fft_mingw.o: fft.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

audio_mingw.o: audio.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

glad_mingw.o: glad.c
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_mingw.o: imgui.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_draw_mingw.o: imgui_draw.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_tables_mingw.o: imgui_tables.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_widgets_mingw.o: imgui_widgets.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_impl_glfw_mingw.o: imgui_impl_glfw.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

imgui_impl_opengl3_mingw.o: imgui_impl_opengl3.cpp
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

glad.o: glad.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(MINGW_OBJS) vizl vizl.exe
