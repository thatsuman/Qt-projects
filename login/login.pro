QT += widgets

CONFIG += c++17
CONFIG -= embed_manifest_exe

# Link Windows libraries required by hooks and activity logger
win32: DEFINES += WIN32_LEAN_AND_MEAN
win32: LIBS += -luser32 -lpsapi -liphlpapi -ladvapi32 -ltdh -lws2_32
win32: RC_FILE = login.rc

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Allow #include "auth/authmanager.h" style includes from any source file
INCLUDEPATH += .
win32: INCLUDEPATH += $$PWD/third_party/WinDivert/include
win32: LIBS += -L$$PWD/third_party/WinDivert/x64 -lWinDivert

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    auth/authmanager.cpp \
    hooks/keyboardhook.cpp \
    hooks/mousehook.cpp \
    logger/activitylogger.cpp \
    network/capture/windivertflowcapture.cpp \
    network/capture/windivertpacketcapture.cpp \
    network/dns/dnscache.cpp \
    network/dns/etwdnsmonitor.cpp \
    network/flow/flowmanager.cpp \
    network/orchestrator/networkorchestrator.cpp \
    network/process/iphelperconnectionpoller.cpp \
    network/process/processresolver.cpp \
    network/protocol/protocolinferencer.cpp \
    network/writer/networkjsonlwriter.cpp \
    ui/loginuimanager.cpp

HEADERS += \
    mainwindow.h \
    auth/authmanager.h \
    hooks/keyboardhook.h \
    hooks/mousehook.h \
    logger/activitylogger.h \
    network/capture/windivertflowcapture.h \
    network/capture/windivertpacketcapture.h \
    network/dns/dnscache.h \
    network/dns/etwdnsmonitor.h \
    network/flow/flowmanager.h \
    network/model/flowkey.h \
    network/model/flowsession.h \
    network/model/networkevents.h \
    network/model/networksessionrecord.h \
    network/orchestrator/networkorchestrator.h \
    network/process/iphelperconnectionpoller.h \
    network/process/processresolver.h \
    network/protocol/protocolinferencer.h \
    network/writer/networkjsonlwriter.h \
    ui/loginuimanager.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
