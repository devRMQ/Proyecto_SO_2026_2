#!/usr/bin/env python3
"""Punto de entrada de la GUI. El núcleo C++ se invoca con subprocess."""
from __future__ import annotations

import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from PySide6.QtWidgets import QApplication, QMessageBox

from engine import ensure_built
from ui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("SO-Sim")
    try:
        ensure_built()
    except Exception as exc:
        QMessageBox.critical(None, "No se pudo compilar el núcleo", str(exc))
        return 1
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
