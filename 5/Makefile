CXX      := g++
CXXFLAGS := -std=c++20 -I.
LDFLAGS  := -lglfw -lGL -lasound

OBJS := main.o modes.o audio.o fft.o shader.o vis_state.o glad.o

.PHONY: all clean

all: visualizer

visualizer: $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

glad.o: glad.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) visualizer
