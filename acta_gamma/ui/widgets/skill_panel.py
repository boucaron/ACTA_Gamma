from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel, QComboBox, QPushButton

class SkillPanel(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Skill"))
        self.skill = QComboBox()
        self.skill.addItems(["review_pr/v1", "review_pr/v2", "extract_entities/v1"])
        layout.addWidget(self.skill)
        layout.addWidget(QPushButton("Load Skill"))
