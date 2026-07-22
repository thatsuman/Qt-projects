QT += core testlib
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

win32: DEFINES += WIN32_LEAN_AND_MEAN
win32: INCLUDEPATH += $$PWD/../third_party/WinDivert/include
win32: LIBS += -liphlpapi -ladvapi32 -ltdh -lws2_32

TEMPLATE = app
TARGET = test_flowmanager

INCLUDEPATH += ..

SOURCES += \
    main.cpp \
    ../network/capture/windivertpacketcapture.cpp \
    ../network/dns/dnscache.cpp \
    ../network/dns/etwdnsmonitor.cpp \
    ../network/flow/flowmanager.cpp \
    ../network/process/iphelperconnectionpoller.cpp \
    ../network/process/processresolver.cpp \
    ../network/protocol/protocolinferencer.cpp \
    ../network/writer/networkjsonlwriter.cpp

HEADERS += \
    ../network/capture/windivertpacketcapture.h \
    ../network/dns/dnscache.h \
    ../network/dns/etwdnsmonitor.h \
    ../network/flow/flowmanager.h \
    ../network/model/flowkey.h \
    ../network/model/flowsession.h \
    ../network/model/networkevents.h \
    ../network/model/networksessionrecord.h \
    ../network/process/iphelperconnectionpoller.h \
    ../network/process/processresolver.h \
    ../network/protocol/protocolinferencer.h \
    ../network/writer/networkjsonlwriter.h
