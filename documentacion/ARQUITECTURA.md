# Arquitectura de SO-Sim

## Visión general

```
┌──────────────────────────┐   input.json    ┌───────────────────────────┐
│  GUI Python (PySide6)    │ ──────────────▶ │  núcleo C++  (sosim)      │
│  ui/os_tab.py  (anima)   │   subprocess    │  main → json_io → registro│
│  ui/analysis_tab.py      │ ◀────────────── │  → módulo.run(sección)    │
│  engine.py (puente)      │   output.json   └───────────────────────────┘
└──────────────────────────┘
```

La GUI **no calcula nada**: arma el escenario, invoca `sosim -i input.json -o output.json -m <módulo>` y dibuja lo que devuelve el núcleo.

## Núcleo C++ (`backend/src`)

| Archivo | Rol |
|---|---|
| `main.cpp` | Argumentos (`-i`, `-o`, `-m`) y escritura del JSON. |
| `json_io.cpp` | Lectura de escenarios; `run_from_input` despacha al módulo pedido (o a todos con `all`). |
| `module.hpp`, `registry.cpp` | Interfaz `SimModule` y registro de módulos (id, número, `run(sección)`). |
| `os_sim.cpp` + `os_module.cpp` | **SO integrado** (módulo `os`): bucle tick a tick con PCB, planificador, memoria y semáforos. |
| `scheduler.cpp` | Laboratorio 1: planificación aislada y comparación de algoritmos. |
| `sync_sim.cpp` | Laboratorio 2: productor-consumidor (discreto o con `std::thread`). |
| `memory_sim.cpp` | Laboratorio 3: contigua (First/Best/Worst) y paginación (FIFO/LRU/Óptimo). |
| `io_/security_/distributed_module.cpp` | Esqueletos de los módulos 4-6. |

## Bucle del SO integrado (`os_sim.cpp`)

En cada tick, en este orden:

1. **Llegadas**: el proceso se admite. Paginado → READY. Contiguo → busca hueco; si no hay, pasa a la cola de trabajos (o se rechaza si pide más que la memoria total).
2. **Cola de trabajos**: se reintenta asignar memoria a los que esperan.
3. **Fallos de página**: se descuenta el temporizador de cada proceso bloqueado; al llegar a 0 la página queda cargada y el proceso vuelve a READY.
4. **Expropiación** (SJF/Prioridad apropiativos).
5. **Despacho y ejecución** del proceso elegido:
   - *Paginado*: referencia una página. Si no está en RAM → **fallo**: se elige víctima (FIFO/LRU/Óptimo), el proceso se bloquea `page_fault_penalty` ticks y el tick se gasta en el trap.
   - *Productor/consumidor*: `wait(empty|full)`; si vale 0 se **bloquea** en el semáforo sin gastar CPU y se despacha otro en el mismo tick. Si no, entra a la sección crítica (`mutex`), produce/consume y hace `signal` al semáforo contrario, despertando al primero de la cola (entrega directa, sin carreras).
   - Avanza 1 tick de ráfaga; si termina, libera su memoria.
6. **Fin de quantum** (RR).
7. **Detección de interbloqueo**: nadie corre, nadie está listo, no hay llegadas ni fallos en curso, pero hay procesos bloqueados.

Cada tick se guarda como un *frame* en el JSON (cola de listos, bloqueados, estados, mapa de memoria, semáforos, eventos), y la GUI solo los reproduce.

## Contrato JSON (módulo `os`)

Entrada (`"module": "os"`):

```json
{"module": "os", "os": {
  "scheduler": {"algorithm": "RR", "quantum": 3, "preemptive": false},
  "memory": {"mode": "paged", "frames": 6, "replacement": "LRU", "page_fault_penalty": 3,
             "total_memory": 48, "strategy": "FIRST_FIT"},
  "sync": {"buffer_size": 2},
  "compare": true,
  "processes": [{"pid": 1, "name": "Init", "arrival": 0, "burst": 8, "priority": 3, "threads": 1,
                 "parent_pid": -1, "role": "normal", "mem_size": 16, "pages": 4}]
}}
```

Salida (`out["os"]`): `metrics`, `processes` (espera, respuesta, retorno, fallos, bloqueos), `gantt`, `memory`, `sync`, `deadlock`, `frames[]`, `log[]` y, con `compare`, `comparison{schedulers, replacement|allocation}`.

## Decisiones y limitaciones (para la sustentación)

- **Un solo núcleo de CPU.** Los hilos y `parent_pid` se muestran en el PCB pero no se planifican por separado.
- **Óptimo aproximado en el SO integrado.** El futuro global depende del planificador, así que el Óptimo usa el futuro de cada proceso. Con un solo proceso es exacto (9 fallos en el caso clásico); en el Laboratorio 3 es exacto.
- **Referencias de memoria.** Si el escenario no trae `refs`, se generan de forma determinista con localidad (misma semilla ⇒ mismos resultados).
- **Espera** = ticks en la cola de listos; los bloqueos por fallo de página, semáforo y memoria se reportan aparte.
