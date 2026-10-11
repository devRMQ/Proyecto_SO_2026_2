#pragma once

#include <nlohmann/json.hpp>
#include <string>

struct SyncConfig {
    int producers = 2;
    int consumers = 2;
    int buffer_size = 4;
    int items_per_producer = 5;
    int items_per_consumer = 5;
    bool realtime = false;
};

nlohmann::json run_sync(const SyncConfig& cfg);
