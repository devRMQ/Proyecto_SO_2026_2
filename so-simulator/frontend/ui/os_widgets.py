"""Widgets de dibujo para la simulación integrada del SO."""
from __future__ import annotations

from typing import Any, Dict, List, Tuple

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QSizePolicy, QWidget

from ui.gantt import PALETTE

BG = QColor("#0d121a")
GRID = QColor("#33465f")
MUTED = QColor("#8aa0b8")

STATE_COLORS = {
    "NEW": "#64748b",
    "JOB_WAIT": "#f59e0b",
    "READY": "#38bdf8",
    "RUNNING": "#22c55e",
    "BLOCKED_PF": "#fb7185",
    "BLOCKED_SEM": "#a78bfa",
    "TERMINATED": "#475569",
    "REJECTED": "#ef4444",
}

STATE_LABELS = {
    "NEW": "Nuevo",
    "JOB_WAIT": "Espera memoria",
    "READY": "Listo",
    "RUNNING": "Ejecutando",
    "BLOCKED_PF": "Fallo de página",
    "BLOCKED_SEM": "Bloq. semáforo",
    "TERMINATED": "Terminado",
    "REJECTED": "Rechazado",
}


def pid_color(pid: int) -> QColor:
    return QColor("#334155") if pid < 0 else PALETTE[pid % len(PALETTE)]


class ChipRow(QWidget):
    """Fila de 'fichas' (procesos) con etiqueta; se usa para las colas."""

    def __init__(self, empty_text: str = "vacía", parent=None):
        super().__init__(parent)
        self.items: List[Tuple[str, int]] = []
        self.empty_text = empty_text
        self.setMinimumHeight(34)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

    def set_items(self, items: List[Tuple[str, int]]):
        self.items = items
        self.update()

    def paintEvent(self, _e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), BG)
        if not self.items:
            p.setPen(MUTED)
            p.drawText(self.rect().adjusted(10, 0, 0, 0), Qt.AlignmentFlag.AlignVCenter, self.empty_text)
            return
        x = 6
        p.setFont(QFont("Segoe UI", 9, QFont.Weight.Bold))
        fm = p.fontMetrics()
        for text, pid in self.items:
            w = fm.horizontalAdvance(text) + 18
            rect = QRectF(x, 4, w, self.height() - 8)
            p.setBrush(pid_color(pid))
            p.setPen(Qt.PenStyle.NoPen)
            p.drawRoundedRect(rect, 8, 8)
            p.setPen(QColor("#0b1220"))
            p.drawText(rect, Qt.AlignmentFlag.AlignCenter, text)
            x += w + 6
            if x > self.width():
                break


class CpuWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.tick = 0
        self.name = ""
        self.pid = -1
        self.remaining = 0
        self.quantum = ""
        self.setMinimumHeight(74)

    def set_state(self, tick: int, name: str, pid: int, remaining: int, quantum: str):
        self.tick, self.name, self.pid, self.remaining, self.quantum = tick, name, pid, remaining, quantum
        self.update()

    def paintEvent(self, _e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), BG)
        box = QRectF(10, 8, 150, self.height() - 16)
        col = pid_color(self.pid) if self.pid >= 0 else QColor("#334155")
        p.setBrush(col)
        p.setPen(QPen(QColor("#0b1220"), 2))
        p.drawRoundedRect(box, 10, 10)
        p.setPen(QColor("#0b1220") if self.pid >= 0 else QColor("#cbd5e1"))
        p.setFont(QFont("Segoe UI", 10, QFont.Weight.Bold))
        p.drawText(box.adjusted(0, 6, 0, 0), Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignTop, "CPU")
        p.setFont(QFont("Segoe UI", 13, QFont.Weight.Bold))
        p.drawText(box.adjusted(0, 18, 0, 0), Qt.AlignmentFlag.AlignCenter, self.name if self.pid >= 0 else "ociosa")
        p.setPen(QColor("#e8eef7"))
        p.setFont(QFont("Segoe UI", 11))
        p.drawText(180, 34, f"Tick: {self.tick}")
        if self.pid >= 0:
            p.drawText(180, 58, f"PID {self.pid} · restante {self.remaining} · {self.quantum}")


