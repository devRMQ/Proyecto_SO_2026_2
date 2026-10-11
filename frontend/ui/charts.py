from __future__ import annotations

from typing import List, Sequence

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure


class BarCanvas(FigureCanvasQTAgg):
    def __init__(self, parent=None):
        self.fig = Figure(figsize=(5, 2.6), facecolor="#0d121a")
        super().__init__(self.fig)
        self.setParent(parent)
        self.ax = self.fig.add_subplot(111)
        self._style()

    def _style(self):
        self.ax.set_facecolor("#0d121a")
        self.ax.tick_params(colors="#9db0c9")
        self.ax.spines["bottom"].set_color("#33465f")
        self.ax.spines["left"].set_color("#33465f")
        self.ax.spines["top"].set_visible(False)
        self.ax.spines["right"].set_visible(False)
        self.ax.yaxis.label.set_color("#c5d4e8")
        self.ax.xaxis.label.set_color("#c5d4e8")
        self.ax.title.set_color("#e8eef7")
        self.fig.tight_layout()

    def plot_grouped(self, labels: Sequence[str], series: dict, title: str, ylabel: str):
        self.ax.clear()
        self._style()
        x = list(range(len(labels)))
        n = max(1, len(series))
        width = 0.8 / n
        colors = ["#38bdf8", "#a78bfa", "#34d399", "#fb7185", "#fbbf24"]
        for i, (name, values) in enumerate(series.items()):
            xs = [xi + (i - (n - 1) / 2) * width for xi in x]
            self.ax.bar(xs, values, width=width * 0.92, label=name, color=colors[i % len(colors)])
        self.ax.set_xticks(x)
        self.ax.set_xticklabels(labels)
        self.ax.set_title(title)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(facecolor="#1a2230", edgecolor="#33465f", labelcolor="#e8eef7")
        self.fig.tight_layout()
        self.draw()

    def plot_line(self, xs: List[int], ys: List[int], title: str, ylabel: str):
        self.ax.clear()
        self._style()
        self.ax.plot(xs, ys, color="#38bdf8", linewidth=2)
        self.ax.fill_between(xs, ys, color="#38bdf8", alpha=0.2)
        self.ax.set_title(title)
        self.ax.set_ylabel(ylabel)
        self.fig.tight_layout()
        self.draw()
