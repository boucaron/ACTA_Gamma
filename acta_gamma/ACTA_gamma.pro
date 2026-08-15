QT += widgets
CONFIG += c++17 console
TARGET = ACTA_Gamma
TEMPLATE = app

RESOURCES += assets.qrc

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/widgets/skillpanel.cpp \
    src/widgets/modelpanel.cpp \
    src/widgets/contextpanel.cpp \
    src/widgets/executionpanel.cpp \
    src/widgets/skillDialog.cpp 

HEADERS += \
    src/mainwindow.h \
    src/widgets/skillpanel.h \
    src/widgets/modelpanel.h \
    src/widgets/contextpanel.h \
    src/widgets/executionpanel.h \
    src/widgets/skillDialog.h

FORMS += \
    ui/skillDialog.ui

DISTFILES +=

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

