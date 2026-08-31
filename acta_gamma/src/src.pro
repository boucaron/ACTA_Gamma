QT += widgets
CONFIG += c++17 console
TARGET = ACTA_Gamma
TEMPLATE = app

RESOURCES += ../assets.qrc

INCLUDEPATH += $$PWD/../../acta_db/include

SOURCES += \
    main.cpp \
    dbhandle.cpp \
    mainWindow.cpp \
    widgets/skillPanel.cpp \
    widgets/modelPanel.cpp \
    widgets/contextPanel.cpp \
    widgets/executionPanel.cpp \
    widgets/skillDialog.cpp \
    widgets/modelDialog.cpp \
    widgets/executionDialog.cpp \
    widgets/executionLogDialog.cpp \
    widgets/contextDialog.cpp

HEADERS += \
    dbhandle.h \
    mainWindow.h \
    widgets/skillPanel.h \
    widgets/modelPanel.h \
    widgets/contextPanel.h \
    widgets/executionPanel.h \
    widgets/skillDialog.h \
    widgets/modelDialog.h \
    widgets/executionDialog.h \
    widgets/executionLogDialog.h \
    widgets/contextDialog.h

FORMS += \
    ../ui/skillDialog.ui \
    ../ui/modelDialog.ui \
    ../ui/executionDialog.ui \
    ../ui/executionLogDialog.ui \
    ../ui/contextDialog.ui

DISTFILES +=

LIBS += $$PWD/../../acta_db/libacta_db.a -lsqlite3

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui


