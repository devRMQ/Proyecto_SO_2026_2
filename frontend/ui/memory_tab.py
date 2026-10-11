from __future__ import annotations

from typing import Any, Dict, List

from PySide6.QtCore import QRect, Qt
from PySide6.QtGui import QColor, QFont, QPainter
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
from ui.gantt import PALETTE


class MemoryMap(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.blocks: List[Dict[str, Any]] = []
        self.total = 64
        self.setMinimumHeight(90)

    def set_blocks(self, blocks, total):
        self.blocks = blocks or []
        self.total = max(1, total)
        self.update()

    def paintEvent(self, _event):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor("#0d121a"))
        x, y, w, h = 12, 16, self.width() - 24, self.height() - 32
        for b in self.blocks:
            x0 = x + int(b.get("start", 0) * w / self.total)
            bw = max(4, int(b.get("size", 0) * w / self.total) - 2)
            pid = b.get("pid", -1)
            color = QColor("#334155") if pid < 0 else PALETTE[pid % len(PALETTE)]
            p.setBrush(color)
            p.setPen(Qt.PenStyle.NoPen)
            p.drawRoundedRect(QRect(x0, y, bw, h), 4, 4)
            p.setPen(QColor("#0b1220") if pid >= 0 else QColor("#cbd5e1"))
            p.setFont(QFont("Segoe UI", 8, QFont.Weight.Bold))
            label = "FREE" if pid < 0 else f"P{pid}"
            p.drawText(QRect(x0, y, bw, h), Qt.AlignmentFlag.AlignCenter, f"{label}\n{b.get('size')}u")


class MemoryTab(QWidget):
    def __init__(self, notify, parent=None):
        super().__init__(parent)
        self.notify = notify
        layout = QVBoxLayout(self)

        top = QHBoxLayout()
        self.total = QSpinBox(); self.total.setRange(16, 512); self.total.setValue(64)
        self.strategy = QComboBox(); self.strategy.addItems(["FIRST_FIT", "BEST_FIT", "WORST_FIT"])
        self.frames = QSpinBox(); self.frames.setRange(1, 12); self.frames.setValue(3)
        self.refs = QTableWidget(1, 1)
        self.compare = QCheckBox("Comparar FIFO / LRU / Óptimo")
        self.compare.setChecked(True)
        btn = QPushButton("Simular memoria")
        btn.clicked.connect(self.run)
        for w, label in [(self.total, "Memoria"), (self.strategy, "Asignación"), (self.frames, "Marcos")]:
            box = QVBoxLayout(); box.addWidget(QLabel(label)); box.addWidget(w); top.addLayout(box)
        top.addWidget(self.compare)
        top.addStretch()
        top.addWidget(btn)
        layout.addLayout(top)

        mid = QHBoxLayout()
        self.req = QTableWidget(0, 3)
        self.req.setHorizontalHeaderLabels(["op", "pid", "size"])
        self.req.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        for r in [
            ("ALLOC", 1, 20), ("ALLOC", 2, 16), ("ALLOC", 3, 10),
            ("FREE", 2, 0), ("ALLOC", 4, 14), ("FREE", 1, 0), ("ALLOC", 5, 18),
        ]:
            row = self.req.rowCount(); self.req.insertRow(row)
            for c, v in enumerate(r):
                self.req.setItem(row, c, QTableWidgetItem(str(v)))
        mid.addWidget(self.req, 1)

        self.ref_edit = QTableWidget(1, 20)
        refs = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1]
        self.ref_edit.setHorizontalHeaderLabels([str(i) for i in range(20)])
        for i, v in enumerate(refs):
            self.ref_edit.setItem(0, i, QTableWidgetItem(str(v)))
        mid.addWidget(self.ref_edit, 2)
        layout.addLayout(mid)

        self.mmap = MemoryMap()
        layout.addWidget(self.mmap)
        self.chart = BarCanvas()
        layout.addWidget(self.chart)

        self.steps = QTableWidget(0, 5)
        self.steps.setHorizontalHeaderLabels(["t", "página", "fallo", "reemplazo", "marcos"])
        self.steps.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.steps)

        hint = QLabel(
            "Módulo 3: asignación contigua (first/best/worst fit y fragmentación externa) "
            "y reemplazo de páginas FIFO, LRU y Óptimo sobre el mismo patrón de acceso."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)

    def _requests(self):
        out = []
        for row in range(self.req.rowCount()):
            def cell(c, d="0"):
                it = self.req.item(row, c)
                return it.text() if it else d
            out.append({"op": cell(0, "ALLOC"), "pid": int(cell(1)), "size": int(cell(2) or 0)})
        return out

    def _refs(self):
        refs = []
        for c in range(self.ref_edit.columnCount()):
            it = self.ref_edit.item(0, c)
            if it and it.text().strip():
                refs.append(int(it.text()))
        return refs

    def run(self):
        payload = {
            "module": "memory",
            "memory": {
                "contiguous": {
                    "total_memory": self.total.value(),
                    "strategy": self.strategy.currentText(),
                    "requests": self._requests(),
                },
                "paging": {
                    "frames": self.frames.value(),
                    "compare": self.compare.isChecked(),
                    "algorithm": "FIFO",
                    "references": self._refs(),
                },
            },
        }
        try:
            data = run_kernel(payload, module="memory")
        except Exception as exc:
            self.notify(str(exc), error=True)
            return
        mem = data.get("memory", {})
        contig = mem.get("contiguous", {})
        snaps = contig.get("snapshots", [])
        if snaps:
            last = snaps[-1]
            self.mmap.set_blocks(last.get("blocks", []), contig.get("total_memory", 64))
        paging = mem.get("paging", {})
        results = paging.get("results", [])
        if results:
            labels = [r.get("algorithm", "?") for r in results]
            series = {"Fallos": [r.get("page_faults", 0) for r in results]}
            self.chart.plot_grouped(labels, series, "Fallos de página (mismo patrón)", "fallos")
            first = results[0]
            steps = first.get("steps", [])
            self.steps.setRowCount(len(steps))
            for i, s in enumerate(steps):
                vals = [s.get("t"), s.get("page"), s.get("fault"), s.get("replaced"), s.get("frames")]
                for c, v in enumerate(vals):
                    self.steps.setItem(i, c, QTableWidgetItem(str(v)))
        self.notify("Memoria simulada.")
