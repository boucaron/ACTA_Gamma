QT += widgets sql
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
    src/widgets/executionDialog.cpp \
    src/widgets/contextDialog.cpp \
    src/dao/baseDao.cpp \  # DAO
    src/dao/contextDao.cpp \
    src/dao/executionDao.cpp \
    src/dao/executionLogDao.cpp \
    src/dao/modelDao.cpp \
    src/dao/modelFolderDao.cpp \
    src/dao/modelRevisionDao.cpp \
    src/dao/skillDao.cpp \
    src/dao/skillFolderDao.cpp \
    src/dao/skillRevisionDao.cpp    

HEADERS += \
    src/mainWindow.h \
    src/widgets/skillPanel.h \
    src/widgets/modelPanel.h \
    src/widgets/contextPanel.h \
    src/widgets/executionPanel.h \
    src/widgets/skillDialog.h \
    src/widgets/modelDialog.h \
    src/widgets/executionDialog.h \
    src/widgets/contextDialog.h \ # DAO
    src/dao/baseDao.h \
    src/dao/contextDao.h \
    src/dao/executionDao.h \
    src/dao/executionLogDao.h \
    src/dao/modelDao.h \
    src/dao/modelFolderDao.h \
    src/dao/modelRevisionDao.h \
    src/dao/skillDao.h \
    src/dao/skillFolderDao.h \
    src/dao/skillRevisionDao.h

FORMS += \
    ui/skillDialog.ui \
    ui/modelDialog.ui \
    ui/executionDialog.ui \
    ui/contextDialog.ui

DISTFILES +=

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

