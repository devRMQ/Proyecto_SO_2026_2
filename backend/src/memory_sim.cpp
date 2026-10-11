#include "memory_sim.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Block {
    int start = 0;
    int size = 0;
    int pid = -1;  // -1 = hueco libre
};

void coalesce(std::vector<Block>& mem) {
    if (mem.empty()) return;
    std::vector<Block> out;
    out.push_back(mem[0]);
    for (size_t i = 1; i < mem.size(); ++i) {
        if (out.back().pid == -1 && mem[i].pid == -1) {
            out.back().size += mem[i].size;
        } else {
            out.push_back(mem[i]);
        }
    }
    mem.swap(out);
}

int find_hole(const std::vector<Block>& mem, int size, AllocStrategy strat) {
    int best = -1;
    int best_size = (strat == AllocStrategy::WORST_FIT) ? -1 : std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(mem.size()); ++i) {
        if (mem[i].pid != -1 || mem[i].size < size) continue;
        if (strat == AllocStrategy::FIRST_FIT) return i;
        if (strat == AllocStrategy::BEST_FIT) {
            if (mem[i].size < best_size) {
                best_size = mem[i].size;
                best = i;
            }
        } else {
            if (mem[i].size > best_size) {
                best_size = mem[i].size;
                best = i;
            }
        }
    }
    return best;
}

nlohmann::json blocks_json(const std::vector<Block>& mem) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : mem) {
        arr.push_back({{"start", b.start}, {"size", b.size}, {"pid", b.pid},
                       {"free", b.pid < 0}});
    }
    return arr;
}

void compute_frag(const std::vector<Block>& mem, int& free_total, int& largest_hole, int& holes) {
    free_total = 0;
    largest_hole = 0;
    holes = 0;
    for (const auto& b : mem) {
        if (b.pid < 0) {
            holes++;
            free_total += b.size;
            largest_hole = std::max(largest_hole, b.size);
        }
    }
}

nlohmann::json simulate_replace(ReplaceAlgo algo, int frames, const std::vector<int>& refs) {
    std::vector<int> frame(frames, -1);
    std::vector<int> load_time(frames, -1);
    std::vector<int> last_used(frames, -1);
    int loaded = 0;
    int faults = 0;
    int hits = 0;
    nlohmann::json steps = nlohmann::json::array();

    auto present = [&](int page) -> int {
        for (int i = 0; i < frames; ++i)
            if (frame[i] == page) return i;
        return -1;
    };

    auto victim = [&](int t) -> int {
        if (loaded < frames) {
            for (int i = 0; i < frames; ++i)
                if (frame[i] < 0) return i;
        }
        if (algo == ReplaceAlgo::FIFO) {
            int best = 0;
            for (int i = 1; i < frames; ++i)
                if (load_time[i] < load_time[best]) best = i;
            return best;
        }
        if (algo == ReplaceAlgo::LRU) {
            int best = 0;
            for (int i = 1; i < frames; ++i)
                if (last_used[i] < last_used[best]) best = i;
            return best;
        }
        // OPTIMAL: reemplaza la página cuyo próximo uso está más lejos
        int best = 0;
        int best_next = -1;
        for (int i = 0; i < frames; ++i) {
            int next = std::numeric_limits<int>::max();
            for (int k = t + 1; k < static_cast<int>(refs.size()); ++k) {
                if (refs[k] == frame[i]) {
                    next = k;
                    break;
                }
            }
            if (next > best_next) {
                best_next = next;
                best = i;
            }
        }
        return best;
    };

    for (int t = 0; t < static_cast<int>(refs.size()); ++t) {
        const int page = refs[t];
        const int idx = present(page);
        bool fault = idx < 0;
        int replaced = -1;
        int slot = idx;
        if (fault) {
            faults++;
            slot = victim(t);
            replaced = frame[slot];
            frame[slot] = page;
            load_time[slot] = t;
            if (replaced < 0) loaded++;
        } else {
            hits++;
        }
        last_used[slot] = t;
        nlohmann::json fr = nlohmann::json::array();
        for (int f : frame) fr.push_back(f);
        steps.push_back({
            {"t", t},
            {"page", page},
            {"fault", fault},
            {"replaced", replaced},
            {"frames", fr},
        });
    }

    nlohmann::json out;
    out["algorithm"] = replaceToString(algo);
    out["frames"] = frames;
    out["references"] = refs;
    out["page_faults"] = faults;
    out["hits"] = hits;
    out["fault_rate"] = refs.empty() ? 0.0 : (100.0 * faults / refs.size());
    out["steps"] = steps;
    return out;
}

}  // namespace

