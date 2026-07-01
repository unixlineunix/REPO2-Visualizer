#pragma once

#include <string>
#include <vector>

struct CpuCore {
    std::string name;
    double usage_percent = 0.0;
    unsigned long long prev_active = 0;
    unsigned long long prev_total = 0;
};

struct ProcessInfo {
    int pid = 0;
    std::string name;
    std::string state;
    double cpu_percent = 0.0;
    double mem_percent = 0.0;
    unsigned long long rss_bytes = 0;
    unsigned long long prev_ticks = 0;
};

struct SystemInfo {
    double total_cpu_usage = 0.0;
    std::vector<CpuCore> cores;
    
    unsigned long long mem_total = 0;
    unsigned long long mem_free = 0;
    unsigned long long mem_available = 0;
    unsigned long long mem_used = 0;
    double mem_percent = 0.0;
    
    unsigned long long swap_total = 0;
    unsigned long long swap_free = 0;
    unsigned long long swap_used = 0;
    double swap_percent = 0.0;
    
    double load_avg[3] = {0.0, 0.0, 0.0};
    std::string uptime_str;
    
    std::vector<ProcessInfo> processes;
};

class StatsReader {
public:
    StatsReader();
    void update(SystemInfo& info);

private:
    unsigned long long prev_cpu_active = 0;
    unsigned long long prev_cpu_total = 0;
    unsigned long long prev_system_ticks = 0;
    unsigned long long last_system_ticks = 0;
    
    void update_cpu(SystemInfo& info);
    void update_mem(SystemInfo& info);
    void update_sysinfo(SystemInfo& info);
    void update_processes(SystemInfo& info);
};
