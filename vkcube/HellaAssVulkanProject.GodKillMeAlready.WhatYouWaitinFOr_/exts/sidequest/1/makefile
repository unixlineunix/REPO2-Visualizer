CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I$(HOME)/N/Stb
LIBS := -lglfw -lGL

TARGET := rotator

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
