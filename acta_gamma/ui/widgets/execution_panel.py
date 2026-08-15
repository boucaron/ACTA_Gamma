from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel, QPushButton, QTextEdit

class ExecutionPanel(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Execution"))
        self.run = QPushButton("Run One-Shot")
        layout.addWidget(self.run)
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log)
