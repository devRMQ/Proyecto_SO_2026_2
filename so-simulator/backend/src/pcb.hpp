#pragma once

#include "types.hpp"

struct PCB {
    int pid = 0;
    std::string name;
    ProcessState state = ProcessState::NEW;
    int arrival = 0;
    int burst = 0;
    int remaining = 0;
    int priority = 1;
    int threads = 1;
    int parent_pid = -1;
    int quantum_used = 0;
    int first_run = -1;
    int finish = -1;
};
