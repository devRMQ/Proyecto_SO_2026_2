#include "module.hpp"

// Módulo 4 — E/S y sistemas de archivos (semanas 9–10).
// Implementar aquí: sondeo, interrupciones, DMA, buffering,
// asignación contigua/enlazada/indexada y permisos básicos.
//
// Entrada JSON esperada (cuando lo implementes):
//   "io": { "mode": "polling|interrupt|dma", "devices": [...],
//           "fs": { "allocation": "contiguous|linked|indexed", ... } }

namespace {

class IoModule final : public SimModule {
public:
    std::string id() const override { return "io"; }
    int number() const override { return 4; }
    std::string title() const override { return "E/S y sistemas de archivos"; }
    bool implemented() const override { return false; }

    nlohmann::json run(const nlohmann::json&) const override {
        return pending_result(id(), number(), title(),
            "Sondeo, interrupciones, DMA, buffering; FS simplificado "
            "(asignación contigua/enlazada/indexada y permisos).");
    }
};

}  // namespace

void register_io_module() {
    ModuleRegistry::instance().add(std::make_unique<IoModule>());
}
