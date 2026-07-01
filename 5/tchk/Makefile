CXX = g++
CXXFLAGS = -O3 -Wall -Wextra -std=c++17

TARGET = tchk_monitor
SRCS = main.cpp stats.cpp tui.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
