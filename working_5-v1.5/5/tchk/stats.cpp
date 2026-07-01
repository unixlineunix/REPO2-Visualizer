#include "stats.hxx"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <cstring>

static std::unordered_map<int, unsigned long long> process_ticks_cache;

StatsReader::StatsReader() {
    // Initial read to set up baselines
    SystemInfo tmp;
    update_cpu(tmp);
}

void StatsReader::update(SystemInfo& info) {
    update_cpu(info);
    update_mem(info);
    update_sysinfo(info);
    update_processes(info);
}

void StatsReader::update_cpu(SystemInfo& info) {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return;

    std::string line;
    std::vector<std::pair<std::string, std::vector<unsigned long long>>> cpu_lines;

    while (std::getline(file, line)) {
        if (line.compare(0, 3, "cpu") == 0) {
            std::istringstream ss(line);
            std::string name;
            ss >> name;
            
            unsigned long long val;
            std::vector<unsigned long long> vals;
            while (ss >> val) {
                vals.push_back(val);
            }
            if (!vals.empty()) {
                cpu_lines.push_back({name, vals});
            }
        }
    }

    if (cpu_lines.empty()) return;

    // Process total CPU (first line "cpu")
    auto total_cpu_data = cpu_lines[0].second;
    unsigned long long active = 0;
    unsigned long long total = 0;

    // user + nice + system + irq + softirq + steal
    if (total_cpu_data.size() >= 8) {
        active = total_cpu_data[0] + total_cpu_data[1] + total_cpu_data[2] +
                 total_cpu_data[5] + total_cpu_data[6] + total_cpu_data[7];
        // idle + iowait
        unsigned long long idle = total_cpu_data[3] + total_cpu_data[4];
        total = active + idle;
    }

    if (prev_cpu_total > 0) {
        unsigned long long delta_active = active - prev_cpu_active;
        unsigned long long delta_total = total - prev_cpu_total;
        if (delta_total > 0) {
            info.total_cpu_usage = 100.0 * delta_active / delta_total;
        }
    }
    prev_cpu_active = active;
    prev_cpu_total = total;
    prev_system_ticks = total; // This represents total system ticks for process calculation

    // Process per-core CPUs
    size_t num_cores = cpu_lines.size() - 1;
    if (info.cores.size() != num_cores) {
        info.cores.resize(num_cores);
    }

    for (size_t i = 0; i < num_cores; ++i) {
        auto& core_data = cpu_lines[i + 1];
        auto& core = info.cores[i];
        core.name = core_data.first;

        unsigned long long c_active = 0;
        unsigned long long c_total = 0;
        if (core_data.second.size() >= 8) {
            c_active = core_data.second[0] + core_data.second[1] + core_data.second[2] +
                       core_data.second[5] + core_data.second[6] + core_data.second[7];
            unsigned long long c_idle = core_data.second[3] + core_data.second[4];
            c_total = c_active + c_idle;
        }

        if (core.prev_total > 0) {
            unsigned long long delta_active = c_active - core.prev_active;
            unsigned long long delta_total = c_total - core.prev_total;
            if (delta_total > 0) {
                core.usage_percent = 100.0 * delta_active / delta_total;
            }
        }
        core.prev_active = c_active;
        core.prev_total = c_total;
    }
}

void StatsReader::update_mem(SystemInfo& info) {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return;

    std::string key;
    unsigned long long val;
    std::string unit;

    while (file >> key >> val >> unit) {
        if (key == "MemTotal:") {
            info.mem_total = val * 1024; // convert to bytes
        } else if (key == "MemFree:") {
            info.mem_free = val * 1024;
        } else if (key == "MemAvailable:") {
            info.mem_available = val * 1024;
        } else if (key == "SwapTotal:") {
            info.swap_total = val * 1024;
        } else if (key == "SwapFree:") {
            info.swap_free = val * 1024;
        }
    }

    if (info.mem_available == 0) {
        info.mem_available = info.mem_free; // fallback
    }

    info.mem_used = info.mem_total - info.mem_available;
    if (info.mem_total > 0) {
        info.mem_percent = 100.0 * info.mem_used / info.mem_total;
    }

    info.swap_used = info.swap_total - info.swap_free;
    if (info.swap_total > 0) {
        info.swap_percent = 100.0 * info.swap_used / info.swap_total;
    }
}

