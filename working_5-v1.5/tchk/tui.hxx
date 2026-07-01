#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
};

struct Cell {
    std::string symbol = " ";
    Color fg = {220, 220, 220};
    Color bg = {15, 17, 26};
    bool has_fg = false;
    bool has_bg = false;
};

class TuiRenderer {
public:
    TuiRenderer();
    ~TuiRenderer();

    void resize(int w, int h);
    void clear();
    void render();

    // Helper functions for drawing
    void draw_cell(int x, int y, const std::string& sym, Color fg, bool has_fg = true, Color bg = {}, bool has_bg = false);
    void draw_string(int x, int y, const std::string& str, Color fg, bool has_fg = true, Color bg = {}, bool has_bg = false);
    void draw_box(int x, int y, int w, int h, const std::string& title, Color border_color, Color title_color);
    void draw_bar(int x, int y, int length, double percent, Color start, Color end);
    void draw_graph(int x, int y, int w, int h, const std::vector<double>& history, double max_val, Color low_color, Color high_color);

    int get_width() const { return width; }
    int get_height() const { return height; }

    // Constants for themes
    static const Color BG_DARK;
    static const Color BORDER;
    static const Color TEXT_WHITE;
    static const Color TEXT_GRAY;
    static const Color TEAL;
    static const Color CYAN;
    static const Color GREEN;
    static const Color YELLOW;
    static const Color ORANGE;
    static const Color RED;
    static const Color MAGENTA;

    static Color interpolate(Color c1, Color c2, double t);

private:
    int width = 0;
    int height = 0;
    std::vector<std::vector<Cell>> grid;
};
