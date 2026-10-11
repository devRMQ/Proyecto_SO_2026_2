<div align="center">

# Simulador educativo de Sistema Operativo

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?logo=python&logoColor=white)
![PySide6](https://img.shields.io/badge/GUI-PySide6-41CD52?logo=qt&logoColor=white)
![Pruebas](https://img.shields.io/badge/pruebas-passing-brightgreen)
![Estado](https://img.shields.io/badge/estado-en%20desarrollo-yellow)

**Visualiza, tick a tick, cómo un sistema operativo planifica procesos, administra memoria y sincroniza hilos.**

</div>

---

## 📑 Tabla de contenidos

- [Acerca del proyecto](#-acerca-del-proyecto)
- [Características](#-características)
- [Estructura del repositorio](#-estructura-del-repositorio)
- [Tecnologías](#️-tecnologías)
- [Limitaciones conocidas](#️-limitaciones-conocidas)
- [Hoja de ruta](#️-hoja-de-ruta)

---

## 📖 Acerca del proyecto

**SO-Sim** es el proyecto integrador del curso **SW407 Sistemas Operativos** (UNI-FIIS), ciclo 2026-2. Cubre los **módulos 1, 2 y 3** del curso: procesos y planificación, sincronización y gestión de memoria.

La mayoría de simuladores didácticos muestran cada tema por separado. SO-Sim, además de ofrecer tres laboratorios independientes, incluye una **simulación integrada de un SO** en la que los módulos interactúan sobre los mismos procesos (PCB):

- El **planificador** decide quién usa la CPU.
- La **memoria** afecta a los procesos: un fallo de página bloquea al proceso durante unos ticks, y un proceso sin hueco en memoria contigua espera en la cola de trabajos.
- La **sincronización** vive dentro de los procesos: productores y consumidores se bloquean y despiertan entre sí con semáforos, y el motor detecta el **interbloqueo**.

El **núcleo está escrito en C++17** (`sosim`) y se ejecuta como un proceso independiente. La **interfaz en Python (PySide6)** lo lanza, lee el JSON de salida y anima el resultado. Esta separación desacopla los módulos y permite repartir el trabajo entre los integrantes.

### 👥 Autores

| Nombre | GitHub |
|---|---|
| _Morales Quispe Rodrigo_ | [@devRMQ](https://github.com/devRMQ) |
| _Luna Roca Fabrizio Santiago_ | [@fabriziolunaroca](https://github.com/usuario) |
| _Romani Cuyo Gabriel Alberto_ | [@gabo-13](https://github.com/gabo-13) |

---

## 🧩 Características

**Planificación de CPU**
- Algoritmos **FCFS**, **SJF**, **Round Robin** y **Prioridad**.
- SJF y Prioridad con opción **apropiativa**.
- Métricas de espera, respuesta y retorno por proceso, y diagrama de **Gantt**.

**Gestión de memoria**
- **Paginada** con reemplazo **FIFO**, **LRU** y **Óptimo**; los fallos de página bloquean al proceso y permiten observar *thrashing*.
- **Contigua** con **First Fit**, **Best Fit** y **Worst Fit**; procesos en cola de trabajos o rechazados si piden más que la memoria total.

**Sincronización**
- Problema **productor-consumidor** con semáforos `mutex`, `empty` y `full`.
- Bloqueo y despertar con entrega directa, sin condiciones de carrera.
- **Detección automática de interbloqueo**.

**Interfaz y análisis**
- Animación de CPU, colas, memoria, semáforos, PCB, Gantt y registro del kernel.
- Pestaña de **análisis comparativo** con gráficos, conclusiones automáticas y exportación de reporte `.md`.
- Tres laboratorios independientes (planificación, sincronización y memoria).

---

### Bucle del SO integrado

En cada tick, en este orden:

1. **Llegadas**: el proceso se admite (paginado → `READY`; contiguo → busca hueco o pasa a la cola de trabajos).
2. **Cola de trabajos**: se reintenta asignar memoria a los que esperan.
3. **Fallos de página**: se descuenta el temporizador de los bloqueados; al llegar a 0 el proceso vuelve a `READY`.
4. **Expropiación** (SJF/Prioridad apropiativos).
5. **Despacho y ejecución** del proceso elegido (referencia de página, `wait`/`signal` de semáforos, avance de ráfaga).
6. **Fin de quantum** (Round Robin).
7. **Detección de interbloqueo**.

Cada tick se guarda como un *frame* en el JSON y la GUI solo los reproduce. El detalle completo está en [`ARQUITECTURA.md`](docs/ARQUITECTURA.md).

---

## 📂 Estructura del repositorio

```
Proyecto_SO_2026_2/
├── README.md
├── run.sh                     # Compila y abre la GUI en un solo paso
│
├── backend/                   # Núcleo C++ (sosim)
│   ├── CMakeLists.txt
│   ├── Makefile
│   ├── include/nlohmann/      #   Librería JSON
│   └── src/                   #   Módulos: os_sim, scheduler, sync_sim, memory_sim…
│
├── frontend/                  # Interfaz gráfica (PySide6)
│   ├── main.py                #   Punto de entrada
│   ├── engine.py              #   Puente con el núcleo C++
│   ├── requirements.txt
│   └── ui/                    #   Pestañas, Gantt y gráficos
│
└── docs/
    ├── ARQUITECTURA.md
    ├── MANUAL.md
    ├── demos/                 #   Escenarios JSON de ejemplo
    └── tests/                 #   test_kernel.py
```

---

## 🛠️ Tecnologías

| Capa | Tecnologías |
|---|---|
| Núcleo | C++17, Make / CMake, [nlohmann/json](https://github.com/nlohmann/json), `std::thread` |
| Interfaz | Python 3.10+, PySide6, matplotlib |
| Pruebas | Python (`test_kernel.py`) sobre el binario compilado |

---

## ⚠️ Limitaciones conocidas

- **Un solo núcleo de CPU.** Los hilos y `parent_pid` se muestran en el PCB, pero no se planifican por separado.
- **Óptimo aproximado en el SO integrado.** El futuro global depende del planificador, por lo que usa el futuro de cada proceso. Es exacto con un solo proceso y en el Laboratorio 3.
- **Referencias de memoria.** Si el escenario no las trae, se generan de forma determinista con localidad (misma semilla, mismos resultados).

---

## 🗺️ Hoja de ruta

- [x] Módulo 1: procesos y planificación
- [x] Módulo 2: sincronización
- [x] Módulo 3: gestión de memoria
- [x] Simulación integrada del SO con interfaz animada
- [x] Análisis comparativo con exportación de reporte
- [ ] Módulo 4: entrada/salida y archivos
- [ ] Módulo 5: seguridad
- [ ] Módulo 6: sistemas distribuidos


## Pendiente (módulos 4-6)

Entrada/Salida y archivos, seguridad y sistemas distribuidos están como **esqueletos** registrados en `registry.cpp` (devuelven `pending`). Se implementan reemplazando `run()` en su `*_module.cpp`; no hay que tocar `main.cpp`.
