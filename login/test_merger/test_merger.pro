QT += core network testlib
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = test_merger

INCLUDEPATH += ..

SOURCES += \
    main.cpp \
    ../network/merge/consecutiverecordmerger.cpp

HEADERS += \
    ../network/merge/consecutiverecordmerger.h \
    ../network/model/flowkey.h \
    ../network/model/flowsession.h \
    ../network/model/networksessionrecord.h
