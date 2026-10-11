#pragma once

#include "types.hpp"

#include <nlohmann/json.hpp>
#include <vector>

struct ScheduleConfig {
    SchedulerAlgo algorithm = SchedulerAlgo::FCFS;
    int quantum = 4;
    bool preemptive = false;  // aplica a PRIORITY (SJF no preemptivo; RR siempre lo es)
    bool compare_all = false;
    int max_ticks = 10000;
    std::vector<ProcessDef> processes;
};

nlohmann::json run_schedule(const ScheduleConfig& cfg);
nlohmann::json compare_schedulers(const ScheduleConfig& base);
