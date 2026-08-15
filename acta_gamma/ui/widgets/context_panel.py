from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel, QTextEdit

class ContextPanel(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Context"))
        self.editor = QTextEdit()
        self.editor.setPlaceholderText("Immutable input JSON...")
        layout.addWidget(self.editor)