class MemoryWidget(QWidget):
    """Mapa de memoria: barra de bloques (contigua) o rejilla de marcos (paginada)."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.mem: Dict[str, Any] = {}
        self.setMinimumHeight(150)

    def set_memory(self, mem: Dict[str, Any]):
        self.mem = mem or {}
        self.update()

    def paintEvent(self, _e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), BG)
        if not self.mem:
            return
        if self.mem.get("mode") == "contiguous":
            self._paint_contiguous(p)
        else:
            self._paint_paged(p)

    def _paint_contiguous(self, p: QPainter):
        total = max(1, self.mem["total"])
        left, right = 12, self.width() - 12
        width = right - left
        top, h = 34, 56
        p.setPen(QColor("#e8eef7"))
        p.setFont(QFont("Segoe UI", 10))
        p.drawText(12, 20, f"Memoria contigua · libre {self.mem['free']} u. · hueco mayor {self.mem['largest_hole']} u. · "
                           f"huecos {self.mem['holes']}")
        for b in self.mem["blocks"]:
            x0 = left + b["start"] * width / total
            w = b["size"] * width / total
            rect = QRectF(x0, top, max(2.0, w - 1), h)
            if b["pid"] < 0:
                p.setBrush(QColor("#1e293b"))
                p.setPen(QPen(GRID, 1, Qt.PenStyle.DashLine))
            else:
                p.setBrush(pid_color(b["pid"]))
                p.setPen(Qt.PenStyle.NoPen)
            p.drawRoundedRect(rect, 4, 4)
            p.setPen(QColor("#0b1220") if b["pid"] >= 0 else MUTED)
            p.setFont(QFont("Segoe UI", 9, QFont.Weight.Bold))
            label = (f"P{b['pid']}" if b["pid"] >= 0 else "libre") + f"\n{b['size']}"
            p.drawText(rect, Qt.AlignmentFlag.AlignCenter, label)
        p.setPen(MUTED)
        p.setFont(QFont("Segoe UI", 8))
        p.drawText(left, top + h + 16, "0")
        p.drawText(right - 24, top + h + 16, str(total))

    def _paint_paged(self, p: QPainter):
        frames = self.mem["frames"]
        n = len(frames)
        p.setPen(QColor("#e8eef7"))
        p.setFont(QFont("Segoe UI", 10))
        p.drawText(12, 20, f"RAM paginada · {n} marcos · fallos acumulados {self.mem['faults']} "
                           f"de {self.mem['refs']} referencias")
        cols = min(n, 8)
        rows = (n + cols - 1) // cols
        cw = (self.width() - 24) / cols
        ch = min(52.0, (self.height() - 40) / max(1, rows))
        for i, f in enumerate(frames):
            r, c = divmod(i, cols)
            rect = QRectF(12 + c * cw + 2, 30 + r * ch + 2, cw - 4, ch - 4)
            if f["pid"] < 0:
                p.setBrush(QColor("#1e293b"))
                p.setPen(QPen(GRID, 1, Qt.PenStyle.DashLine))
                text = f"M{f['frame']}\nlibre"
                tcol = MUTED
            else:
                p.setBrush(pid_color(f["pid"]))
                p.setPen(QPen(QColor("#fb7185"), 2, Qt.PenStyle.DashLine) if f["loading"] else Qt.PenStyle.NoPen)
                text = f"M{f['frame']}\nP{f['pid']}:pág {f['page']}" + (" …" if f["loading"] else "")
                tcol = QColor("#0b1220")
            p.drawRoundedRect(rect, 6, 6)
            p.setPen(tcol)
            p.setFont(QFont("Segoe UI", 8, QFont.Weight.Bold))
            p.drawText(rect, Qt.AlignmentFlag.AlignCenter, text)


class SyncWidget(QWidget):
    """Semáforos y buffer del productor-consumidor."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sync: Dict[str, Any] = {}
        self.setMinimumHeight(86)

    def set_sync(self, sync: Dict[str, Any]):
        self.sync = sync or {}
        self.update()

    def paintEvent(self, _e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), BG)
        if not self.sync:
            p.setPen(MUTED)
            p.drawText(self.rect().adjusted(10, 0, 0, 0), Qt.AlignmentFlag.AlignVCenter,
                       "Sin procesos productor/consumidor en este escenario")
            return
        p.setPen(QColor("#e8eef7"))
        p.setFont(QFont("Segoe UI", 10, QFont.Weight.Bold))
        p.drawText(12, 20, f"mutex = {self.sync['mutex']}    empty = {self.sync['empty']}    "
                           f"full = {self.sync['full']}")
        cap = max(1, self.sync["capacity"])
        cw = min(46.0, (self.width() - 24) / cap)
        for i in range(cap):
            rect = QRectF(12 + i * (cw + 4), 32, cw, 38)
            filled = i < self.sync["buffer"]
            p.setBrush(QColor("#34d399") if filled else QColor("#1e293b"))
            p.setPen(QPen(GRID, 1))
            p.drawRoundedRect(rect, 5, 5)
            p.setPen(QColor("#0b1220") if filled else MUTED)
            p.drawText(rect, Qt.AlignmentFlag.AlignCenter, "ítem" if filled else "·")