nlohmann::json run_contiguous(const ContiguousConfig& cfg) {
    std::vector<Block> mem;
    mem.push_back({0, std::max(1, cfg.total_memory), -1});
    nlohmann::json snapshots = nlohmann::json::array();
    int failed = 0;

    for (const auto& req : cfg.requests) {
        std::string status = "ok";
        if (req.op == "FREE") {
            bool found = false;
            for (auto& b : mem) {
                if (b.pid == req.pid) {
                    b.pid = -1;
                    found = true;
                }
            }
            if (!found) status = "not_found";
            coalesce(mem);
        } else {
            int idx = find_hole(mem, req.size, cfg.strategy);
            if (idx < 0) {
                status = "out_of_memory";
                failed++;
            } else {
                Block& hole = mem[idx];
                if (hole.size == req.size) {
                    hole.pid = req.pid;
                } else {
                    Block alloc{hole.start, req.size, req.pid};
                    Block rest{hole.start + req.size, hole.size - req.size, -1};
                    mem[idx] = alloc;
                    mem.insert(mem.begin() + idx + 1, rest);
                }
            }
        }
        int start = 0;
        for (auto& b : mem) {
            b.start = start;
            start += b.size;
        }
        int free_total = 0, largest = 0, holes = 0;
        compute_frag(mem, free_total, largest, holes);
        snapshots.push_back({
            {"op", req.op},
            {"pid", req.pid},
            {"size", req.size},
            {"status", status},
            {"blocks", blocks_json(mem)},
            {"free_total", free_total},
            {"largest_hole", largest},
            {"holes", holes},
            {"external_fragmentation", (free_total > 0 && largest < free_total)},
            {"external_fragmentation_ratio",
             free_total ? (100.0 * (free_total - largest) / cfg.total_memory) : 0.0},
        });
    }

    int free_total = 0, largest = 0, holes = 0;
    compute_frag(mem, free_total, largest, holes);

    nlohmann::json out;
    out["mode"] = "contiguous";
    out["total_memory"] = cfg.total_memory;
    out["strategy"] = strategyToString(cfg.strategy);
    out["snapshots"] = snapshots;
    out["failed_allocs"] = failed;
    out["final_free"] = free_total;
    out["final_largest_hole"] = largest;
    out["final_holes"] = holes;
    return out;
}

nlohmann::json run_paging(const PagingConfig& cfg) {
    nlohmann::json out;
    out["mode"] = "paging";
    out["frames"] = cfg.frames;
    out["references"] = cfg.references;
    if (cfg.compare_all) {
        nlohmann::json results = nlohmann::json::array();
        results.push_back(simulate_replace(ReplaceAlgo::FIFO, cfg.frames, cfg.references));
        results.push_back(simulate_replace(ReplaceAlgo::LRU, cfg.frames, cfg.references));
        results.push_back(simulate_replace(ReplaceAlgo::OPTIMAL, cfg.frames, cfg.references));
        out["results"] = results;
        out["compare"] = true;
    } else {
        out["results"] = nlohmann::json::array({simulate_replace(cfg.algorithm, cfg.frames, cfg.references)});
        out["compare"] = false;
    }
    return out;
}

nlohmann::json run_memory(const ContiguousConfig& contig, const PagingConfig& paging) {
    nlohmann::json out;
    out["module"] = "memory";
    out["contiguous"] = run_contiguous(contig);
    out["paging"] = run_paging(paging);
    return out;
}
