from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel, QComboBox

class ModelPanel(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Model"))
        self.model = QComboBox()
        self.model.addItems(["llamacpp/local-7b", "openai/gpt-4o-mini"])
        layout.addWidget(self.model)
