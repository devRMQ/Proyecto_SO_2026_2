#pragma once

#include "types.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct MemRequest {
    std::string op;  // ALLOC | FREE
    int pid = 0;
    int size = 0;
};

struct ContiguousConfig {
    int total_memory = 64;
    AllocStrategy strategy = AllocStrategy::FIRST_FIT;
    std::vector<MemRequest> requests;
};

struct PagingConfig {
    int frames = 3;
    std::vector<int> references;
    bool compare_all = true;
    ReplaceAlgo algorithm = ReplaceAlgo::FIFO;
};

nlohmann::json run_contiguous(const ContiguousConfig& cfg);
nlohmann::json run_paging(const PagingConfig& cfg);
nlohmann::json run_memory(const ContiguousConfig& contig, const PagingConfig& paging);
