#include "module.hpp"
#include "os_sim.hpp"

namespace {

// Módulo "os": simulación integrada de los módulos 1-3 sobre los mismos procesos.
class OsModule final : public SimModule {
public:
    std::string id() const override { return "os"; }
    int number() const override { return 0; }
    std::string title() const override { return "SO integrado (planificación + memoria + sincronización)"; }

    nlohmann::json run(const nlohmann::json& section) const override {
        return run_os(parse_os_section(section));
    }
};

}  // namespace

void register_os_module() {
    ModuleRegistry::instance().add(std::make_unique<OsModule>());
}
