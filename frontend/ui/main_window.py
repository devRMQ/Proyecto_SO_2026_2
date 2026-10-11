from __future__ import annotations

from PySide6.QtWidgets import QLabel, QMainWindow, QMessageBox, QStatusBar, QTabWidget

from ui.analysis_tab import AnalysisTab
from ui.memory_tab import MemoryTab
from ui.os_tab import OsTab
from ui.schedule_tab import ScheduleTab
from ui.styles import STYLESHEET
from ui.sync_tab import SyncTab


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SO-Sim — Simulador educativo de Sistema Operativo (procesos, memoria y sincronización)")
        self.resize(1280, 820)
        self.setStyleSheet(STYLESHEET)

        tabs = QTabWidget()
        self.os_tab = OsTab(self.notify)
        tabs.addTab(self.os_tab, "SO integrado")
        tabs.addTab(AnalysisTab(self.notify, self.os_tab.build_payload), "Análisis comparativo")
        tabs.addTab(ScheduleTab(self.notify), "Lab 1: Planificación")
        tabs.addTab(SyncTab(self.notify), "Lab 2: Sincronización")
        tabs.addTab(MemoryTab(self.notify), "Lab 3: Memoria")
        self.setCentralWidget(tabs)

        bar = QStatusBar()
        bar.addWidget(QLabel("Núcleo C++ (sosim) + GUI Python · JSON en shared_data/"))
        self.setStatusBar(bar)

    def notify(self, message: str, error: bool = False):
        if error:
            QMessageBox.critical(self, "Error", message)
        self.statusBar().showMessage(message, 8000)