void StatsReader::update_sysinfo(SystemInfo& info) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.load_avg[0] = si.loads[0] / 65536.0;
        info.load_avg[1] = si.loads[1] / 65536.0;
        info.load_avg[2] = si.loads[2] / 65536.0;
        
        long uptime_secs = si.uptime;
        long days = uptime_secs / 86400;
        long hours = (uptime_secs % 86400) / 3600;
        long mins = (uptime_secs % 3600) / 60;
        long secs = uptime_secs % 60;

        std::ostringstream oss;
        if (days > 0) oss << days << "d ";
        if (hours > 0 || days > 0) oss << hours << "h ";
        oss << mins << "m " << secs << "s";
        info.uptime_str = oss.str();
    }
}

void StatsReader::update_processes(SystemInfo& info) {
    DIR* dir = opendir("/proc");
    if (!dir) return;

    std::vector<ProcessInfo> new_list;
    struct dirent* entry;
    long page_size = sysconf(_SC_PAGESIZE);
    int num_cores = info.cores.size();
    if (num_cores <= 0) num_cores = 1;

    while ((entry = readdir(dir))) {
        // Only look at folders that are numbers (PIDs)
        if (entry->d_type != DT_DIR) continue;
        char* end;
        int pid = strtol(entry->d_name, &end, 10);
        if (*end != '\0') continue; // Not a number

        std::string stat_path = "/proc/" + std::string(entry->d_name) + "/stat";
        std::ifstream stat_file(stat_path);
        if (!stat_file.is_open()) continue;

        std::string content;
        std::getline(stat_file, content);
        stat_file.close();

        // Robust parsing of name between '(' and ')'
        size_t first_paren = content.find('(');
        size_t last_paren = content.rfind(')');
        if (first_paren == std::string::npos || last_paren == std::string::npos || last_paren <= first_paren) {
            continue;
        }

        ProcessInfo pinfo;
        pinfo.pid = pid;
        pinfo.name = content.substr(first_paren + 1, last_paren - first_paren - 1);

        // Get everything after the last ')'
        std::string after_paren = content.substr(last_paren + 2);
        std::istringstream ss(after_paren);

        std::string word;
        std::vector<std::string> fields;
        while (ss >> word) {
            fields.push_back(word);
        }

        // Field indices relative to fields vector:
        // fields[0] -> state (char)
        // fields[11] -> utime (long)
        // fields[12] -> stime (long)
        // fields[21] -> rss (long in pages)
        if (fields.size() >= 22) {
            pinfo.state = fields[0];
            unsigned long long utime = std::stoull(fields[11]);
            unsigned long long stime = std::stoull(fields[12]);
            unsigned long long ticks = utime + stime;
            long long rss_pages = std::stoll(fields[21]);
            pinfo.rss_bytes = rss_pages * page_size;

            // Mem percent
            if (info.mem_total > 0) {
                pinfo.mem_percent = 100.0 * pinfo.rss_bytes / info.mem_total;
            }

            // CPU percent calculation based on system ticks diff
            auto it = process_ticks_cache.find(pid);
            if (it != process_ticks_cache.end()) {
                unsigned long long prev_ticks = it->second;
                long long delta_ticks = ticks - prev_ticks;
                
                // Estimate CPU percent since last update
                // System tick count difference is prev_system_ticks
                long long delta_system = prev_system_ticks - last_system_ticks;
                
                if (delta_system > 0 && delta_ticks >= 0) {
                    // Multiply by num_cores because 100% means 1 core fully loaded.
                    // btop reports processes relative to total CPU capacity (or per-core, depending on setting).
                    // We'll report it as per-core percentage (i.e. up to 100% for single core, or overall. Let's do overall max 100% to match total CPU).
                    pinfo.cpu_percent = 100.0 * delta_ticks / delta_system;
                }
            }
            process_ticks_cache[pid] = ticks;
            new_list.push_back(pinfo);
        }
    }
    closedir(dir);

    // Save current total system ticks for the next process calculation
    last_system_ticks = prev_system_ticks;

    // Filter out processes that have exited from the cache
    std::vector<int> current_pids;
    for (auto& p : new_list) current_pids.push_back(p.pid);
    std::sort(current_pids.begin(), current_pids.end());
    for (auto it = process_ticks_cache.begin(); it != process_ticks_cache.end();) {
        if (!std::binary_search(current_pids.begin(), current_pids.end(), it->first)) {
            it = process_ticks_cache.erase(it);
        } else {
            ++it;
        }
    }

    info.processes = std::move(new_list);
}
