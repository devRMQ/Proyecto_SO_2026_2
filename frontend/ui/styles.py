STYLESHEET = """
QWidget {
    background: #10141c;
    color: #e8eef7;
    font-family: "Segoe UI", "SF Pro Text", sans-serif;
    font-size: 13px;
}
QMainWindow, QTabWidget::pane {
    background: #10141c;
    border: 0;
}
QTabBar::tab {
    background: #1a2230;
    color: #9db0c9;
    padding: 10px 18px;
    margin-right: 4px;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
}
QTabBar::tab:selected {
    background: #243044;
    color: #ffffff;
}
QGroupBox {
    border: 1px solid #2b3b55;
    border-radius: 10px;
    margin-top: 12px;
    padding: 12px 10px 10px 10px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: #7dd3fc;
}
QPushButton {
    background: #2563eb;
    color: white;
    border: 0;
    border-radius: 8px;
    padding: 8px 14px;
    font-weight: 600;
}
QPushButton:hover { background: #1d4ed8; }
QPushButton:pressed { background: #1e40af; }
QTableWidget, QTableView, QPlainTextEdit, QTextEdit {
    background: #0d121a;
    border: 1px solid #2b3b55;
    border-radius: 8px;
    gridline-color: #223049;
}
QHeaderView::section {
    background: #1a2230;
    color: #c5d4e8;
    padding: 6px;
    border: 0;
}
QSpinBox, QComboBox, QLineEdit {
    background: #1a2230;
    border: 1px solid #33465f;
    border-radius: 6px;
    padding: 4px 8px;
    color: #e8eef7;
}
QLabel#hint {
    color: #8aa0b8;
    font-size: 12px;
}
QStatusBar {
    background: #0d121a;
    color: #9db0c9;
}
"""
