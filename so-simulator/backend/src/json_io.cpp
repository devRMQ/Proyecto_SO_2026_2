#include "json_io.hpp"

#include "module.hpp"

#include <algorithm>
#include <fstream>

namespace {

template <typename T>
T jget(const nlohmann::json& j, const std::string& key, T def) {
    if (j.is_object() && j.contains(key) && !j[key].is_null()) return j[key].get<T>();
    return def;
}

ContiguousConfig parse_contig(const nlohmann::json& j) {
    ContiguousConfig c;
    c.total_memory = jget<int>(j, "total_memory", 64);
    c.strategy = parseStrategy(jget<std::string>(j, "strategy", "FIRST_FIT"));
    if (j.contains("requests") && j["requests"].is_array()) {
        for (const auto& r : j["requests"]) {
            MemRequest m;
            m.op = jget<std::string>(r, "op", "ALLOC");
            m.pid = jget<int>(r, "pid", 0);
            m.size = jget<int>(r, "size", 0);
            c.requests.push_back(m);
        }
    }
    return c;
}

PagingConfig parse_paging(const nlohmann::json& j) {
    PagingConfig c;
    c.frames = jget<int>(j, "frames", 3);
    c.compare_all = jget<bool>(j, "compare", true);
    c.algorithm = parseReplace(jget<std::string>(j, "algorithm", "FIFO"));
    if (j.contains("references") && j["references"].is_array()) {
        for (const auto& v : j["references"]) c.references.push_back(v.get<int>());
    }
    return c;
}

}  // namespace

ScheduleConfig parse_schedule_section(const nlohmann::json& j) {
    ScheduleConfig c;
    c.algorithm = parseAlgo(jget<std::string>(j, "algorithm", "FCFS"));
    c.quantum = jget<int>(j, "quantum", 4);
    c.preemptive = jget<bool>(j, "preemptive", false);
    c.compare_all = jget<bool>(j, "compare", false);
    c.max_ticks = jget<int>(j, "max_ticks", 10000);
    if (j.contains("processes") && j["processes"].is_array()) {
        int auto_pid = 1;
        for (const auto& p : j["processes"]) {
            ProcessDef d;
            d.pid = jget<int>(p, "pid", auto_pid);
            d.name = jget<std::string>(p, "name", "P" + std::to_string(d.pid));
            d.arrival = jget<int>(p, "arrival", 0);
            d.burst = jget<int>(p, "burst", 1);
            d.priority = jget<int>(p, "priority", 1);
            d.threads = jget<int>(p, "threads", 1);
            d.parent_pid = jget<int>(p, "parent_pid", -1);
            c.processes.push_back(d);
            auto_pid = std::max(auto_pid, d.pid) + 1;
        }
    }
    return c;
}

SyncConfig parse_sync_section(const nlohmann::json& j) {
    SyncConfig c;
    c.producers = jget<int>(j, "producers", 2);
    c.consumers = jget<int>(j, "consumers", 2);
    c.buffer_size = jget<int>(j, "buffer_size", 4);
    c.items_per_producer = jget<int>(j, "items_per_producer", 5);
    c.items_per_consumer = jget<int>(j, "items_per_consumer", 5);
    c.realtime = jget<bool>(j, "realtime", false);
    return c;
}

void parse_memory_section(const nlohmann::json& j, ContiguousConfig& contig, PagingConfig& paging) {
    if (j.contains("contiguous")) contig = parse_contig(j["contiguous"]);
    if (j.contains("paging")) paging = parse_paging(j["paging"]);
}

nlohmann::json run_from_input(const nlohmann::json& root, const std::string& module_override) {
    register_all_modules();
    nlohmann::json out;
    out["ok"] = true;
    out["simulator"] = "SO-Sim";
    out["version"] = "2.0";

    std::string module = module_override.empty() ? jget<std::string>(root, "module", "all")
                                                 : module_override;
    auto section_of = [&](const std::string& id) -> nlohmann::json {
        if (root.contains(id) && root[id].is_object()) return root[id];
        // Escenario "plano": el propio objeto raíz es la sección del módulo.
        return root;
    };

    if (module == "all") {
        // Laboratorios 1-3 (siempre) + SO integrado (solo si el escenario lo trae).
        for (const char* id : {"schedule", "sync", "memory", "os"}) {
            if (std::string(id) == "os" && !root.contains("os")) continue;
            SimModule* m = ModuleRegistry::instance().find(id);
            if (m && m->implemented()) out[id] = m->run(section_of(id));
        }
        return out;
    }

    SimModule* m = ModuleRegistry::instance().find(module);
    if (!m) {
        out["ok"] = false;
        out["error"] = "Módulo desconocido: " + module;
        return out;
    }
    out[module] = m->run(section_of(module));
    return out;
}

bool load_json_file(const std::string& path, nlohmann::json& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "No se pudo abrir: " + path;
        return false;
    }
    try {
        in >> out;
    } catch (const std::exception& e) {
        err = std::string("JSON inválido: ") + e.what();
        return false;
    }
    return true;
}

bool write_json_file(const std::string& path, const nlohmann::json& data, std::string& err) {
    std::ofstream out(path);
    if (!out) {
        err = "No se pudo escribir: " + path;
        return false;
    }
    out << data.dump(2) << "\n";
    return true;
}
