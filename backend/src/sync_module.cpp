#include "json_io.hpp"
#include "module.hpp"
#include "sync_sim.hpp"

namespace {

class SyncModule final : public SimModule {
public:
    std::string id() const override { return "sync"; }
    int number() const override { return 2; }
    std::string title() const override { return "Sincronización (productor-consumidor)"; }

    nlohmann::json run(const nlohmann::json& section) const override {
        return run_sync(parse_sync_section(section));
    }
};

}  // namespace

void register_sync_module() {
    ModuleRegistry::instance().add(std::make_unique<SyncModule>());
}
