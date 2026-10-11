#include "scheduler.hpp"
#include "pcb.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

PCB* pick_fcfs(std::vector<PCB*>& ready) {
    if (ready.empty()) return nullptr;
    PCB* p = ready.front();
    ready.erase(ready.begin());
    return p;
}

PCB* pick_sjf(std::vector<PCB*>& ready) {
    if (ready.empty()) return nullptr;
    auto it = std::min_element(ready.begin(), ready.end(), [](PCB* a, PCB* b) {
        if (a->burst != b->burst) return a->burst < b->burst;
        if (a->arrival != b->arrival) return a->arrival < b->arrival;
        return a->pid < b->pid;
    });
    PCB* p = *it;
    ready.erase(it);
    return p;
}

PCB* pick_priority(std::vector<PCB*>& ready) {
    if (ready.empty()) return nullptr;
    auto it = std::max_element(ready.begin(), ready.end(), [](PCB* a, PCB* b) {
        if (a->priority != b->priority) return a->priority < b->priority;
        if (a->arrival != b->arrival) return a->arrival > b->arrival;
        return a->pid > b->pid;
    });
    PCB* p = *it;
    ready.erase(it);
    return p;
}

PCB* pick_rr(std::vector<PCB*>& ready) {
    return pick_fcfs(ready);
}

PCB* select_next(SchedulerAlgo algo, std::vector<PCB*>& ready) {
    switch (algo) {
        case SchedulerAlgo::SJF: return pick_sjf(ready);
        case SchedulerAlgo::PRIORITY: return pick_priority(ready);
        case SchedulerAlgo::RR: return pick_rr(ready);
        case SchedulerAlgo::FCFS:
        default: return pick_fcfs(ready);
    }
}

bool should_preempt(const ScheduleConfig& cfg, PCB* running, const std::vector<PCB*>& ready) {
    if (!running || ready.empty()) return false;
    if (cfg.algorithm == SchedulerAlgo::RR) {
        return running->quantum_used >= cfg.quantum;
    }
    if (cfg.algorithm == SchedulerAlgo::PRIORITY && cfg.preemptive) {
        int best = std::numeric_limits<int>::min();
        for (PCB* p : ready) best = std::max(best, p->priority);
        return best > running->priority;
    }
    return false;
}

nlohmann::json metrics_json(const std::vector<ProcessMetrics>& metrics) {
    nlohmann::json arr = nlohmann::json::array();
    double sum_w = 0, sum_r = 0, sum_t = 0;
    for (const auto& m : metrics) {
        arr.push_back({
            {"pid", m.pid},
            {"name", m.name},
            {"arrival", m.arrival},
            {"burst", m.burst},
            {"priority", m.priority},
            {"threads", m.threads},
            {"parent_pid", m.parent_pid},
            {"start", m.start},
            {"finish", m.finish},
            {"waiting", m.waiting},
            {"response", m.response},
            {"turnaround", m.turnaround},
        });
        sum_w += m.waiting;
        sum_r += m.response;
        sum_t += m.turnaround;
    }
    const int n = static_cast<int>(metrics.size());
    nlohmann::json out;
    out["processes"] = arr;
    out["avg_waiting"] = n ? sum_w / n : 0.0;
    out["avg_response"] = n ? sum_r / n : 0.0;
    out["avg_turnaround"] = n ? sum_t / n : 0.0;
    return out;
}

}  // namespace

