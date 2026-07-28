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
win32 {
    CONFIG(debug, debug|release): WINDIVERT_DEPLOY_DIR = $$OUT_PWD/debug
    else: WINDIVERT_DEPLOY_DIR = $$OUT_PWD/release

    QMAKE_POST_LINK += $$quote(cmd /c if not exist "$$shell_path($$WINDIVERT_DEPLOY_DIR)" mkdir "$$shell_path($$WINDIVERT_DEPLOY_DIR)" && copy /Y "$$shell_path($$PWD/third_party/WinDivert/x64/WinDivert.dll)" "$$shell_path($$WINDIVERT_DEPLOY_DIR/WinDivert.dll)" && copy /Y "$$shell_path($$PWD/third_party/WinDivert/x64/WinDivert64.sys)" "$$shell_path($$WINDIVERT_DEPLOY_DIR/WinDivert64.sys)")
}

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
    network/merge/consecutiverecordmerger.cpp \
    network/orchestrator/networkorchestrator.cpp \
    network/process/iphelperconnectionpoller.cpp \
    network/process/processresolver.cpp \
    network/protocol/protocolinferencer.cpp \
    network/writer/networkjsonlwriter.cpp \
    ui/loginuimanager.cpp \
    ui/dashboardcontroller.cpp \
    analytics/reader/activitylogreader.cpp \
    analytics/reader/networklogreader.cpp \
    analytics/reader/networkerrorreader.cpp \
    analytics/aggregator/activityaggregator.cpp \
    analytics/aggregator/networkaggregator.cpp \
    analytics/aggregator/appcorrelator.cpp \
    analytics/aggregator/timelineaggregator.cpp \
    analytics/dashboard/dashboarddatamodel.cpp \
    analytics/dashboard/htmldashboardgenerator.cpp

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
    network/merge/consecutiverecordmerger.h \
    network/model/flowkey.h \
    network/model/flowsession.h \
    network/model/networkevents.h \
    network/model/networksessionrecord.h \
    network/orchestrator/networkorchestrator.h \
    network/process/iphelperconnectionpoller.h \
    network/process/processresolver.h \
    network/protocol/protocolinferencer.h \
    network/writer/networkjsonlwriter.h \
    ui/loginuimanager.h \
    ui/dashboardcontroller.h \
    analytics/model/analyticsmodels.h \
    analytics/model/activitysummary.h \
    analytics/model/networksummary.h \
    analytics/model/appsummary.h \
    analytics/model/timelinebucket.h \
    analytics/reader/ianalyticreaders.h \
    analytics/reader/activitylogreader.h \
    analytics/reader/networklogreader.h \
    analytics/reader/networkerrorreader.h \
    analytics/aggregator/activityaggregator.h \
    analytics/aggregator/networkaggregator.h \
    analytics/aggregator/appcorrelator.h \
    analytics/aggregator/timelineaggregator.h \
    analytics/dashboard/dashboarddatamodel.h \
    analytics/dashboard/dashboardtemplate.h \
    analytics/dashboard/htmldashboardgenerator.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
