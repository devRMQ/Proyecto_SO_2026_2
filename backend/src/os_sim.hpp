#pragma once

#include "types.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Simulación integrada de un SO (tick a tick): planificador + memoria
// (contigua o paginada) + sincronización (productor-consumidor con semáforos),
// todo sobre los mismos procesos/PCB.

enum class MemMode { CONTIGUOUS, PAGED };
enum class ProcRole { NORMAL, PRODUCER, CONSUMER };

struct OsProcess {
    int pid = 0;
    std::string name;
    int arrival = 0;
    int burst = 1;       // ticks de CPU productivos (para productor/consumidor = nº de ítems)
    int priority = 1;    // mayor número = mayor prioridad
    int threads = 1;
    int parent_pid = -1;
    int mem_size = 8;    // unidades de memoria (modo contiguo)
    int pages = 4;       // páginas del espacio virtual (modo paginado)
    ProcRole role = ProcRole::NORMAL;
    std::vector<int> refs;  // cadena de referencias opcional (cíclica)
};

struct OsConfig {
    SchedulerAlgo algorithm = SchedulerAlgo::FCFS;
    int quantum = 3;
    bool preemptive = false;  // aplica a PRIORITY y SJF (SRTF)
    MemMode mem_mode = MemMode::PAGED;
    int total_memory = 64;
    AllocStrategy strategy = AllocStrategy::FIRST_FIT;
    int frames = 6;
    ReplaceAlgo replacement = ReplaceAlgo::LRU;
    int pf_penalty = 3;       // ticks que cuesta un fallo de página
    int buffer_size = 3;      // buffer compartido productor-consumidor
    int max_ticks = 3000;
    bool compare = false;
    std::vector<OsProcess> processes;
};

OsConfig parse_os_section(const nlohmann::json& j);
nlohmann::json run_os(const OsConfig& cfg);
