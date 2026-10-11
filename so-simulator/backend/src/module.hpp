#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

// Contrato de un módulo del simulador (1–6).
// Para agregar E/S, seguridad o distribuidos: implementar SimModule,
// registrarlo en register_all_modules() y (opcional) una pestaña Python.
class SimModule {
public:
    virtual ~SimModule() = default;

    virtual std::string id() const = 0;       // clave JSON: schedule, sync, memory, io, ...
    virtual int number() const = 0;           // 1..6
    virtual std::string title() const = 0;
    virtual bool implemented() const { return true; }

    // Recibe la sección JSON del módulo (o el objeto raíz si viene plano).
    virtual nlohmann::json run(const nlohmann::json& section) const = 0;
};

class ModuleRegistry {
public:
    static ModuleRegistry& instance();

    void add(std::unique_ptr<SimModule> m);
    SimModule* find(const std::string& id) const;
    const std::vector<std::unique_ptr<SimModule>>& all() const { return modules_; }
    nlohmann::json catalog() const;

private:
    std::vector<std::unique_ptr<SimModule>> modules_;
};

void register_all_modules();
nlohmann::json pending_result(const std::string& id, int number, const std::string& title,
                              const std::string& syllabus);
