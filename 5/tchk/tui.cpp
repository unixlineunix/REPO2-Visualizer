#include "tui.hxx"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

// Theme definition
const Color TuiRenderer::BG_DARK = {12, 14, 20};       // Rich dark background
const Color TuiRenderer::BORDER = {45, 60, 80};        // Sleek frame borders
const Color TuiRenderer::TEXT_WHITE = {235, 240, 250};  // Bright white
const Color TuiRenderer::TEXT_GRAY = {110, 125, 145};   // Muted gray
const Color TuiRenderer::TEAL = {30, 200, 180};        // Cold accent
const Color TuiRenderer::CYAN = {50, 180, 240};        // Cool blue accent
const Color TuiRenderer::GREEN = {60, 220, 110};       // CPU Low
const Color TuiRenderer::YELLOW = {230, 190, 40};      // CPU Medium
const Color TuiRenderer::ORANGE = {240, 120, 30};      // CPU Medium-High
const Color TuiRenderer::RED = {245, 65, 85};          // CPU High / Error
const Color TuiRenderer::MAGENTA = {210, 60, 210};     // Swap / Extreme

static std::vector<std::string> split_utf8(const std::string& str) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.size();) {
        int len = 1;
        unsigned char c = str[i];
        if ((c & 0x80) == 0) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        chars.push_back(str.substr(i, len));
        i += len;
    }
    return chars;
}

Color TuiRenderer::interpolate(Color c1, Color c2, double t) {
    t = std::max(0.0, std::min(1.0, t));
    uint8_t r = c1.r + (c2.r - c1.r) * t;
    uint8_t g = c1.g + (c2.g - c1.g) * t;
    uint8_t b = c1.b + (c2.b - c1.b) * t;
    return {r, g, b};
}

TuiRenderer::TuiRenderer() {
    // Hide cursor and clear screen at start
    std::cout << "\033[?25l\033[2J";
}

TuiRenderer::~TuiRenderer() {
    // Show cursor and reset colors on destruction
    std::cout << "\033[?25h\033[0m\n";
}

void TuiRenderer::resize(int w, int h) {
    width = w;
    height = h;
    grid.assign(height, std::vector<Cell>(width));
    clear();
}

void TuiRenderer::clear() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            grid[y][x].symbol = " ";
            grid[y][x].fg = TEXT_WHITE;
            grid[y][x].bg = BG_DARK;
            grid[y][x].has_fg = false;
            grid[y][x].has_bg = true;
        }
    }
}

void TuiRenderer::draw_cell(int x, int y, const std::string& sym, Color fg, bool has_fg, Color bg, bool has_bg) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    grid[y][x].symbol = sym;
    grid[y][x].fg = fg;
    grid[y][x].bg = bg;
    grid[y][x].has_fg = has_fg;
    grid[y][x].has_bg = has_bg;
}

void TuiRenderer::draw_string(int x, int y, const std::string& str, Color fg, bool has_fg, Color bg, bool has_bg) {
    auto syms = split_utf8(str);
    for (int i = 0; i < (int)syms.size(); ++i) {
        if (x + i >= width) break;
        draw_cell(x + i, y, syms[i], fg, has_fg, bg, has_bg);
    }
}

void TuiRenderer::draw_box(int x, int y, int w, int h, const std::string& title, Color border_color, Color title_color) {
    if (w <= 2 || h <= 2) return;

    // Corners
    draw_cell(x, y, "┌", border_color);
    draw_cell(x + w - 1, y, "┐", border_color);
    draw_cell(x, y + h - 1, "└", border_color);
    draw_cell(x + w - 1, y + h - 1, "┘", border_color);

    // Vertical borders
    for (int j = 1; j < h - 1; ++j) {
        draw_cell(x, y + j, "│", border_color);
        draw_cell(x + w - 1, y + j, "│", border_color);
    }

    // Horizontal borders and title
    int title_start = x + 2;
    int title_end = title_start + title.length();
    
    for (int i = 1; i < w - 1; ++i) {
        int screen_x = x + i;
        if (!title.empty() && screen_x >= title_start && screen_x < title_end) {
            std::string ch(1, title[screen_x - title_start]);
            draw_cell(screen_x, y, ch, title_color);
        } else if (!title.empty() && (screen_x == title_start - 1 || screen_x == title_end)) {
            draw_cell(screen_x, y, " ", border_color); // Space before/after title
        } else {
            draw_cell(screen_x, y, "─", border_color);
        }
        draw_cell(screen_x, y + h - 1, "─", border_color);
    }
}