nlohmann::json run_schedule(const ScheduleConfig& cfg) {
    std::vector<std::unique_ptr<PCB>> pool;
    std::vector<PCB*> all;
    for (const auto& d : cfg.processes) {
        auto p = std::make_unique<PCB>();
        p->pid = d.pid;
        p->name = d.name.empty() ? ("P" + std::to_string(d.pid)) : d.name;
        p->arrival = d.arrival;
        p->burst = std::max(1, d.burst);
        p->remaining = p->burst;
        p->priority = d.priority;
        p->threads = std::max(1, d.threads);
        p->parent_pid = d.parent_pid;
        p->state = ProcessState::NEW;
        all.push_back(p.get());
        pool.push_back(std::move(p));
    }
    std::sort(all.begin(), all.end(), [](PCB* a, PCB* b) {
        if (a->arrival != b->arrival) return a->arrival < b->arrival;
        return a->pid < b->pid;
    });

    std::vector<PCB*> ready;
    PCB* running = nullptr;
    std::vector<GanttSlice> gantt;
    int completed = 0;
    int context_switches = 0;
    int tick = 0;
    const int n = static_cast<int>(all.size());

    auto close_slice = [&](int end) {
        if (gantt.empty()) return;
        gantt.back().end = end;
    };

    auto open_slice = [&](int start, PCB* p) {
        if (!gantt.empty() && gantt.back().end == 0) {
            gantt.back().end = start;
        }
        GanttSlice s;
        s.start = start;
        s.end = start;
        s.pid = p ? p->pid : -1;
        s.name = p ? p->name : "IDLE";
        if (!gantt.empty() && gantt.back().pid == s.pid) {
            return;  // se extenderá
        }
        gantt.push_back(s);
    };

    int last_pid = -2;
    while (completed < n && tick < cfg.max_ticks) {
        for (PCB* p : all) {
            if (p->state == ProcessState::NEW && p->arrival <= tick) {
                p->state = ProcessState::READY;
                ready.push_back(p);
            }
        }

        if (running && should_preempt(cfg, running, ready)) {
            running->state = ProcessState::READY;
            running->quantum_used = 0;
            ready.push_back(running);
            running = nullptr;
            context_switches++;
        }

        if (!running) {
            running = select_next(cfg.algorithm, ready);
            if (running) {
                running->state = ProcessState::RUNNING;
                running->quantum_used = 0;
                if (running->first_run < 0) running->first_run = tick;
                if (last_pid != running->pid && last_pid != -2 && last_pid != -1) {
                    context_switches++;
                }
            }
        }

        const int cur_pid = running ? running->pid : -1;
        if (gantt.empty() || gantt.back().pid != cur_pid) {
            open_slice(tick, running);
        }

        if (running) {
            running->remaining--;
            running->quantum_used++;
            if (running->remaining == 0) {
                running->state = ProcessState::TERMINATED;
                running->finish = tick + 1;
                completed++;
                running = nullptr;
            }
        }

        last_pid = cur_pid;
        tick++;
        if (!gantt.empty()) gantt.back().end = tick;
    }
    close_slice(tick);

    std::vector<ProcessMetrics> metrics;
    int busy = 0;
    for (const auto& sl : gantt) {
        if (sl.pid >= 0) busy += (sl.end - sl.start);
    }
    for (PCB* p : all) {
        ProcessMetrics m;
        m.pid = p->pid;
        m.name = p->name;
        m.arrival = p->arrival;
        m.burst = p->burst;
        m.priority = p->priority;
        m.threads = p->threads;
        m.parent_pid = p->parent_pid;
        m.start = p->first_run;
        m.finish = p->finish;
        m.turnaround = (p->finish >= 0) ? (p->finish - p->arrival) : 0;
        m.waiting = m.turnaround - p->burst;
        m.response = (p->first_run >= 0) ? (p->first_run - p->arrival) : 0;
        metrics.push_back(m);
    }

    nlohmann::json garr = nlohmann::json::array();
    for (const auto& sl : gantt) {
        garr.push_back({{"start", sl.start}, {"end", sl.end}, {"pid", sl.pid}, {"name", sl.name}});
    }

    nlohmann::json out = metrics_json(metrics);
    out["module"] = "schedule";
    out["algorithm"] = algoToString(cfg.algorithm);
    out["quantum"] = cfg.quantum;
    out["preemptive"] = cfg.preemptive || cfg.algorithm == SchedulerAlgo::RR;
    out["gantt"] = garr;
    out["context_switches"] = context_switches;
    out["makespan"] = tick;
    out["cpu_utilization"] = tick ? (100.0 * busy / tick) : 0.0;
    out["completed"] = completed;
    out["note"] = "Mayor número de prioridad = mayor prioridad. SJF y FCFS son no apropiativos. RR usa quantum. PRIORITY puede ser apropiativo.";
    return out;
}

nlohmann::json compare_schedulers(const ScheduleConfig& base) {
    nlohmann::json out;
    out["module"] = "schedule";
    out["mode"] = "compare";
    nlohmann::json results = nlohmann::json::array();
    const SchedulerAlgo algos[] = {
        SchedulerAlgo::FCFS, SchedulerAlgo::SJF, SchedulerAlgo::RR, SchedulerAlgo::PRIORITY
    };
    for (auto a : algos) {
        ScheduleConfig c = base;
        c.algorithm = a;
        c.compare_all = false;
        results.push_back(run_schedule(c));
    }
    out["results"] = results;
    return out;
}
