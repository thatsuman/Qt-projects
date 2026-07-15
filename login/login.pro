QT += widgets

CONFIG += c++17

# Link Windows libraries required by hooks and activity logger
win32: LIBS += -luser32 -lpsapi

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
    ui/loginuimanager.cpp

HEADERS += \
    mainwindow.h \
    auth/authmanager.h \
    hooks/keyboardhook.h \
    hooks/mousehook.h \
    logger/activitylogger.h \
    ui/loginuimanager.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
