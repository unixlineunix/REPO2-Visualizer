#include "tui.hxx"
#include "stats.hxx"
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>
#include <signal.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

// Terminal raw mode management
static struct termios orig_termios;
static bool raw_mode_enabled = false;

void disable_raw_mode() {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1) {
        raw_mode_enabled = true;
    }
}

// Key code constants
const int KEY_UP = 1000;
const int KEY_DOWN = 1001;
const int KEY_LEFT = 1002;
const int KEY_RIGHT = 1003;
const int KEY_ESC = 27;

int read_key() {
    char c;
    int nread = read(STDIN_FILENO, &c, 1);
    if (nread <= 0) return -1;

    if (c == '\033') {
        char seq[3];
        // Read next two chars of escape sequence
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return KEY_ESC;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
        }
        return KEY_ESC;
    }
    return c;
}

std::string format_bytes(unsigned long long bytes) {
    double d = bytes;
    const char* units[] = {"B", "K", "M", "G", "T"};
    int i = 0;
    while (d >= 1024 && i < 4) {
        d /= 1024;
        i++;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f%s", d, units[i]);
    return buf;
}

enum class SortMode {
    CPU,
    MEM,
    PID,
    NAME
};

int main() {
    // Check if terminal is a tty
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        std::cerr << "Error: Standard input/output must be a TTY.\n";
        return 1;
    }

    enable_raw_mode();

    TuiRenderer renderer;
    StatsReader stats_reader;
    SystemInfo sys_info;

    // View histories for graphs
    std::vector<double> cpu_history;
    std::vector<double> mem_history;

    SortMode sort_mode = SortMode::CPU;
    int selected_proc_idx = 0;
    int proc_scroll_offset = 0;

    auto last_stats_update = std::chrono::steady_clock::now();
    bool running = true;
    bool force_redraw = true;

    // Run first update to get valid data
    stats_reader.update(sys_info);

    while (running) {
        // Query terminal size
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
            w.ws_col = 80;
            w.ws_row = 24;
        }

        if (w.ws_col != renderer.get_width() || w.ws_row != renderer.get_height()) {
            renderer.resize(w.ws_col, w.ws_row);
            force_redraw = true;
        }

        auto now = std::chrono::steady_clock::now();
        bool stats_updated = false;

        // Update statistics every 800ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_update).count() >= 800) {
            stats_reader.update(sys_info);
            
            // Record history
            cpu_history.push_back(sys_info.total_cpu_usage);
            mem_history.push_back(sys_info.mem_percent);

            last_stats_update = now;
            stats_updated = true;
        }

        // Handle drawing
        if (stats_updated || force_redraw) {
            renderer.clear();

            int width = renderer.get_width();
            int height = renderer.get_height();

            if (width < 60 || height < 15) {
                renderer.draw_string(2, height / 2, "Terminal too small. Please resize!", TuiRenderer::RED);
                renderer.render();
                force_redraw = false;
                
                // Sleep brief layout delay to avoid CPU burning on small terminal resize loops
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // --- LAYOUT GEOMETRY ---
            int left_w = width * 0.58;
            int right_x = left_w;
            int right_w = width - left_w;

            int cpu_h = height * 0.50;
            int mem_y = cpu_h;
            int mem_h = height - cpu_h;

            // --- CPU BOX DRAWING ---
            std::string cpu_title = "[ CPU - " + std::to_string((int)sys_info.total_cpu_usage) + "% ]";
            renderer.draw_box(0, 0, left_w, cpu_h, cpu_title, TuiRenderer::BORDER, TuiRenderer::TEAL);

            // CPU Header info
            std::string uptime_txt = "Uptime: " + sys_info.uptime_str;
            renderer.draw_string(left_w - uptime_txt.length() - 2, 0, uptime_txt, TuiRenderer::TEXT_GRAY);

            // Draw CPU Graph
            int cpu_graph_w = left_w - 24;
            int cpu_graph_h = cpu_h - 4;
            if (cpu_graph_w > 5 && cpu_graph_h > 1) {
                // Ensure history size fits graph
                if (cpu_history.size() > (size_t)cpu_graph_w) {
                    cpu_history.erase(cpu_history.begin(), cpu_history.begin() + (cpu_history.size() - cpu_graph_w));
                }
                renderer.draw_graph(2, 2, cpu_graph_w, cpu_graph_h, cpu_history, 100.0, TuiRenderer::GREEN, TuiRenderer::RED);
            }

            // Draw Cores
            int cores_x = left_w - 20;
            int num_cores = sys_info.cores.size();
            int max_cores_rows = cpu_h - 3;
            
            if (num_cores <= max_cores_rows) {
                for (int i = 0; i < num_cores && i < max_cores_rows; ++i) {
                    int core_y = 1 + i;
                    auto& core = sys_info.cores[i];
                    renderer.draw_string(cores_x, core_y, core.name, TuiRenderer::TEXT_GRAY);
                    renderer.draw_bar(cores_x + 6, core_y, 7, core.usage_percent, TuiRenderer::GREEN, TuiRenderer::RED);
                    renderer.draw_string(cores_x + 14, core_y, std::to_string((int)core.usage_percent) + "%", TuiRenderer::TEXT_WHITE);
                }
            } else {
                // Dual column for many cores
                int col1_x = left_w - 21;
                int col2_x = left_w - 11;
                int rows_needed = (num_cores + 1) / 2;
                for (int i = 0; i < num_cores; ++i) {
                    int col = i / rows_needed;
                    int row = i % rows_needed;
                    if (row >= max_cores_rows) continue;
                    
                    int target_x = (col == 0) ? col1_x : col2_x;
                    int core_y = 1 + row;
                    auto& core = sys_info.cores[i];
                    std::string short_name = "C" + std::to_string(i);

                    renderer.draw_string(target_x, core_y, short_name, TuiRenderer::TEXT_GRAY);
                    renderer.draw_bar(target_x + 3, core_y, 3, core.usage_percent, TuiRenderer::GREEN, TuiRenderer::RED);
                    renderer.draw_string(target_x + 7, core_y, std::to_string((int)core.usage_percent) + "%", TuiRenderer::TEXT_WHITE);
                }
            }


            // --- MEMORY BOX DRAWING ---
            std::string mem_title = "[ MEMORY / SWAP ]";
            renderer.draw_box(0, mem_y, left_w, mem_h, mem_title, TuiRenderer::BORDER, TuiRenderer::CYAN);

            // RAM info
            renderer.draw_string(2, mem_y + 1, "RAM ", TuiRenderer::TEXT_WHITE);
            renderer.draw_bar(7, mem_y + 1, left_w - 28, sys_info.mem_percent, TuiRenderer::TEAL, TuiRenderer::CYAN);
            std::string ram_vals = format_bytes(sys_info.mem_used) + "/" + format_bytes(sys_info.mem_total);
            renderer.draw_string(left_w - 20, mem_y + 1, ram_vals, TuiRenderer::TEXT_WHITE);
            renderer.draw_string(left_w - 7, mem_y + 1, std::to_string((int)sys_info.mem_percent) + "%", TuiRenderer::TEAL);

            // SWAP info
            renderer.draw_string(2, mem_y + 2, "SWAP", TuiRenderer::TEXT_WHITE);
            renderer.draw_bar(7, mem_y + 2, left_w - 28, sys_info.swap_percent, TuiRenderer::TEAL, TuiRenderer::MAGENTA);
            std::string swap_vals = format_bytes(sys_info.swap_used) + "/" + format_bytes(sys_info.swap_total);
            renderer.draw_string(left_w - 20, mem_y + 2, swap_vals, TuiRenderer::TEXT_WHITE);
            renderer.draw_string(left_w - 7, mem_y + 2, std::to_string((int)sys_info.swap_percent) + "%", TuiRenderer::MAGENTA);

            // Load Averages
            std::stringstream load_ss;
            load_ss << std::fixed << std::setprecision(2) << "Load avg: " << sys_info.load_avg[0] << " " << sys_info.load_avg[1] << " " << sys_info.load_avg[2];
            renderer.draw_string(2, mem_y + 3, load_ss.str(), TuiRenderer::TEXT_GRAY);

            // RAM Graph
            int mem_graph_w = left_w - 4;
            int mem_graph_h = mem_h - 5;
            if (mem_graph_w > 5 && mem_graph_h > 1) {
                if (mem_history.size() > (size_t)mem_graph_w) {
                    mem_history.erase(mem_history.begin(), mem_history.begin() + (mem_history.size() - mem_graph_w));
                }
                renderer.draw_graph(2, mem_y + 4, mem_graph_w, mem_graph_h, mem_history, 100.0, TuiRenderer::CYAN, TuiRenderer::MAGENTA);
            }


            // --- PROCESSES BOX DRAWING ---
            std::string sort_indicator = "";
            switch (sort_mode) {
                case SortMode::CPU: sort_indicator = "CPU%"; break;
                case SortMode::MEM: sort_indicator = "MEM%"; break;
                case SortMode::PID: sort_indicator = "PID"; break;
                case SortMode::NAME: sort_indicator = "NAME"; break;
            }
            std::string proc_title = "[ PROCESSES - Sort: " + sort_indicator + " ]";
            renderer.draw_box(right_x, 0, right_w, height, proc_title, TuiRenderer::BORDER, TuiRenderer::YELLOW);

            // Process list sorting
            std::vector<ProcessInfo> sorted_procs = sys_info.processes;
            std::sort(sorted_procs.begin(), sorted_procs.end(), [sort_mode](const ProcessInfo& a, const ProcessInfo& b) {
                switch (sort_mode) {
                    case SortMode::CPU:
                        if (a.cpu_percent != b.cpu_percent) return a.cpu_percent > b.cpu_percent;
                        return a.mem_percent > b.mem_percent;
                    case SortMode::MEM:
                        if (a.mem_percent != b.mem_percent) return a.mem_percent > b.mem_percent;
                        return a.cpu_percent > b.cpu_percent;
                    case SortMode::PID: return a.pid < b.pid;
                    case SortMode::NAME: return a.name < b.name;
                }
                return false;
            });

            // Adjust selected index limits
            int proc_count = sorted_procs.size();
            if (selected_proc_idx < 0) selected_proc_idx = 0;
            if (selected_proc_idx >= proc_count) selected_proc_idx = proc_count - 1;

            int max_proc_rows = height - 5;
            if (selected_proc_idx < proc_scroll_offset) {
                proc_scroll_offset = selected_proc_idx;
            }
            if (selected_proc_idx >= proc_scroll_offset + max_proc_rows) {
                proc_scroll_offset = selected_proc_idx - max_proc_rows + 1;
            }

            // Headers
            // Form columns dynamically depending on width
            int pid_col_w = 7;
            int cpu_col_w = 6;
            int mem_col_w = 6;
            int name_col_w = right_w - pid_col_w - cpu_col_w - mem_col_w - 4;

            std::stringstream header_ss;
            header_ss << std::left 
                      << std::setw(pid_col_w) << "PID"
                      << std::setw(cpu_col_w) << "CPU%"
                      << std::setw(mem_col_w) << "MEM%"
                      << "NAME";
            renderer.draw_string(right_x + 2, 1, header_ss.str(), TuiRenderer::TEXT_GRAY);

            // Draw divider
            std::string divider(right_w - 4, '-');
            renderer.draw_string(right_x + 2, 2, divider, TuiRenderer::BORDER);

            // Draw processes
            for (int i = 0; i < max_proc_rows; ++i) {
                int idx = proc_scroll_offset + i;
                if (idx >= proc_count) break;

                auto& proc = sorted_procs[idx];
                int row_y = 3 + i;

                bool is_selected = (idx == selected_proc_idx);
                Color row_bg = is_selected ? TuiRenderer::BORDER : TuiRenderer::BG_DARK;

                std::string pid_str = std::to_string(proc.pid);
                std::string cpu_str;
                {
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(1) << proc.cpu_percent;
                    cpu_str = ss.str();
                }
                std::string mem_str;
                {
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(1) << proc.mem_percent;
                    mem_str = ss.str();
                }

                // Truncate process name if too long
                std::string name_str = proc.name;
                if ((int)name_str.length() > name_col_w) {
                    name_str = name_str.substr(0, name_col_w - 3) + "...";
                }

                // Draw cells
                if (is_selected) {
                    // Fill whole row background
                    for (int x_offset = 2; x_offset < right_w - 2; ++x_offset) {
                        renderer.draw_cell(right_x + x_offset, row_y, " ", TuiRenderer::TEXT_WHITE, true, row_bg, true);
                    }
                    // Write values over it
                    std::stringstream proc_row;
                    proc_row << std::left
                             << std::setw(pid_col_w) << pid_str
                             << std::setw(cpu_col_w) << cpu_str
                             << std::setw(mem_col_w) << mem_str
                             << name_str;
                    renderer.draw_string(right_x + 2, row_y, proc_row.str(), TuiRenderer::TEXT_WHITE, true, row_bg, true);
                } else {
                    // Regular colors
                    renderer.draw_string(right_x + 2, row_y, pid_str, TuiRenderer::TEXT_WHITE);
                    
                    // CPU Color gradient based on load
                    Color cpu_c = TuiRenderer::interpolate(TuiRenderer::GREEN, TuiRenderer::RED, proc.cpu_percent / 20.0);
                    renderer.draw_string(right_x + 2 + pid_col_w, row_y, cpu_str, cpu_c);
                    
                    // MEM Color
                    renderer.draw_string(right_x + 2 + pid_col_w + cpu_col_w, row_y, mem_str, TuiRenderer::CYAN);
                    
                    // Name
                    renderer.draw_string(right_x + 2 + pid_col_w + cpu_col_w + mem_col_w, row_y, name_str, TuiRenderer::TEXT_WHITE);
                }
            }

            // Draw control keys at bottom
            std::string keys_txt = "[c]Cpu [m]Mem [p]Pid [n]Name [k]Kill [q]Quit";
            // Center the instructions
            int space_available = right_w - 4;
            int start_inst_x = right_x + 2;
            if (space_available > (int)keys_txt.length()) {
                start_inst_x += (space_available - keys_txt.length()) / 2;
            }
            renderer.draw_string(start_inst_x, height - 2, keys_txt, TuiRenderer::TEXT_GRAY);

            // Execute buffer flush
            renderer.render();
            force_redraw = false;
        }

        // Non-blocking keyboard check using poll()
        struct pollfd fds[1];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;

        int poll_ret = poll(fds, 1, 100); // Wait up to 100ms for input
        if (poll_ret > 0) {
            int key = read_key();
            if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                running = false;
            } else if (key == 'c' || key == 'C') {
                sort_mode = SortMode::CPU;
                selected_proc_idx = 0;
                force_redraw = true;
            } else if (key == 'm' || key == 'M') {
                sort_mode = SortMode::MEM;
                selected_proc_idx = 0;
                force_redraw = true;
            } else if (key == 'p' || key == 'P') {
                sort_mode = SortMode::PID;
                selected_proc_idx = 0;
                force_redraw = true;
            } else if (key == 'n' || key == 'N') {
                sort_mode = SortMode::NAME;
                selected_proc_idx = 0;
                force_redraw = true;
            } else if (key == KEY_UP) {
                if (selected_proc_idx > 0) {
                    selected_proc_idx--;
                    force_redraw = true;
                }
            } else if (key == KEY_DOWN) {
                int limit = sys_info.processes.size();
                if (selected_proc_idx < limit - 1) {
                    selected_proc_idx++;
                    force_redraw = true;
                }
            } else if (key == 'k' || key == 'K') {
                // Kill process logic
                std::vector<ProcessInfo> sorted_procs = sys_info.processes;
                std::sort(sorted_procs.begin(), sorted_procs.end(), [sort_mode](const ProcessInfo& a, const ProcessInfo& b) {
                    switch (sort_mode) {
                        case SortMode::CPU:
                            if (a.cpu_percent != b.cpu_percent) return a.cpu_percent > b.cpu_percent;
                            return a.mem_percent > b.mem_percent;
                        case SortMode::MEM:
                            if (a.mem_percent != b.mem_percent) return a.mem_percent > b.mem_percent;
                            return a.cpu_percent > b.cpu_percent;
                        case SortMode::PID: return a.pid < b.pid;
                        case SortMode::NAME: return a.name < b.name;
                    }
                    return false;
                });

                if (selected_proc_idx >= 0 && selected_proc_idx < (int)sorted_procs.size()) {
                    int pid_to_kill = sorted_procs[selected_proc_idx].pid;
                    // Don't let users easily kill critical system PIDs like 1 or the shell itself, or our current process.
                    if (pid_to_kill > 1 && pid_to_kill != getpid()) {
                        kill(pid_to_kill, SIGTERM);
                        // Trigger immediate update so it disappears quickly
                        stats_reader.update(sys_info);
                        force_redraw = true;
                    }
                }
            }
        }
    }

    disable_raw_mode();
    return 0;
}
