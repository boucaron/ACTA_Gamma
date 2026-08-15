QT += core testlib sql
CONFIG += c++17 console testcase
TARGET = testExecutionDao
TEMPLATE = app

# Reuse the app headers / sources you need
INCLUDEPATH += $$PWD/../../src

LIBS += -lsqlite3

SOURCES += \
    ../dao/testExecutionDao.cpp \    
    $$PWD/../../src/dao/baseDao.cpp \
    $$PWD/../../src/dao/executionDao.cpp 
   
HEADERS += \
    $$PWD/../../src/dao/baseDao.h \
    $$PWD/../../src/dao/executionDao.h
    

# Reuse the same resource file as the app
RESOURCES += $$PWD/../../assets.qrc

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
