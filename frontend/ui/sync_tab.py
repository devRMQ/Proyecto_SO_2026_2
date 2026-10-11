from __future__ import annotations

from typing import Any, Dict

from PySide6.QtWidgets import (
    QCheckBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from engine import run_kernel
from ui.charts import BarCanvas


class SyncTab(QWidget):
    def __init__(self, notify, parent=None):
        super().__init__(parent)
        self.notify = notify
        layout = QVBoxLayout(self)

        row = QHBoxLayout()
        self.producers = QSpinBox(); self.producers.setRange(1, 8); self.producers.setValue(2)
        self.consumers = QSpinBox(); self.consumers.setRange(1, 8); self.consumers.setValue(2)
        self.buffer = QSpinBox(); self.buffer.setRange(1, 32); self.buffer.setValue(4)
        self.ipp = QSpinBox(); self.ipp.setRange(1, 50); self.ipp.setValue(5)
        self.ipc = QSpinBox(); self.ipc.setRange(1, 50); self.ipc.setValue(5)
        self.realtime = QCheckBox("Hilos reales (C++ std::thread)")
        btn = QPushButton("Simular productor-consumidor")
        btn.clicked.connect(self.run)
        for w, label in [
            (self.producers, "Productores"),
            (self.consumers, "Consumidores"),
            (self.buffer, "Buffer N"),
            (self.ipp, "Ítems/prod"),
            (self.ipc, "Ítems/cons"),
        ]:
            box = QVBoxLayout()
            box.addWidget(QLabel(label))
            box.addWidget(w)
            row.addLayout(box)
        row.addWidget(self.realtime)
        row.addStretch()
        row.addWidget(btn)
        layout.addLayout(row)

        self.chart = BarCanvas()
        layout.addWidget(self.chart)

        self.table = QTableWidget(0, 6)
        self.table.setHorizontalHeaderLabels(["Paso", "Actor", "Acción", "Detalle", "Buffer", "empty/full/mutex"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumHeight(90)
        layout.addWidget(self.log)

        hint = QLabel(
            "Módulo 2: productor-consumidor con semáforos mutex, empty y full. "
            "El núcleo verifica overflow, underflow e interbloqueo."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)

    def run(self):
        payload = {
            "module": "sync",
            "sync": {
                "producers": self.producers.value(),
                "consumers": self.consumers.value(),
                "buffer_size": self.buffer.value(),
                "items_per_producer": self.ipp.value(),
                "items_per_consumer": self.ipc.value(),
                "realtime": self.realtime.isChecked(),
            },
        }
        try:
            data = run_kernel(payload, module="sync")
        except Exception as exc:
            self.notify(str(exc), error=True)
            return
        sync = data.get("sync", {})
        occ = sync.get("occupancy") or []
        if occ:
            xs = [o.get("step", i) for i, o in enumerate(occ)]
            ys = [o.get("buffer", 0) for o in occ]
            self.chart.plot_line(xs, ys, "Ocupación del buffer", "ítems")
        events = sync.get("events", [])
        self.table.setRowCount(len(events))
        for i, e in enumerate(events):
            extra = f"{e.get('empty', '-')} / {e.get('full', '-')} / {e.get('mutex', '-')}"
            vals = [e.get("step"), e.get("actor"), e.get("action"), e.get("detail"), e.get("buffer"), extra]
            for c, v in enumerate(vals):
                self.table.setItem(i, c, QTableWidgetItem(str(v)))
        inv = sync.get("invariants", {})
        self.log.setPlainText(
            sync.get("explanation", "")
            + "\n\nInvariantes: "
            + ", ".join(f"{k}={v}" for k, v in inv.items())
        )
        self.notify("Sincronización completada.")
