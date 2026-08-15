QT += widgets
CONFIG += c++17 console
TARGET = ACTA_Gamma
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/widgets/skillpanel.cpp \
    src/widgets/modelpanel.cpp \
    src/widgets/contextpanel.cpp \
    src/widgets/executionpanel.cpp

HEADERS += \
    src/mainwindow.h \
    src/widgets/skillpanel.h \
    src/widgets/modelpanel.h \
    src/widgets/contextpanel.h \
    src/widgets/executionpanel.h

FORMS =

DISTFILES +=
