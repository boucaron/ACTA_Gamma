QT += core testlib sql
CONFIG += c++17 console testcase
TARGET = test_contextdao
TEMPLATE = app

# Reuse the app headers / sources you need
INCLUDEPATH += $$PWD/../src

LIBS += -lsqlite3

SOURCES += \
    dao/testContextDao.cpp \
    $$PWD/../dao/baseDao.cpp \
    $$PWD/../dao/contextDao.cpp

HEADERS += \
    $$PWD/../dao/baseDao.h \
    $$PWD/../dao/contextDao.h

# Reuse the same resource file as the app
RESOURCES += $$PWD/../../assets.qrc

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
