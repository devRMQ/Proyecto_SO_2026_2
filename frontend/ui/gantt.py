from __future__ import annotations

from typing import Any, Dict, List

from PySide6.QtCore import QRect, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QWidget

PALETTE = [
    QColor("#38bdf8"),
    QColor("#a78bfa"),
    QColor("#34d399"),
    QColor("#fb7185"),
    QColor("#fbbf24"),
    QColor("#60a5fa"),
    QColor("#f472b6"),
    QColor("#2dd4bf"),
]


class GanttWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.slices: List[Dict[str, Any]] = []
        self.horizon: int | None = None  # escala fija (útil para animación)
        self.setMinimumHeight(160)

    def set_slices(self, slices: List[Dict[str, Any]]):
        self.slices = slices or []
        self.update()

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor("#0d121a"))
        if not self.slices:
            painter.setPen(QColor("#8aa0b8"))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Sin diagrama de Gantt")
            return

        makespan = self.horizon or max(s.get("end", 0) for s in self.slices) or 1
        left, top, right, bottom = 48, 28, self.width() - 16, self.height() - 28
        width = max(1, right - left)
        bar_h = max(28, (bottom - top) - 20)

        painter.setPen(QPen(QColor("#33465f")))
        painter.drawLine(left, bottom, right, bottom)
        step = max(1, makespan // 40)
        for t in range(0, makespan + 1, step):
            x = left + int(t * width / makespan)
            painter.setPen(QColor("#2b3b55"))
            painter.drawLine(x, top, x, bottom)
            painter.setPen(QColor("#8aa0b8"))
            painter.setFont(QFont("Segoe UI", 8))
            painter.drawText(x - 8, bottom + 16, str(t))

        for sl in self.slices:
            pid = sl.get("pid", -1)
            x0 = left + int(sl.get("start", 0) * width / makespan)
            x1 = left + int(sl.get("end", 0) * width / makespan)
            rect = QRect(x0 + 1, top, max(2, x1 - x0 - 2), bar_h)
            color = QColor("#334155") if pid < 0 else PALETTE[pid % len(PALETTE)]
            painter.setBrush(color)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawRoundedRect(rect, 4, 4)
            painter.setPen(QColor("#0b1220") if pid >= 0 else QColor("#cbd5e1"))
            painter.setFont(QFont("Segoe UI", 9, QFont.Weight.Bold))
            label = sl.get("name", "")
            if painter.fontMetrics().horizontalAdvance(label) > rect.width() - 4:
                label = str(pid) if pid >= 0 else ""
            if painter.fontMetrics().horizontalAdvance(label) <= rect.width():
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, label)