void TuiRenderer::draw_bar(int x, int y, int length, double percent, Color start, Color end) {
    if (length <= 0) return;
    percent = std::max(0.0, std::min(100.0, percent));
    
    double val = (percent / 100.0) * length;
    int filled = (int)val;
    double fraction = val - filled;

    for (int i = 0; i < length; ++i) {
        double t = (double)i / length;
        Color color = interpolate(start, end, t);

        if (i < filled) {
            draw_cell(x + i, y, "█", color);
        } else if (i == filled && fraction > 0.0) {
            std::string block;
            int idx = (int)(fraction * 8);
            if (idx <= 1) block = "░";
            else if (idx <= 3) block = "▒";
            else if (idx <= 6) block = "▓";
            else block = "█";
            draw_cell(x + i, y, block, color);
        } else {
            draw_cell(x + i, y, "░", TEXT_GRAY);
        }
    }
}

void TuiRenderer::draw_graph(int x, int y, int w, int h, const std::vector<double>& history, double max_val, Color low_color, Color high_color) {
    if (w <= 0 || h <= 0 || history.empty()) return;

    int draw_w = std::min((int)history.size(), w);
    int start_idx = history.size() - draw_w;

    for (int i = 0; i < draw_w; ++i) {
        double val = history[start_idx + i];
        double pct = val / max_val;
        if (pct > 1.0) pct = 1.0;
        if (pct < 0.0) pct = 0.0;

        double float_height = pct * h;
        int screen_x = x + w - draw_w + i;

        for (int row = 0; row < h; ++row) {
            int screen_y = y + h - 1 - row; // Bottom up
            double row_fill = float_height - row;

            std::string block = " ";
            Color cell_color = interpolate(low_color, high_color, pct);

            if (row_fill >= 1.0) {
                block = "█";
            } else if (row_fill > 0.0) {
                int idx = (int)(row_fill * 8);
                if (idx <= 0) block = " ";
                else if (idx == 1) block = " ";
                else if (idx == 2) block = "▂";
                else if (idx == 3) block = "▃";
                else if (idx == 4) block = "▄";
                else if (idx == 5) block = "▅";
                else if (idx == 6) block = "▆";
                else if (idx == 7) block = "▇";
                else block = "█";
            } else {
                // Background of the graph (draw faint dot for empty spaces if you want grid lines)
                if (row % 2 == 0 && i % 4 == 0) {
                    block = "·";
                    cell_color = TEXT_GRAY;
                } else {
                    continue; // Leave background cell as-is
                }
            }

            draw_cell(screen_x, screen_y, block, cell_color);
        }
    }
}

void TuiRenderer::render() {
    std::ostringstream out;
    out << "\033[H"; // Move cursor to top-left home position

    Color current_fg = {0, 0, 0};
    Color current_bg = {0, 0, 0};
    bool fg_set = false;
    bool bg_set = false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Cell& cell = grid[y][x];

            // Handle background color
            if (cell.has_bg) {
                if (!bg_set || !(current_bg == cell.bg)) {
                    out << "\033[48;2;" << (int)cell.bg.r << ";" << (int)cell.bg.g << ";" << (int)cell.bg.b << "m";
                    current_bg = cell.bg;
                    bg_set = true;
                }
            } else {
                if (bg_set) {
                    out << "\033[49m"; // default background
                    bg_set = false;
                }
            }

            // Handle foreground color
            if (cell.has_fg) {
                if (!fg_set || !(current_fg == cell.fg)) {
                    out << "\033[38;2;" << (int)cell.fg.r << ";" << (int)cell.fg.g << ";" << (int)cell.fg.b << "m";
                    current_fg = cell.fg;
                    fg_set = true;
                }
            } else {
                if (fg_set) {
                    out << "\033[39m"; // default foreground
                    fg_set = false;
                }
            }

            out << cell.symbol;
        }
        if (y < height - 1) {
            out << "\n";
        }
    }

    out << "\033[0m"; // Reset all attributes
    std::cout << out.str() << std::flush;
}
