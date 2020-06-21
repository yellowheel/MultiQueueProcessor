TEMPLATE = app
QMAKE_CXXFLAGS         += -std=c++2a
CONFIG += console
CONFIG -= app_bundle


INCLUDEPATH += "C:\Program Files\boost"

SOURCES += \
        main.cpp

HEADERS += \
    MultiQueueProcessor.h \
    MultiQueueProcessor_test.h
