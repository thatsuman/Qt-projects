#include "network/merge/consecutiverecordmerger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QList>

#include <iostream>
#include <cstdlib>

using namespace Network;

static void assertTest(bool condition, const char *name)
{
    if (condition) {
        std::cout << "[PASS] " << name << std::endl;
    } else {
        std::cerr << "[FAIL] " << name << std::endl;
        std::exit(1);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qRegisterMetaType<NetworkSessionRecord>("Network::NetworkSessionRecord");
    qRegisterMetaType<NetworkSessionRecord>("NetworkSessionRecord");

    // Test 1: Consecutive Matching Merge
    {
        ConsecutiveRecordMerger merger(300);
        QList<NetworkSessionRecord> logged;

        QObject::connect(&merger, &ConsecutiveRecordMerger::recordReadyForLogging,
                         [&logged](const NetworkSessionRecord &rec) { logged.append(rec); });

        QDateTime t1 = QDateTime::currentDateTimeUtc();

        NetworkSessionRecord r1;
        r1.process.pid = 100;
        r1.process.name = "firefox.exe";
        r1.key.remoteIp = IpAddress::fromV4Bytes(140, 82, 112, 21);
        r1.key.remotePort = 443;
        r1.key.transport = TransportProtocol::Tcp;
        r1.remoteHost.primaryName = "api.github.com";
        r1.startTimeUtc = t1;
        r1.endTimeUtc = t1.addSecs(5);
        r1.bytesSentTotal = 1000;
        r1.bytesReceivedTotal = 5000;

        NetworkSessionRecord r2 = r1;
        r2.startTimeUtc = t1.addSecs(10);
        r2.endTimeUtc = t1.addSecs(15);
        r2.bytesSentTotal = 2000;
        r2.bytesReceivedTotal = 8000;

        NetworkSessionRecord r3 = r1;
        r3.startTimeUtc = t1.addSecs(20);
        r3.endTimeUtc = t1.addSecs(25);
        r3.bytesSentTotal = 3000;
        r3.bytesReceivedTotal = 12000;

        merger.processSessionRecord(r1);
        merger.processSessionRecord(r2);
        merger.processSessionRecord(r3);
        merger.flush();

        assertTest(logged.size() == 1, "Consecutive match count == 1");
        assertTest(logged[0].mergedRecordCount == 3, "Merged record count == 3");
        assertTest(logged[0].isMergedConsecutiveRun == true, "isMergedConsecutiveRun == true");
        assertTest(logged[0].bytesSentTotal == 6000, "bytesSentTotal == 6000");
        assertTest(logged[0].bytesReceivedTotal == 25000, "bytesReceivedTotal == 25000");
        assertTest(logged[0].startTimeUtc == t1, "startTimeUtc matches first");
        assertTest(logged[0].endTimeUtc == t1.addSecs(25), "endTimeUtc matches last");
    }

    // Test 2: CDN IP Rotation Merge
    {
        ConsecutiveRecordMerger merger(300);
        QList<NetworkSessionRecord> logged;

        QObject::connect(&merger, &ConsecutiveRecordMerger::recordReadyForLogging,
                         [&logged](const NetworkSessionRecord &rec) { logged.append(rec); });

        QDateTime t1 = QDateTime::currentDateTimeUtc();

        NetworkSessionRecord r1;
        r1.process.name = "chrome.exe";
        r1.remoteHost.primaryName = "video.netflix.com";
        r1.key.remoteIp = IpAddress::fromV4Bytes(45, 57, 84, 136);
        r1.key.remotePort = 443;
        r1.key.transport = TransportProtocol::Tcp;
        r1.startTimeUtc = t1;
        r1.endTimeUtc = t1.addSecs(2);

        NetworkSessionRecord r2 = r1;
        r2.key.remoteIp = IpAddress::fromV4Bytes(45, 57, 84, 137); // Dynamic CDN IP rotation
        r2.startTimeUtc = t1.addSecs(3);
        r2.endTimeUtc = t1.addSecs(5);

        merger.processSessionRecord(r1);
        merger.processSessionRecord(r2);
        merger.flush();

        assertTest(logged.size() == 1, "CDN IP rotation merge count == 1");
        assertTest(logged[0].mergedRecordCount == 2, "CDN merged count == 2");
        assertTest(logged[0].remoteHost.primaryName == "video.netflix.com", "CDN domain preserved");
    }

    // Test 3: Non-Consecutive Not Merged
    {
        ConsecutiveRecordMerger merger(300);
        QList<NetworkSessionRecord> logged;

        QObject::connect(&merger, &ConsecutiveRecordMerger::recordReadyForLogging,
                         [&logged](const NetworkSessionRecord &rec) { logged.append(rec); });

        QDateTime t1 = QDateTime::currentDateTimeUtc();

        NetworkSessionRecord rA;
        rA.process.name = "firefox.exe";
        rA.remoteHost.primaryName = "google.com";
        rA.key.remotePort = 443;
        rA.key.transport = TransportProtocol::Tcp;
        rA.startTimeUtc = t1;
        rA.endTimeUtc = t1.addSecs(2);

        NetworkSessionRecord rB = rA;
        rB.remoteHost.primaryName = "bing.com";
        rB.startTimeUtc = t1.addSecs(3);
        rB.endTimeUtc = t1.addSecs(5);

        NetworkSessionRecord rA2 = rA;
        rA2.startTimeUtc = t1.addSecs(6);
        rA2.endTimeUtc = t1.addSecs(8);

        merger.processSessionRecord(rA);
        merger.processSessionRecord(rB);
        merger.processSessionRecord(rA2);
        merger.flush();

        assertTest(logged.size() == 3, "Non-consecutive emits 3 records");
        assertTest(logged[0].remoteHost.primaryName == "google.com", "Record 1 == google.com");
        assertTest(logged[1].remoteHost.primaryName == "bing.com", "Record 2 == bing.com");
        assertTest(logged[2].remoteHost.primaryName == "google.com", "Record 3 == google.com");
    }

    // Test 4: Idle Gap Splits
    {
        ConsecutiveRecordMerger merger(300); // 5 minute max gap
        QList<NetworkSessionRecord> logged;

        QObject::connect(&merger, &ConsecutiveRecordMerger::recordReadyForLogging,
                         [&logged](const NetworkSessionRecord &rec) { logged.append(rec); });

        QDateTime t1 = QDateTime::currentDateTimeUtc();

        NetworkSessionRecord r1;
        r1.process.name = "firefox.exe";
        r1.remoteHost.primaryName = "github.com";
        r1.key.remotePort = 443;
        r1.key.transport = TransportProtocol::Tcp;
        r1.startTimeUtc = t1;
        r1.endTimeUtc = t1.addSecs(5);

        NetworkSessionRecord r2 = r1;
        r2.startTimeUtc = t1.addSecs(400); // 400 seconds gap (> 300s limit)
        r2.endTimeUtc = t1.addSecs(410);

        merger.processSessionRecord(r1);
        merger.processSessionRecord(r2);
        merger.flush();

        assertTest(logged.size() == 2, "Idle gap > 300s splits into 2 records");
        assertTest(logged[0].mergedRecordCount == 1, "First session merged count == 1");
        assertTest(logged[1].mergedRecordCount == 1, "Second session merged count == 1");
    }

    // Test 5: Shutdown Flush
    {
        ConsecutiveRecordMerger merger(300);
        QList<NetworkSessionRecord> logged;

        QObject::connect(&merger, &ConsecutiveRecordMerger::recordReadyForLogging,
                         [&logged](const NetworkSessionRecord &rec) { logged.append(rec); });

        NetworkSessionRecord r;
        r.process.name = "slack.exe";
        r.remoteHost.primaryName = "slack.com";
        r.key.remotePort = 443;
        r.key.transport = TransportProtocol::Tcp;

        merger.processSessionRecord(r);
        assertTest(logged.size() == 0, "Record buffered before flush");

        merger.flush();
        assertTest(logged.size() == 1, "Record emitted after flush");
        assertTest(logged[0].process.name == "slack.exe", "Flushed process name == slack.exe");
    }

    std::cout << "All ConsecutiveRecordMerger unit tests PASSED successfully!" << std::endl;
    return 0;
}
