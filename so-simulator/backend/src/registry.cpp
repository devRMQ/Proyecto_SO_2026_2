#include "module.hpp"

#include <algorithm>

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry r;
    return r;
}

void ModuleRegistry::add(std::unique_ptr<SimModule> m) {
    modules_.push_back(std::move(m));
}

SimModule* ModuleRegistry::find(const std::string& id) const {
    for (const auto& m : modules_) {
        if (m->id() == id) return m.get();
    }
    return nullptr;
}

nlohmann::json ModuleRegistry::catalog() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : modules_) {
        arr.push_back({
            {"id", m->id()},
            {"number", m->number()},
            {"title", m->title()},
            {"implemented", m->implemented()},
        });
    }
    return arr;
}

nlohmann::json pending_result(const std::string& id, int number, const std::string& title,
                              const std::string& syllabus) {
    return {
        {"module", id},
        {"number", number},
        {"title", title},
        {"status", "pending"},
        {"implemented", false},
        {"syllabus", syllabus},
        {"hint", "Reemplaza el cuerpo de run() en el .cpp de este módulo. No hace falta cambiar main.cpp."},
    };
}

void register_os_module();
void register_schedule_module();
void register_sync_module();
void register_memory_module();
void register_io_module();
void register_security_module();
void register_distributed_module();

void register_all_modules() {
    static bool done = false;
    if (done) return;
    register_os_module();
    register_schedule_module();
    register_sync_module();
    register_memory_module();
    register_io_module();
    register_security_module();
    register_distributed_module();
    done = true;
}
