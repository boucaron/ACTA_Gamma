QT += widgets
CONFIG += c++17 console
TARGET = ACTA_Gamma
TEMPLATE = app

RESOURCES += assets.qrc

SOURCES += \
    src/main.cpp \
    src/mainWindow.cpp \
    src/widgets/skillPanel.cpp \
    src/widgets/modelPanel.cpp \
    src/widgets/contextPanel.cpp \
    src/widgets/executionPanel.cpp \
    src/widgets/skillDialog.cpp \
    src/widgets/modelDialog.cpp \
    src/widgets/executionDialog.cpp

HEADERS += \
    src/mainWindow.h \
    src/widgets/skillPanel.h \
    src/widgets/modelPanel.h \
    src/widgets/contextPanel.h \
    src/widgets/executionPanel.h \
    src/widgets/skillDialog.h \
    src/widgets/modelDialog.h \
    src/widgets/executionDialog.h

FORMS += \
    ui/skillDialog.ui \
    ui/modelDialog.ui \
    ui/executionDialog.ui

DISTFILES +=

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

