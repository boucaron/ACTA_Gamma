QT += widgets
CONFIG += c++17 console
TARGET = acta_gui
TEMPLATE = app

RESOURCES += ../assets.qrc

INCLUDEPATH += $$PWD/../../acta_db/include
# M1 / UR #45: in-process runner — the pipeline sources (run.c,
# backend.c, plus argparse.c for the cmd_args_* helpers used by
# cmd_run, deathmark.c for the death-marker claim/release helpers
# run_execution uses, and dialect.c for the backend dialect
# llamacpp/openai that run.c resolves from the model revision) are
# compiled straight into the app; the exit-code contract comes from
# runner.h.
INCLUDEPATH += $$PWD/../../acta_runner/include
# runner_util.h (in acta_runner/include/) includes "dialect.h", which lives in
# acta_runner/src/. A quoted include resolves relative to the INCLUDING file, so
# any consumer of runner_util.h needs src/ on the include path - exactly why
# acta_cli/Makefile passes -I../acta_runner/src. Without this line the GUI build
# stops in runnerWorker.cpp:
#   fatal error: dialect.h: No such file or directory
# (the runner's own Makefile already has -Isrc, so only the GUI was blind to it).
INCLUDEPATH += $$PWD/../../acta_runner/src

SOURCES += \
    main.cpp \
    dbhandle.cpp \
    runnerWorker.cpp \
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
    widgets/executionLogDialog.cpp \
    ../../acta_runner/src/run.c \
    ../../acta_runner/src/backend.c \
    ../../acta_runner/src/dialect.c \
    ../../acta_runner/src/argparse.c \
    ../../acta_runner/src/deathmark.c

HEADERS += \
    confreader.h \
    dbhandle.h \
    runnerWorker.h \
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
TRANSLATIONS += ../translations/acta_gui.ts

DISTFILES +=

# M1 / UR #45: run.c/backend.c need cJSON and curl (the same
# dependencies as the standalone acta_runner Makefile).
LIBS += $$PWD/../../acta_db/libacta_db.a -lsqlite3 -lcjson -lcurl
win32: LIBS += -lws2_32

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui


