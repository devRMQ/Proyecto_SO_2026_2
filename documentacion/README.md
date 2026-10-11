# SO-Sim — Simulador educativo de Sistema Operativo

Proyecto integrador de **SW407 Sistemas Operativos** (UNI-FIIS). Cubre los **módulos 1, 2 y 3** (procesos y planificación, sincronización y gestión de memoria), alineados con lo visto hasta la **semana 7**.

El **núcleo va en C++** (`sosim`) y se ejecuta como proceso. **Python** lo lanza con `subprocess`, lee el JSON de salida y lo muestra/anima en la GUI. Así los módulos quedan desacoplados y el trabajo se reparte entre integrantes.

## Qué hay de nuevo (v2)

Además de los tres laboratorios independientes, ahora hay una **simulación integrada de un SO**, tick a tick, donde los módulos interactúan sobre los mismos procesos (PCB):

- El **planificador** (FCFS, SJF, RR, Prioridad; SJF y Prioridad con opción apropiativa) decide quién usa la CPU.
- La **memoria** afecta a los procesos: en modo *paginado*, un fallo de página **bloquea** al proceso durante unos ticks; en modo *contiguo*, un proceso sin hueco **espera en la cola de trabajos** (o es rechazado si pide más que la memoria total).
- La **sincronización** vive dentro de los procesos: los de rol *productor/consumidor* usan semáforos `mutex`, `empty` y `full`, se **bloquean** y se **despiertan** entre sí; el motor detecta **interbloqueo**.
- La GUI **anima** CPU, colas, mapa de memoria, semáforos, PCB, Gantt y el registro del kernel (play/pausa/paso a paso/velocidad).
- Una pestaña de **análisis comparativo** ejecuta el mismo escenario con todos los algoritmos, grafica y exporta un reporte `.md`.

## Organización de carpetas

```
SO-Sim/
├── so-simulator/          # SOLO CÓDIGO
│   ├── backend/           #   núcleo C++ (os_sim, scheduler, sync_sim, memory_sim, registro de módulos)
│   ├── frontend/          #   GUI PySide6 (ui/os_tab.py = simulación animada) + puente subprocess
│   └── run.sh             #   compila y abre la GUI en un paso
└── documentacion/         # TODO LO QUE NO ES CÓDIGO
    ├── README.md, ARQUITECTURA.md, MANUAL.md
    ├── ejemplos/          #   escenarios JSON (demo_os, demo_os_contiguo, demo_all)
    └── pruebas/           #   test_kernel.py (pruebas contra el binario)
```

`shared_data/` (el `input.json`/`output.json` que intercambian la GUI y el núcleo) se crea sola dentro de `so-simulator/` la primera vez que ejecutas la GUI.

## Requisitos

- Un compilador C++17 (Clang/GCC) y `make` (o CMake ≥ 3.15)
- Python 3.10+
- `pip install -r so-simulator/frontend/requirements.txt` (PySide6 y matplotlib)

## Cómo correrlo (desde la carpeta `SO-Sim/`)

```bash
make -C so-simulator/backend                                   # compila el núcleo
python3 -m pip install -r so-simulator/frontend/requirements.txt
python3 so-simulator/frontend/main.py                          # abre la GUI
```

O todo en un paso: `./so-simulator/run.sh` (si da *Permission denied*: `chmod +x so-simulator/run.sh`).

> El `Makefile` compila todo `backend/src/*.cpp`, pero **no detecta cambios en los `.hpp`**. Si editas un header: `make -C so-simulator/backend clean && make -C so-simulator/backend`.

### Solo el motor, por terminal

```bash
./so-simulator/backend/build/sosim -i documentacion/ejemplos/demo_os.json
./so-simulator/backend/build/sosim -i documentacion/ejemplos/demo_all.json -m schedule   # schedule|sync|memory|os|all
```

## Pruebas

```bash
python3 documentacion/pruebas/test_kernel.py
```

Verifica los casos clásicos (FCFS espera media 17; FIFO/LRU/Óptimo = 15/12/9 fallos), el rechazo y la espera de memoria, que el productor-consumidor no tenga overflow/underflow, la detección de interbloqueo y que la RAM nunca duplique páginas.

## Pendiente (módulos 4-6)

Entrada/Salida y archivos, seguridad y sistemas distribuidos están como **esqueletos** registrados en `registry.cpp` (devuelven `pending`). Se implementan reemplazando `run()` en su `*_module.cpp`; no hay que tocar `main.cpp`.
