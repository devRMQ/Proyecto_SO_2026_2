"""Pestaña principal: simulación integrada del SO (planificación + memoria + sincronización)."""
from __future__ import annotations

import json
from typing import Any, Dict, List

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QScrollArea,
    QSlider,
    QSpinBox,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from engine import EXAMPLES, run_kernel
from ui.gantt import GanttWidget
from ui.os_widgets import (
    STATE_COLORS,
    STATE_LABELS,
    ChipRow,
    CpuWidget,
    MemoryWidget,
    SyncWidget,
    pid_color,
)

DEFAULT_PROCS = [
    # pid, nombre, llegada, ráfaga, prioridad, hilos, padre, rol, mem(u), páginas
    (1, "Init", 0, 8, 3, 1, -1, "normal", 16, 4),
    (2, "Shell", 1, 6, 2, 2, 1, "normal", 12, 3),
    (3, "Editor", 2, 10, 1, 3, 2, "normal", 24, 5),
    (4, "Productor", 0, 6, 2, 1, 1, "producer", 8, 2),
    (5, "Consumidor", 1, 6, 2, 1, 1, "consumer", 8, 2),
]
HEADERS = ["PID", "Nombre", "Lleg.", "Ráf.", "Prior", "Hilos", "Padre", "Rol", "Mem", "Pág."]
ROLES = ["normal", "producer", "consumer"]


