QT += core testlib sql
CONFIG += c++17 console testcase
TARGET = test_contextdao
TEMPLATE = app

# Reuse the app headers / sources you need
INCLUDEPATH += $$PWD/../src

LIBS += -lsqlite3

SOURCES += \
    dao/testContextDao.cpp \
    $$PWD/../src/dao/baseDao.cpp \
    $$PWD/../src/dao/contextDao.cpp

HEADERS += \
    $$PWD/../src/dao/baseDao.h \
    $$PWD/../src/dao/contextDao.h

# Reuse the same resource file as the app
RESOURCES += $$PWD/../assets.qrc

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
