#include "module.hpp"

// Módulo 5 — Seguridad y protección (semanas 11–12).
// Implementar aquí: autenticación simulada, control de acceso,
// cifrado simétrico/asimétrico básico y nota de virtualización.
//
// Entrada JSON esperada:
//   "security": { "users": [...], "acl": [...], "cipher": "xor|aes-demo" }

namespace {

class SecurityModule final : public SimModule {
public:
    std::string id() const override { return "security"; }
    int number() const override { return 5; }
    std::string title() const override { return "Seguridad y protección"; }
    bool implemented() const override { return false; }

    nlohmann::json run(const nlohmann::json&) const override {
        return pending_result(id(), number(), title(),
            "Autenticación, ACL/control de acceso, cifrado básico; "
            "comparación conceptual VM vs contenedores.");
    }
};

}  // namespace

void register_security_module() {
    ModuleRegistry::instance().add(std::make_unique<SecurityModule>());
}
