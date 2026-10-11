#include "module.hpp"

// Módulo 6 — Sistemas distribuidos (semanas 13–15).
// Implementar aquí: paso de mensajes o RPC simulado entre nodos.
//
// Entrada JSON esperada:
//   "distributed": { "nodes": 3, "protocol": "message|rpc", "messages": [...] }

namespace {

class DistributedModule final : public SimModule {
public:
    std::string id() const override { return "distributed"; }
    int number() const override { return 6; }
    std::string title() const override { return "Sistemas distribuidos"; }
    bool implemented() const override { return false; }

    nlohmann::json run(const nlohmann::json&) const override {
        return pending_result(id(), number(), title(),
            "Comunicación entre procesos/nodos (RPC o paso de mensajes simulado).");
    }
};

}  // namespace

void register_distributed_module() {
    ModuleRegistry::instance().add(std::make_unique<DistributedModule>());
}
