#include "json_io.hpp"
#include "module.hpp"
#include "scheduler.hpp"

namespace {

class ScheduleModule final : public SimModule {
public:
    std::string id() const override { return "schedule"; }
    int number() const override { return 1; }
    std::string title() const override { return "Procesos, hilos y planificación"; }

    nlohmann::json run(const nlohmann::json& section) const override {
        ScheduleConfig cfg = parse_schedule_section(section);
        if (cfg.compare_all) return compare_schedulers(cfg);
        return run_schedule(cfg);
    }
};

}  // namespace

void register_schedule_module() {
    ModuleRegistry::instance().add(std::make_unique<ScheduleModule>());
}
