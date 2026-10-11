"""Análisis comparativo: ejecuta el mismo escenario con cada algoritmo y compara."""
from __future__ import annotations

from datetime import datetime
from typing import Any, Callable, Dict, List

from PySide6.QtWidgets import (
    QFileDialog,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from engine import run_kernel
from ui.charts import BarCanvas


class AnalysisTab(QWidget):
    def __init__(self, notify, payload_getter: Callable[[bool], Dict[str, Any]], parent=None):
        super().__init__(parent)
        self.notify = notify
        self.get_payload = payload_getter
        self.data: Dict[str, Any] = {}

        root = QVBoxLayout(self)
        bar = QHBoxLayout()
        run = QPushButton("Comparar algoritmos con el escenario actual")
        run.clicked.connect(self.run)
        exp = QPushButton("Exportar reporte (.md)")
        exp.clicked.connect(self.export)
        bar.addWidget(run)
        bar.addWidget(exp)
        bar.addStretch()
        root.addLayout(bar)
        hint = QLabel("Usa los procesos y la configuración de la pestaña «SO integrado». "
                      "Se ejecuta el mismo escenario con cada planificador y cada política de memoria.")
        hint.setObjectName("hint")
        root.addWidget(hint)

        grid = QGridLayout()
        self.c_wait = BarCanvas(); self.c_turn = BarCanvas(); self.c_mem = BarCanvas(); self.c_cpu = BarCanvas()
        grid.addWidget(self.c_wait, 0, 0); grid.addWidget(self.c_turn, 0, 1)
        grid.addWidget(self.c_mem, 1, 0); grid.addWidget(self.c_cpu, 1, 1)
        root.addLayout(grid, 3)
        self.text = QPlainTextEdit()
        self.text.setReadOnly(True)
        self.text.setStyleSheet("font-family: Menlo, Consolas, monospace;")
        root.addWidget(self.text, 1)

    def run(self):
        try:
            out = run_kernel(self.get_payload(True), "os")
        except Exception as exc:  # noqa: BLE001
            self.notify(str(exc), True)
            return
        self.data = out["os"]
        cmp = self.data["comparison"]
        sch: List[Dict[str, Any]] = cmp["schedulers"]
        labels = [s["algorithm"] for s in sch]
        self.c_wait.plot_grouped(labels, {"Espera": [s["avg_waiting"] for s in sch],
                                          "Respuesta": [s["avg_response"] for s in sch]},
                                 "Planificadores: espera y respuesta medias", "ticks")
        self.c_turn.plot_grouped(labels, {"Retorno": [s["avg_turnaround"] for s in sch]},
                                 "Planificadores: retorno medio", "ticks")
        self.c_cpu.plot_grouped(labels, {"Cambios de contexto": [s["context_switches"] for s in sch]},
                                "Planificadores: cambios de contexto", "cambios")
        if "replacement" in cmp:
            rep = cmp["replacement"]
            self.c_mem.plot_grouped([r["algorithm"] for r in rep], {"Fallos de página": [r["page_faults"] for r in rep]},
                                    "Reemplazo de páginas (mismo escenario)", "fallos")
        else:
            al = cmp["allocation"]
            self.c_mem.plot_grouped([r["algorithm"] for r in al],
                                    {"Fragmentación externa media": [r["avg_external_fragmentation"] for r in al]},
                                    "Estrategias de asignación contigua", "unidades")
        self.text.setPlainText(self._report_text())

    def _best(self, rows, key):
        best = min(rows, key=lambda r: r[key])
        return best["algorithm"], best[key]

    def _report_text(self) -> str:
        cmp = self.data["comparison"]
        sch = cmp["schedulers"]
        lines = ["CONCLUSIONES (calculadas automáticamente)"]
        for key, name in (("avg_waiting", "espera media"), ("avg_response", "respuesta media"),
                          ("avg_turnaround", "retorno medio")):
            a, v = self._best(sch, key)
            lines.append(f"- Menor {name}: {a} ({v:.2f} ticks)")
        if "replacement" in cmp:
            a, v = self._best(cmp["replacement"], "page_faults")
            lines.append(f"- Menos fallos de página: {a} ({v}). El Óptimo es aproximado: usa el futuro de cada "
                         f"proceso, ya que el orden global depende del planificador.")
        else:
            a, v = self._best(cmp["allocation"], "avg_external_fragmentation")
            lines.append(f"- Menor fragmentación externa media: {a} ({v:.2f} u.)")
        lines.append("")
        lines.append(f"{'Algoritmo':<10}{'Espera':>9}{'Respuesta':>11}{'Retorno':>9}{'CPU %':>8}{'Ctx':>6}{'Fallos':>8}")
        for s in sch:
            lines.append(f"{s['algorithm']:<10}{s['avg_waiting']:>9.2f}{s['avg_response']:>11.2f}"
                         f"{s['avg_turnaround']:>9.2f}{s['cpu_utilization']:>8.1f}{s['context_switches']:>6}"
                         f"{s['page_faults']:>8}")
        return "\n".join(lines)

    def export(self):
        if not self.data:
            self.notify("Primero ejecuta la comparación.", True)
            return
        path, _ = QFileDialog.getSaveFileName(self, "Guardar reporte", "reporte_so_sim.md", "Markdown (*.md)")
        if not path:
            return
        cmp = self.data["comparison"]
        md = [f"# Reporte SO-Sim — {datetime.now():%Y-%m-%d %H:%M}", "", "## Configuración", "",
              f"```json\n{__import__('json').dumps(self.data['config'], indent=2, ensure_ascii=False)}\n```", "",
              "## Planificadores", "",
              "| Algoritmo | Espera | Respuesta | Retorno | CPU % | Cambios ctx | Fallos pág. |",
              "|---|---|---|---|---|---|---|"]
        for s in cmp["schedulers"]:
            md.append(f"| {s['algorithm']} | {s['avg_waiting']:.2f} | {s['avg_response']:.2f} | "
                      f"{s['avg_turnaround']:.2f} | {s['cpu_utilization']:.1f} | {s['context_switches']} | {s['page_faults']} |")
        if "replacement" in cmp:
            md += ["", "## Reemplazo de páginas", "", "| Algoritmo | Fallos | % fallos | Ticks totales |", "|---|---|---|---|"]
            for r in cmp["replacement"]:
                md.append(f"| {r['algorithm']} | {r['page_faults']} | {r['fault_rate']:.1f} | {r['total_ticks']} |")
        else:
            md += ["", "## Asignación contigua", "", "| Estrategia | Frag. externa media | Ticks totales |", "|---|---|---|"]
            for r in cmp["allocation"]:
                md.append(f"| {r['algorithm']} | {r['avg_external_fragmentation']:.2f} | {r['total_ticks']} |")
        md += ["", "## Conclusiones", "", "```", self._report_text(), "```", ""]
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(md))
        self.notify(f"Reporte guardado en {path}")
