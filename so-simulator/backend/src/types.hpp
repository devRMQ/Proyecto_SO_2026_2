#pragma once

#include <string>
#include <vector>

enum class ProcessState { NEW, READY, RUNNING, WAITING, TERMINATED };

inline std::string stateToString(ProcessState s) {
    switch (s) {
        case ProcessState::NEW: return "NEW";
        case ProcessState::READY: return "READY";
        case ProcessState::RUNNING: return "RUNNING";
        case ProcessState::WAITING: return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

enum class SchedulerAlgo { FCFS, SJF, RR, PRIORITY };

inline SchedulerAlgo parseAlgo(const std::string& s) {
    if (s == "SJF") return SchedulerAlgo::SJF;
    if (s == "RR" || s == "ROUND_ROBIN") return SchedulerAlgo::RR;
    if (s == "PRIORITY" || s == "Priority") return SchedulerAlgo::PRIORITY;
    return SchedulerAlgo::FCFS;
}

inline std::string algoToString(SchedulerAlgo a) {
    switch (a) {
        case SchedulerAlgo::FCFS: return "FCFS";
        case SchedulerAlgo::SJF: return "SJF";
        case SchedulerAlgo::RR: return "RR";
        case SchedulerAlgo::PRIORITY: return "PRIORITY";
    }
    return "FCFS";
}

enum class AllocStrategy { FIRST_FIT, BEST_FIT, WORST_FIT };

inline AllocStrategy parseStrategy(const std::string& s) {
    if (s == "BEST_FIT") return AllocStrategy::BEST_FIT;
    if (s == "WORST_FIT") return AllocStrategy::WORST_FIT;
    return AllocStrategy::FIRST_FIT;
}

inline std::string strategyToString(AllocStrategy s) {
    switch (s) {
        case AllocStrategy::FIRST_FIT: return "FIRST_FIT";
        case AllocStrategy::BEST_FIT: return "BEST_FIT";
        case AllocStrategy::WORST_FIT: return "WORST_FIT";
    }
    return "FIRST_FIT";
}

enum class ReplaceAlgo { FIFO, LRU, OPTIMAL };

inline ReplaceAlgo parseReplace(const std::string& s) {
    if (s == "LRU") return ReplaceAlgo::LRU;
    if (s == "OPTIMAL" || s == "OPTIMO" || s == "OPT") return ReplaceAlgo::OPTIMAL;
    return ReplaceAlgo::FIFO;
}

inline std::string replaceToString(ReplaceAlgo a) {
    switch (a) {
        case ReplaceAlgo::FIFO: return "FIFO";
        case ReplaceAlgo::LRU: return "LRU";
        case ReplaceAlgo::OPTIMAL: return "OPTIMAL";
    }
    return "FIFO";
}

struct ProcessDef {
    int pid = 0;
    std::string name;
    int arrival = 0;
    int burst = 1;
    int priority = 1;  // mayor número = mayor prioridad
    int threads = 1;
    int parent_pid = -1;
};

struct GanttSlice {
    int start = 0;
    int end = 0;
    int pid = -1;  // -1 = idle
    std::string name;
};

struct ProcessMetrics {
    int pid = 0;
    std::string name;
    int arrival = 0;
    int burst = 0;
    int priority = 0;
    int threads = 1;
    int parent_pid = -1;
    int start = -1;
    int finish = -1;
    int waiting = 0;
    int response = 0;
    int turnaround = 0;
};
