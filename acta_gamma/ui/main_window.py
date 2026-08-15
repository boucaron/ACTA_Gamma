from pathlib import Path
from PySide6.QtWidgets import QApplication, QMainWindow, QHBoxLayout, QVBoxLayout, QWidget, QLabel, QSplitter
from PySide6.QtGui import QPixmap
from .widgets.skill_panel import SkillPanel
from .widgets.model_panel import ModelPanel
from .widgets.context_panel import ContextPanel
from .widgets.execution_panel import ExecutionPanel

ASSETS = Path(__file__).parents[2] / "assets" / "logo.jpg"

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ACTA Gamma")
        self.resize(1200, 700)

        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        # Logo bar
        header = QWidget()
        h = QHBoxLayout(header)
        logo = QLabel()
        pix = QPixmap(str(ASSETS))
        if not pix.isNull():
            logo.setPixmap(pix.scaledToHeight(32))
        title = QLabel("ACTA Gamma — LLMs as actions, not agents")
        h.addWidget(logo)
        h.addWidget(title)
        h.addStretch()
        root.addWidget(header)

        # Panels
        splitter = QSplitter()
        splitter.setOrientation(QSplitter.Horizontal)

        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.addWidget(SkillPanel())
        left_layout.addWidget(ModelPanel())
        left.setMaximumWidth(320)
        splitter.addWidget(left)

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.addWidget(ContextPanel())
        right_layout.addWidget(ExecutionPanel())
        splitter.addWidget(right)

        splitter.setSizes([320, 880])
        root.addWidget(splitter, 1)

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
