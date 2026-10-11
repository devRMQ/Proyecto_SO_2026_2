# Manual de uso de SO-Sim

## 1. Pestaña «SO integrado»

1. **Configura el sistema** (arriba): planificador y quantum, tipo de memoria (paginada o contigua) y sus parámetros, y el tamaño del buffer productor-consumidor.
2. **Edita los procesos** en la tabla (cada PID debe ser único; los procesos nuevos reciben el siguiente PID libre; llegada, ráfaga, prioridad, hilos, padre, rol, memoria en unidades y páginas). Usa *Añadir proceso* / *Quitar*, o *Cargar escenario…* (`documentacion/ejemplos/*.json`).
3. Pulsa **▶ Simular**. La simulación se reproduce sola; puedes pausar, avanzar ◀ ▶, saltar con la barra y cambiar la velocidad.
4. Lee el panel:
   - **CPU**: quién ejecuta y cuánto le queda.
   - **Colas**: listos, bloqueados por fallo de página, bloqueados en semáforo y esperando memoria.
   - **Tabla**: PCB en vivo (estado, restante, fallos de página).
   - **Memoria**: marcos (con `…` si se está cargando una página) o bloques contiguos y huecos.
   - **Sincronización**: valores de `mutex`, `empty`, `full` y el buffer.
   - **Registro del kernel**: cada dispatch, fallo, wait/signal, fin de quantum, etc.
   - **Gantt** y la **línea de resultado** con las métricas finales.

Consejos para probar:

- **RR con quantum 1 vs 6** → cambia el número de cambios de contexto y la espera.
- **Marcos 3 vs 10** con memoria paginada → pasa de *thrashing* a casi sin fallos.
- **Productor con ráfaga 6 y consumidor con ráfaga 3** → el sistema termina en **interbloqueo** (el productor se queda bloqueado en `empty`) y la GUI lo avisa.
- **Memoria contigua con 24 u. totales** → los procesos grandes esperan o son rechazados.

## 2. Pestaña «Análisis comparativo»

Ejecuta el escenario actual con FCFS, SJF, RR y Prioridad, y con FIFO/LRU/Óptimo (paginado) o First/Best/Worst Fit (contiguo). Muestra gráficos, conclusiones automáticas y permite **exportar un reporte `.md`** para el informe del curso.

## 3. Laboratorios 1-3

Las tres pestañas «Lab» son las prácticas independientes (planificación, sincronización y memoria) con sus propios gráficos. Sirven para las entregas de las semanas 4 y 7.

## 4. Solución de problemas

| Síntoma | Solución |
|---|---|
| `Nothing to be done for 'all'` | No es un error: el binario ya está actualizado. |
| Cambié un `.hpp` y no cambia nada | `make -C so-simulator/backend clean && make -C so-simulator/backend`. |
| `ERROR: falta -i input.json` | Ejecuta `./so-simulator/backend/build/sosim -i documentacion/ejemplos/demo_os.json`. |
| `externally-managed-environment` al instalar | Usa un entorno virtual: `python3 -m venv .venv && source .venv/bin/activate`. |
