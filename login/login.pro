QT += widgets network

CONFIG += c++17

# Link Windows libraries required by hooks, activity logger, and network logger
win32: DEFINES += WIN32_LEAN_AND_MEAN
win32: LIBS += -luser32 -lpsapi -liphlpapi -ltdh -lws2_32 -ladvapi32 -lole32

# Force UAC elevation prompt (requireAdministrator) for ETW real-time tracing
win32-msvc* {
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Allow #include "auth/authmanager.h" style includes from any source file
INCLUDEPATH += .

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    auth/authmanager.cpp \
    hooks/keyboardhook.cpp \
    hooks/mousehook.cpp \
    logger/activitylogger.cpp \
    logger/networklogger.cpp \
    ui/loginuimanager.cpp \
    etw/EtwTraceSession.cpp \
    network/ProtocolClassifier.cpp

HEADERS += \
    mainwindow.h \
    auth/authmanager.h \
    hooks/keyboardhook.h \
    hooks/mousehook.h \
    logger/activitylogger.h \
    logger/networklogger.h \
    ui/loginuimanager.h \
    etw/etw_providers.h \
    etw/EtwTraceSession.h \
    etw/etw_network_taxonomy.h \
    network/ProtocolClassifier.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
