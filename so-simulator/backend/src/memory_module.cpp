#include "json_io.hpp"
#include "module.hpp"
#include "memory_sim.hpp"

namespace {

class MemoryModule final : public SimModule {
public:
    std::string id() const override { return "memory"; }
    int number() const override { return 3; }
    std::string title() const override { return "Gestión de memoria"; }

    nlohmann::json run(const nlohmann::json& section) const override {
        ContiguousConfig contig;
        PagingConfig paging;
        parse_memory_section(section, contig, paging);
        return run_memory(contig, paging);
    }
};

}  // namespace

void register_memory_module() {
    ModuleRegistry::instance().add(std::make_unique<MemoryModule>());
}
