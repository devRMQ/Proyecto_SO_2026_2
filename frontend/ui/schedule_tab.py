from __future__ import annotations

from typing import Any, Dict, List

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from engine import run_kernel
from ui.charts import BarCanvas
from ui.gantt import GanttWidget


class ScheduleTab(QWidget):
    def __init__(self, notify, parent=None):
        super().__init__(parent)
        self.notify = notify
        self.last: Dict[str, Any] = {}
        layout = QVBoxLayout(self)

        controls = QHBoxLayout()
        self.algo = QComboBox()
        self.algo.addItems(["FCFS", "SJF", "RR", "PRIORITY"])
        self.quantum = QSpinBox()
        self.quantum.setRange(1, 50)
        self.quantum.setValue(4)
        self.preemptive = QCheckBox("PRIORITY apropiativo")
        self.compare = QCheckBox("Comparar los 4 algoritmos")
        self.compare.setChecked(True)
        run_btn = QPushButton("Simular planificación")
        run_btn.clicked.connect(self.run)
        add_btn = QPushButton("Añadir proceso")
        add_btn.clicked.connect(self.add_row)
        del_btn = QPushButton("Quitar")
        del_btn.clicked.connect(self.remove_row)
        for w, label in [
            (self.algo, "Algoritmo"),
            (self.quantum, "Quantum"),
        ]:
            box = QVBoxLayout()
            box.addWidget(QLabel(label))
            box.addWidget(w)
            controls.addLayout(box)
        controls.addWidget(self.preemptive)
        controls.addWidget(self.compare)
        controls.addStretch()
        controls.addWidget(add_btn)
        controls.addWidget(del_btn)
        controls.addWidget(run_btn)
        layout.addLayout(controls)

        self.table = QTableWidget(0, 7)
        self.table.setHorizontalHeaderLabels(
            ["PID", "Nombre", "Llegada", "Ráfaga", "Prioridad", "Hilos", "Padre"]
        )
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)
        self._seed()

        self.gantt = GanttWidget()
        layout.addWidget(self.gantt)

        bottom = QHBoxLayout()
        self.metrics = QTableWidget(0, 7)
        self.metrics.setHorizontalHeaderLabels(
            ["PID", "Inicio", "Fin", "Espera", "Respuesta", "Retorno", "Hilos"]
        )
        self.metrics.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        bottom.addWidget(self.metrics, 2)
        self.chart = BarCanvas()
        bottom.addWidget(self.chart, 2)
        layout.addLayout(bottom)

        hint = QLabel(
            "Módulo 1: FCFS, SJF (no apropiativo), Round Robin y Prioridad. "
            "Mayor número = mayor prioridad. Los hilos y el padre se registran en el PCB (fork simulado)."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)

    def _seed(self):
        rows = [
            (1, "Init", 0, 8, 3, 1, -1),
            (2, "Shell", 1, 4, 2, 2, 1),
            (3, "Editor", 2, 9, 1, 3, 2),
            (4, "Compilador", 3, 5, 4, 2, 2),
        ]
        for r in rows:
            self.add_row(r)

    def add_row(self, values=None):
        row = self.table.rowCount()
        self.table.insertRow(row)
        n = row + 1
        data = values or (n, f"P{n}", 0, 5, 1, 1, -1)
        for col, val in enumerate(data):
            self.table.setItem(row, col, QTableWidgetItem(str(val)))

    def remove_row(self):
        r = self.table.currentRow()
        if r >= 0:
            self.table.removeRow(r)

    def _processes(self) -> List[Dict[str, Any]]:
        procs = []
        for row in range(self.table.rowCount()):
            def cell(c, default="0"):
                item = self.table.item(row, c)
                return item.text() if item else default
            procs.append({
                "pid": int(cell(0)),
                "name": cell(1, f"P{row+1}"),
                "arrival": int(cell(2)),
                "burst": int(cell(3)),
                "priority": int(cell(4)),
                "threads": int(cell(5) or 1),
                "parent_pid": int(cell(6) or -1),
            })
        return procs

    def run(self):
        payload = {
            "module": "schedule",
            "schedule": {
                "algorithm": self.algo.currentText(),
                "quantum": self.quantum.value(),
                "preemptive": self.preemptive.isChecked(),
                "compare": self.compare.isChecked(),
                "processes": self._processes(),
            },
        }
        try:
            data = run_kernel(payload, module="schedule")
        except Exception as exc:
            self.notify(str(exc), error=True)
            return
        self.last = data.get("schedule", {})
        self._render(self.last)
        self.notify("Planificación completada.")

    def _render(self, data: Dict[str, Any]):
        results = data.get("results")
        if results:
            first = results[0]
            self.gantt.set_slices(first.get("gantt", []))
            self._fill_metrics(first)
            labels = [r.get("algorithm", "?") for r in results]
            series = {
                "Espera": [r.get("avg_waiting", 0) for r in results],
                "Respuesta": [r.get("avg_response", 0) for r in results],
                "Retorno": [r.get("avg_turnaround", 0) for r in results],
            }
            self.chart.plot_grouped(labels, series, "Comparación de algoritmos", "ticks")
        else:
            self.gantt.set_slices(data.get("gantt", []))
            self._fill_metrics(data)
            labels = [p["name"] for p in data.get("processes", [])]
            series = {
                "Espera": [p.get("waiting", 0) for p in data.get("processes", [])],
                "Respuesta": [p.get("response", 0) for p in data.get("processes", [])],
                "Retorno": [p.get("turnaround", 0) for p in data.get("processes", [])],
            }
            self.chart.plot_grouped(labels, series, data.get("algorithm", "Métricas"), "ticks")

    def _fill_metrics(self, data: Dict[str, Any]):
        procs = data.get("processes", [])
        self.metrics.setRowCount(len(procs))
        for i, p in enumerate(procs):
            vals = [p.get("pid"), p.get("start"), p.get("finish"), p.get("waiting"),
                    p.get("response"), p.get("turnaround"), p.get("threads")]
            for c, v in enumerate(vals):
                self.metrics.setItem(i, c, QTableWidgetItem(str(v)))