class OsTab(QWidget):
    def __init__(self, notify, parent=None):
        super().__init__(parent)
        self.notify = notify
        self.result: Dict[str, Any] = {}
        self.frames: List[Dict[str, Any]] = []
        self.pids: Dict[int, Dict[str, Any]] = {}
        self.cur = 0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._advance)

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        outer.addWidget(scroll)
        content = QWidget()
        content.setMinimumHeight(800)
        scroll.setWidget(content)
        root = QVBoxLayout(content)
        root.addWidget(self._build_config())
        root.addWidget(self._build_player())

        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(self._build_left())
        split.addWidget(self._build_right())
        split.setSizes([760, 500])
        root.addWidget(split, 1)

        self.gantt = GanttWidget()
        self.gantt.setMinimumHeight(110)
        self.gantt.setMaximumHeight(130)
        root.addWidget(self.gantt)

        self.summary = QLabel("Pulsa «Simular» para ejecutar el SO.")
        self.summary.setObjectName("hint")
        self.summary.setWordWrap(True)
        root.addWidget(self.summary)

        self._fill_table(DEFAULT_PROCS)
        self._mem_mode_changed()

    # ------------------------------------------------------------------ construcción de la UI
    def _build_config(self) -> QGroupBox:
        box = QGroupBox("Configuración del sistema")
        g = QGridLayout(box)

        self.algo = QComboBox(); self.algo.addItems(["FCFS", "SJF", "RR", "PRIORITY"]); self.algo.setCurrentText("RR")
        self.quantum = QSpinBox(); self.quantum.setRange(1, 50); self.quantum.setValue(3)
        self.preemptive = QCheckBox("Apropiativo (SJF/Prioridad)")
        self.mem_mode = QComboBox(); self.mem_mode.addItems(["Paginada", "Contigua"])
        self.mem_mode.currentIndexChanged.connect(self._mem_mode_changed)
        self.frames_spin = QSpinBox(); self.frames_spin.setRange(1, 32); self.frames_spin.setValue(6)
        self.replacement = QComboBox(); self.replacement.addItems(["FIFO", "LRU", "OPTIMAL"]); self.replacement.setCurrentText("LRU")
        self.penalty = QSpinBox(); self.penalty.setRange(1, 20); self.penalty.setValue(3)
        self.total_mem = QSpinBox(); self.total_mem.setRange(8, 512); self.total_mem.setValue(48)
        self.strategy = QComboBox(); self.strategy.addItems(["FIRST_FIT", "BEST_FIT", "WORST_FIT"])
        self.buffer = QSpinBox(); self.buffer.setRange(1, 10); self.buffer.setValue(2)

        def put(label, widget, row, col):
            g.addWidget(QLabel(label), row, col * 2)
            g.addWidget(widget, row, col * 2 + 1)

        put("Planificador", self.algo, 0, 0)
        put("Quantum", self.quantum, 0, 1)
        g.addWidget(self.preemptive, 0, 4, 1, 2)
        put("Memoria", self.mem_mode, 1, 0)
        put("Marcos", self.frames_spin, 1, 1)
        put("Reemplazo", self.replacement, 1, 2)
        put("Penaliz. fallo (ticks)", self.penalty, 1, 3)
        put("Mem. total (u)", self.total_mem, 1, 4)
        put("Asignación", self.strategy, 1, 5)
        put("Buffer prod/cons", self.buffer, 0, 3)
        return box

    def _build_player(self) -> QWidget:
        w = QWidget()
        h = QHBoxLayout(w)
        h.setContentsMargins(0, 0, 0, 0)
        add = QPushButton("Añadir proceso"); add.clicked.connect(self._add_row)
        rem = QPushButton("Quitar"); rem.clicked.connect(self._remove_row)
        load = QPushButton("Cargar escenario…"); load.clicked.connect(self._load_file)
        run = QPushButton("▶ Simular"); run.clicked.connect(self.simulate)
        self.btn_first = QPushButton("⏮"); self.btn_prev = QPushButton("◀")
        self.btn_play = QPushButton("Reproducir"); self.btn_next = QPushButton("▶")
        self.btn_last = QPushButton("⏭")
        self.btn_first.clicked.connect(lambda: self._goto(0))
        self.btn_prev.clicked.connect(lambda: self._goto(self.cur - 1))
        self.btn_next.clicked.connect(lambda: self._goto(self.cur + 1))
        self.btn_last.clicked.connect(lambda: self._goto(len(self.frames) - 1))
        self.btn_play.clicked.connect(self._toggle_play)
        self.slider = QSlider(Qt.Orientation.Horizontal)
        self.slider.valueChanged.connect(self._goto)
        self.speed = QComboBox(); self.speed.addItems(["0.5x", "1x", "2x", "5x", "10x"]); self.speed.setCurrentText("2x")
        self.speed.currentIndexChanged.connect(self._retime)
        self.tick_lbl = QLabel("t = –")
        for b in (add, rem, load, run):
            h.addWidget(b)
        h.addSpacing(16)
        for b in (self.btn_first, self.btn_prev, self.btn_play, self.btn_next, self.btn_last):
            h.addWidget(b)
        h.addWidget(self.slider, 1)
        h.addWidget(QLabel("Velocidad"))
        h.addWidget(self.speed)
        h.addWidget(self.tick_lbl)
        return w

    def _build_left(self) -> QWidget:
        top = QWidget()
        v = QVBoxLayout(top)
        v.setContentsMargins(0, 0, 0, 0)
        self.cpu = CpuWidget()
        v.addWidget(self.cpu)
        self.q_ready = ChipRow("vacía")
        self.q_pf = ChipRow("ninguno")
        self.q_sem = ChipRow("ninguno")
        self.q_job = ChipRow("ninguno")
        qg = QGridLayout()
        qg.setVerticalSpacing(2)
        for r, (lbl, q) in enumerate((("Listos", self.q_ready), ("Fallo de página", self.q_pf),
                                      ("Semáforo", self.q_sem), ("Esperan memoria", self.q_job))):
            name = QLabel(lbl)
            name.setFixedWidth(110)
            qg.addWidget(name, r, 0)
            qg.addWidget(q, r, 1)
        v.addLayout(qg)

        bottom = QWidget()
        bv = QVBoxLayout(bottom)
        bv.setContentsMargins(0, 0, 0, 0)
        # tabla editable de procesos (entrada) / PCB (estado en vivo) en la misma tabla
        self.table = QTableWidget(0, len(HEADERS) + 3)
        self.table.setHorizontalHeaderLabels(HEADERS + ["Estado", "Rest.", "Fallos"])
        hdr = self.table.horizontalHeader()
        hdr.setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        for fixed in (1, 7, 10):  # Nombre, Rol y Estado se ajustan a su contenido
            hdr.setSectionResizeMode(fixed, QHeaderView.ResizeMode.ResizeToContents)
        hdr.setMinimumSectionSize(40)
        hdr.setStretchLastSection(False)
        self.table.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.table.verticalHeader().setVisible(False)
        self.table.verticalHeader().setDefaultSectionSize(34)
        self.table.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.table_title = QLabel("Procesos (editable) y PCB en vivo")
        bv.addWidget(self.table_title)
        bv.addWidget(self.table, 1)

        split = QSplitter(Qt.Orientation.Vertical)
        split.addWidget(top)
        split.addWidget(bottom)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setChildrenCollapsible(False)
        return split

    def _fit_table(self):
        """Ajusta la altura de la tabla para que se vean TODAS las filas (hasta 10)."""
        rows = self.table.rowCount()
        h = self.table.horizontalHeader().height() + 6
        for r in range(rows):
            h += self.table.rowHeight(r)
        self.table.setMinimumHeight(min(h, 10 * 34 + 60))
        self.table_title.setText(f"Procesos (editable) y PCB en vivo — {rows} proceso(s)")

    def _build_right(self) -> QWidget:
        w = QWidget()
        v = QVBoxLayout(w)
        v.setContentsMargins(0, 0, 0, 0)
        v.addWidget(QLabel("Memoria"))
        self.mem_w = MemoryWidget()
        v.addWidget(self.mem_w, 1)
        v.addWidget(QLabel("Sincronización (productor-consumidor con semáforos)"))
        self.sync_w = SyncWidget()
        v.addWidget(self.sync_w)
        v.addWidget(QLabel("Registro del kernel"))
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        v.addWidget(self.log, 1)
        return w

    # ------------------------------------------------------------------ tabla de procesos
    def _fill_table(self, rows):
        self.table.setRowCount(0)
        for r in rows:
            self._add_row(values=r)
        self._fit_table()

    def _add_row(self, _checked=False, values=None):
        if not isinstance(values, (tuple, list)):
            used = []
            for r0 in range(self.table.rowCount()):
                try:
                    used.append(int(self.table.item(r0, 0).text()))
                except (ValueError, AttributeError):
                    pass
            nxt = max(used, default=0) + 1
            values = (nxt, f"P{nxt}", 0, 5, 1, 1, -1, "normal", 8, 3)
        row = self.table.rowCount()
        self.table.insertRow(row)
        for c, v in enumerate(values):
            if c == 7:
                cb = QComboBox(); cb.addItems(ROLES); cb.setCurrentText(str(v))
                self.table.setCellWidget(row, c, cb)
            else:
                it = QTableWidgetItem(str(v))
                it.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                self.table.setItem(row, c, it)
        for c in range(len(HEADERS), len(HEADERS) + 3):
            it = QTableWidgetItem("")
            it.setFlags(Qt.ItemFlag.ItemIsEnabled)
            it.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(row, c, it)
        self._fit_table()

    def _remove_row(self):
        r = self.table.currentRow()
        if r < 0:
            r = self.table.rowCount() - 1
        if r >= 0:
            self.table.removeRow(r)
            self._fit_table()

    def _mem_mode_changed(self):
        paged = self.mem_mode.currentText() == "Paginada"
        for w in (self.frames_spin, self.replacement, self.penalty):
            w.setEnabled(paged)
        for w in (self.total_mem, self.strategy):
            w.setEnabled(not paged)

    # ------------------------------------------------------------------ payload / escenarios
    def build_payload(self, compare: bool = False) -> Dict[str, Any]:
        procs = []
        for r in range(self.table.rowCount()):
            def num(c, d=0):
                it = self.table.item(r, c)
                try:
                    return int(it.text())
                except (ValueError, AttributeError):
                    return d
            name_it = self.table.item(r, 1)
            procs.append({
                "pid": num(0, r + 1), "name": name_it.text() if name_it else f"P{r + 1}",
                "arrival": num(2), "burst": max(1, num(3, 1)), "priority": num(4, 1), "threads": num(5, 1),
                "parent_pid": num(6, -1), "role": self.table.cellWidget(r, 7).currentText(),
                "mem_size": max(1, num(8, 8)), "pages": max(1, num(9, 3)),
            })
        return {"module": "os", "os": {
            "scheduler": {"algorithm": self.algo.currentText(), "quantum": self.quantum.value(),
                          "preemptive": self.preemptive.isChecked()},
            "memory": {"mode": "paged" if self.mem_mode.currentText() == "Paginada" else "contiguous",
                       "frames": self.frames_spin.value(), "replacement": self.replacement.currentText(),
                       "page_fault_penalty": self.penalty.value(), "total_memory": self.total_mem.value(),
                       "strategy": self.strategy.currentText()},
            "sync": {"buffer_size": self.buffer.value()},
            "compare": compare, "processes": procs}}

    def _load_file(self):
        path, _ = QFileDialog.getOpenFileName(self, "Cargar escenario", str(EXAMPLES), "JSON (*.json)")
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as fh:
                data = json.load(fh)
            self.apply_scenario(data.get("os", data))
        except Exception as exc:  # noqa: BLE001
            self.notify(f"No se pudo cargar el escenario: {exc}", True)

    def apply_scenario(self, o: Dict[str, Any]):
        s, m = o.get("scheduler", {}), o.get("memory", {})
        self.algo.setCurrentText(s.get("algorithm", "RR"))
        self.quantum.setValue(int(s.get("quantum", 3)))
        self.preemptive.setChecked(bool(s.get("preemptive", False)))
        self.mem_mode.setCurrentText("Contigua" if m.get("mode") == "contiguous" else "Paginada")
        self.frames_spin.setValue(int(m.get("frames", 6)))
        self.replacement.setCurrentText(m.get("replacement", "LRU"))
        self.penalty.setValue(int(m.get("page_fault_penalty", 3)))
        self.total_mem.setValue(int(m.get("total_memory", 48)))
        self.strategy.setCurrentText(m.get("strategy", "FIRST_FIT"))
        self.buffer.setValue(int(o.get("sync", {}).get("buffer_size", 2)))
        self._fill_table([(p.get("pid", i + 1), p.get("name", f"P{i + 1}"), p.get("arrival", 0), p.get("burst", 1),
                           p.get("priority", 1), p.get("threads", 1), p.get("parent_pid", -1),
                           p.get("role", "normal"), p.get("mem_size", 8), p.get("pages", 3))
                          for i, p in enumerate(o.get("processes", []))])
        self.notify("Escenario cargado.")

    # ------------------------------------------------------------------ simulación y reproducción
    def simulate(self):
        self.timer.stop()
        self.btn_play.setText("Reproducir")
        payload = self.build_payload()
        pids = [p["pid"] for p in payload["os"]["processes"]]
        if len(pids) != len(set(pids)):
            dup = sorted({p for p in pids if pids.count(p) > 1})
            self.notify(f"Hay PIDs repetidos en la tabla: {dup}. Cada proceso debe tener un PID único.", True)
            return
        try:
            data = run_kernel(payload, "os")
        except Exception as exc:  # noqa: BLE001
            self.notify(str(exc), True)
            return
        self.result = data.get("os", {})
        self.frames = self.result.get("frames", [])
        self.pids = {p["pid"]: p for p in self.result.get("processes", [])}
        if not self.frames:
            self.notify("La simulación no produjo ticks.", True)
            return
        self.slider.blockSignals(True)
        self.slider.setRange(0, len(self.frames) - 1)
        self.slider.setValue(0)
        self.slider.blockSignals(False)
        self.gantt.horizon = len(self.frames)
        self.log.clear()
        self._logged_until = -1
        self._goto(0)
        self._show_summary()
        self._toggle_play()

    def _toggle_play(self):
        if not self.frames:
            return
        if self.timer.isActive():
            self.timer.stop()
            self.btn_play.setText("Reproducir")
        else:
            if self.cur >= len(self.frames) - 1:
                self._goto(0)
            self.timer.start(self._interval())
            self.btn_play.setText("Pausa")

    def _interval(self) -> int:
        speed = float(self.speed.currentText().rstrip("x"))
        return int(800 / speed)

    def _retime(self):
        if self.timer.isActive():
            self.timer.start(self._interval())

    def _advance(self):
        if self.cur >= len(self.frames) - 1:
            self.timer.stop()
            self.btn_play.setText("Reproducir")
            return
        self._goto(self.cur + 1)

    def _goto(self, i: int):
        if not self.frames:
            return
        i = max(0, min(i, len(self.frames) - 1))
        self.cur = i
        self.slider.blockSignals(True)
        self.slider.setValue(i)
        self.slider.blockSignals(False)
        self._render(self.frames[i])

    def _name(self, pid: int) -> str:
        return self.pids.get(pid, {}).get("name", f"P{pid}")

    def _render(self, f: Dict[str, Any]):
        self.tick_lbl.setText(f"t = {f['tick']} / {len(self.frames) - 1}")
        cpu = f["cpu"]
        info = self.pids.get(cpu, {})
        remaining = next((p["remaining"] for p in f["procs"] if p["pid"] == cpu), 0)
        quantum = f"quantum {self.quantum.value()}" if self.algo.currentText() == "RR" else self.algo.currentText()
        self.cpu.set_state(f["tick"], info.get("name", ""), cpu, remaining, quantum)
        chip = lambda pid: (f"{self._name(pid)}", pid)  # noqa: E731
        self.q_ready.set_items([chip(p) for p in f["ready"]])
        self.q_pf.set_items([chip(p) for p in f["blocked_pf"]])
        self.q_sem.set_items([(f"{self._name(b['pid'])} ⏳{b['sem']}", b["pid"]) for b in f["blocked_sem"]])
        self.q_job.set_items([chip(p) for p in f["job_queue"]])
        self.mem_w.set_memory(f["memory"])
        self.sync_w.set_sync(f.get("sync", {}))

        # PCB en vivo
        by_pid = {p["pid"]: p for p in f["procs"]}
        for r in range(self.table.rowCount()):
            try:
                pid = int(self.table.item(r, 0).text())
            except (ValueError, AttributeError):
                continue
            st = by_pid.get(pid)
            if not st:
                continue
            vals = [STATE_LABELS.get(st["state"], st["state"]), str(st["remaining"]), str(st["faults"])]
            for k, text in enumerate(vals):
                it = self.table.item(r, len(HEADERS) + k)
                it.setText(text)
                it.setForeground(QColor(STATE_COLORS.get(st["state"], "#e8eef7")))

        # Gantt hasta el tick actual
        slices = [s for s in self.result.get("gantt", []) if s["start"] <= f["tick"]]
        slices = [dict(s, end=min(s["end"], f["tick"] + 1)) for s in slices]
        self.gantt.set_slices(slices)

        # log acumulado
        entries = self.result.get("log", [])
        if f["tick"] <= self._logged_until:
            self.log.clear()
            self._logged_until = -1
        lines = [f"[t={e['tick']:>3}] {e['text']}" for e in entries if self._logged_until < e["tick"] <= f["tick"]]
        if lines:
            self.log.appendPlainText("\n".join(lines))
        self._logged_until = f["tick"]

    def _show_summary(self):
        r = self.result
        m = r["metrics"]
        txt = (f"Resultado: {m['total_ticks']} ticks · CPU {m['cpu_utilization']:.0f}% · espera media {m['avg_waiting']:.1f} · "
               f"retorno medio {m['avg_turnaround']:.1f} · respuesta media {m['avg_response']:.1f} · "
               f"cambios de contexto {m['context_switches']}")
        if r["memory"]["mode"] == "paged":
            txt += f" · fallos de página {m['page_faults']} ({m['fault_rate']:.0f}%)"
        else:
            txt += f" · fragmentación ext. media {r['memory']['avg_external_fragmentation']:.1f} u."
        s = r["sync"]
        if s.get("enabled"):
            txt += (f" · sync: producidos {s['produced']}/consumidos {s['consumed']}, "
                    f"{'sin carreras' if s['invariants_ok'] else 'INVARIANTE ROTO'}")
        if r.get("deadlock"):
            txt += " · ⚠ INTERBLOQUEO detectado"
        self.summary.setText(txt)
