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
    widgets/folderTreePanel.cpp \
    widgets/entityDialog.cpp \
    widgets/skillPanel.cpp \
    widgets/modelPanel.cpp \
    widgets/contextPanel.cpp \
    widgets/executionPanel.cpp \
    widgets/executionCreateDialog.cpp \
    widgets/skillDialog.cpp \
    widgets/modelDialog.cpp \
    widgets/executionDialog.cpp \
    widgets/contextDialog.cpp \
    widgets/executionLogDialog.cpp

HEADERS += \
    dbhandle.h \
    mainWindow.h \
    widgets/folderTreePanel.h \
    widgets/entityDialog.h \
    widgets/util.h \
    widgets/skillPanel.h \
    widgets/modelPanel.h \
    widgets/contextPanel.h \
    widgets/executionPanel.h \
    widgets/executionCreateDialog.h \
    widgets/skillDialog.h \
    widgets/modelDialog.h \
    widgets/executionDialog.h \
    widgets/contextDialog.h \
    widgets/executionLogDialog.h

FORMS += \
    ../ui/skillDialog.ui \
    ../ui/modelDialog.ui \
    ../ui/executionDialog.ui \
    ../ui/contextDialog.ui \
    ../ui/executionCreateDialog.ui \
    ../ui/executionLogDialog.ui

# UR #43: translation template (i18n). Regenerate after any string
# change with `lupdate`; compiled .qm files (when added) are loaded
# by main.cpp from the application directory.
TRANSLATIONS += ../translations/acta_gamma.ts

DISTFILES +=

LIBS += $$PWD/../../acta_db/libacta_db.a -lsqlite3

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui


