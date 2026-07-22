#include "network/capture/windivertpacketcapture.h"
#include "network/dns/etwdnsmonitor.h"
#include "network/flow/flowmanager.h"
#include "network/process/iphelperconnectionpoller.h"
#include "network/writer/networkjsonlwriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace Network;

class FlowManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<NetworkSessionRecord>("Network::NetworkSessionRecord");
        qRegisterMetaType<NetworkSessionRecord>("NetworkSessionRecord");
        qRegisterMetaType<DnsObservation>("DnsObservation");
        qRegisterMetaType<ProcessConnectionSnapshot>("ProcessConnectionSnapshot");
    }

    void tcpFlowFlushProducesSessionRecord()
    {
        FlowManager manager;
        QSignalSpy spy(&manager, &FlowManager::sessionClosed);
        QVERIFY(spy.isValid());

        manager.start("tester");

        PacketObservation outbound;
        outbound.timestampUtc = QDateTime::currentDateTimeUtc();
        outbound.direction = Direction::Outbound;
        outbound.transport = TransportProtocol::Tcp;
        outbound.srcIp = IpAddress::fromV4Bytes(192, 168, 1, 25);
        outbound.srcPort = 53000;
        outbound.dstIp = IpAddress::fromV4Bytes(142, 250, 183, 14);
        outbound.dstPort = 443;
        outbound.packetBytes = 100;
        outbound.payloadBytes = 60;
        outbound.tcpFlags.syn = true;

        PacketObservation inbound = outbound;
        inbound.direction = Direction::Inbound;
        inbound.srcIp = outbound.dstIp;
        inbound.srcPort = outbound.dstPort;
        inbound.dstIp = outbound.srcIp;
        inbound.dstPort = outbound.srcPort;
        inbound.packetBytes = 200;
        inbound.payloadBytes = 150;

        manager.handlePacket(outbound);
        manager.handlePacket(inbound);
        manager.flushAll("app_shutdown");

        QCOMPARE(spy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(spy.takeFirst().at(0));
        const QJsonObject json = record.toJson();

        QCOMPARE(json.value("type").toString(), QString("network_session"));
        QCOMPARE(json.value("username").toString(), QString("tester"));
        QCOMPARE(json.value("transport_protocol").toString(), QString("TCP"));
        QCOMPARE(json.value("app_protocol_hint").toString(), QString("HTTPS"));
        QCOMPARE(json.value("close_reason").toString(), QString("app_shutdown"));
        QCOMPARE(json.value("local").toObject().value("port").toInt(), 53000);
        QCOMPARE(json.value("remote").toObject().value("port").toInt(), 443);
    }

    void writerCreatesJsonlFile()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString originalPath = QDir::currentPath();
        QVERIFY(QDir::setCurrent(tempDir.path()));

        FlowManager manager;
        NetworkJsonlWriter writer;
        connect(&manager, &FlowManager::sessionClosed,
                &writer, &NetworkJsonlWriter::writeSession);

        writer.start("tester");
        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Udp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 54000;
        packet.dstIp = IpAddress::fromV4Bytes(1, 1, 1, 1);
        packet.dstPort = 53;
        packet.packetBytes = 70;
        packet.payloadBytes = 42;

        manager.handlePacket(packet);
        manager.flushAll("app_shutdown");
        writer.stop();

        QFile file("logs/tester/network_log.jsonl");
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray contents = file.readAll();
        QVERIFY(contents.contains("\"type\":\"network_session\""));
        QVERIFY(contents.contains("\"app_protocol_hint\":\"DNS\""));

        QVERIFY(QDir::setCurrent(originalPath));
    }

    void processSnapshotEnrichesSession()
    {
        FlowManager manager;
        QSignalSpy spy(&manager, &FlowManager::sessionClosed);
        QVERIFY(spy.isValid());

        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Tcp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 55000;
        packet.dstIp = IpAddress::fromV4Bytes(93, 184, 216, 34);
        packet.dstPort = 80;
        packet.packetBytes = 64;

        manager.handlePacket(packet);

        ProcessConnectionSnapshot snapshot;
        snapshot.timestampUtc = QDateTime::currentDateTimeUtc();
        snapshot.pid = 1234;
        snapshot.processName = "browser.exe";
        snapshot.processPath = "C:/Program Files/Browser/browser.exe";
        snapshot.key = normalizePacketKey(packet);
        manager.handleProcessSnapshot(snapshot);

        manager.flushAll("app_shutdown");

        QCOMPARE(spy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(spy.takeFirst().at(0));
        const QJsonObject process = record.toJson().value("process").toObject();
        QCOMPARE(process.value("pid").toInt(), 1234);
        QCOMPARE(process.value("name").toString(), QString("browser.exe"));
        QCOMPARE(process.value("source").toString(), QString("iphelper"));
        QCOMPARE(process.value("confidence").toString(), QString("medium"));
    }

    void dnsObservationEnrichesHostname()
    {
        FlowManager manager;
        QSignalSpy spy(&manager, &FlowManager::sessionClosed);
        QVERIFY(spy.isValid());

        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Tcp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 56000;
        packet.dstIp = IpAddress::fromV4Bytes(93, 184, 216, 34);
        packet.dstPort = 443;
        packet.packetBytes = 64;

        DnsObservation dns;
        dns.timestampUtc = packet.timestampUtc.addSecs(-1);
        dns.queryName = "example.com";
        dns.answerIps.append(packet.dstIp);
        dns.ttlSeconds = 300;
        dns.pid = 4321;
        manager.handleDnsObservation(dns);

        manager.handlePacket(packet);
        manager.flushAll("app_shutdown");

        QCOMPARE(spy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(spy.takeFirst().at(0));
        const QJsonObject remote = record.toJson().value("remote").toObject();
        QCOMPARE(remote.value("hostname").toString(), QString("example.com"));
        QCOMPARE(remote.value("hostname_confidence").toString(), QString("medium"));
    }

    void missingWinDivertDegradesWithoutCrash()
    {
        qputenv("LOGIN_FORCE_WINDIVERT_MISSING", "1");

        WinDivertPacketCapture capture;
        QSignalSpy errorSpy(&capture, &WinDivertPacketCapture::errorOccurred);
        QSignalSpy statusSpy(&capture, &WinDivertPacketCapture::statusChanged);
        QVERIFY(errorSpy.isValid());
        QVERIFY(statusSpy.isValid());

        capture.start();

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.takeFirst().at(0).toString().contains("WinDivert"));
        QCOMPARE(statusSpy.takeFirst().at(0).toString(), QString("Network packet capture degraded"));

        qunsetenv("LOGIN_FORCE_WINDIVERT_MISSING");
    }

    void forcedEtwFailureDegradesWithoutHostname()
    {
        qputenv("LOGIN_FORCE_ETW_FAIL", "1");

        EtwDnsMonitor monitor;
        QSignalSpy errorSpy(&monitor, &EtwDnsMonitor::errorOccurred);
        QSignalSpy dnsSpy(&monitor, &EtwDnsMonitor::dnsObserved);
        QVERIFY(errorSpy.isValid());
        QVERIFY(dnsSpy.isValid());

        monitor.start();

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(dnsSpy.count(), 0);

        FlowManager manager;
        QSignalSpy sessionSpy(&manager, &FlowManager::sessionClosed);
        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Tcp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 57000;
        packet.dstIp = IpAddress::fromV4Bytes(203, 0, 113, 10);
        packet.dstPort = 443;
        packet.packetBytes = 64;
        manager.handlePacket(packet);
        manager.flushAll("app_shutdown");

        QCOMPARE(sessionSpy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(sessionSpy.takeFirst().at(0));
        QVERIFY(record.remoteHost.primaryName.isEmpty());
        QCOMPARE(record.remoteHost.confidence, QString("none"));

        qunsetenv("LOGIN_FORCE_ETW_FAIL");
    }

    void forcedIpHelperFailureKeepsProcessUnknown()
    {
        qputenv("LOGIN_FORCE_IPHELPER_FAIL", "1");

        IpHelperConnectionPoller poller;
        QSignalSpy errorSpy(&poller, &IpHelperConnectionPoller::errorOccurred);
        QSignalSpy snapshotSpy(&poller, &IpHelperConnectionPoller::processSnapshotObserved);
        QVERIFY(errorSpy.isValid());
        QVERIFY(snapshotSpy.isValid());

        poller.start();

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(snapshotSpy.count(), 0);

        FlowManager manager;
        QSignalSpy sessionSpy(&manager, &FlowManager::sessionClosed);
        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Tcp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 58000;
        packet.dstIp = IpAddress::fromV4Bytes(203, 0, 113, 20);
        packet.dstPort = 443;
        packet.packetBytes = 64;
        manager.handlePacket(packet);
        manager.flushAll("app_shutdown");

        QCOMPARE(sessionSpy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(sessionSpy.takeFirst().at(0));
        QCOMPARE(record.process.name, QString("unknown"));
        QCOMPARE(record.process.confidence, QString("none"));

        qunsetenv("LOGIN_FORCE_IPHELPER_FAIL");
    }

    void highVolumeTrafficDoesNotLoseSessions()
    {
        FlowManager manager;
        QSignalSpy sessionSpy(&manager, &FlowManager::sessionClosed);
        QVERIFY(sessionSpy.isValid());
        manager.start("tester");

        QElapsedTimer timer;
        timer.start();
        const int flowCount = 2000;
        for (int i = 0; i < flowCount; ++i) {
            PacketObservation packet;
            packet.timestampUtc = QDateTime::currentDateTimeUtc();
            packet.direction = Direction::Outbound;
            packet.transport = TransportProtocol::Tcp;
            packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
            packet.srcPort = static_cast<quint16>(20000 + i);
            packet.dstIp = IpAddress::fromV4Bytes(203, 0, static_cast<quint8>(i / 255), static_cast<quint8>(i % 255));
            packet.dstPort = 443;
            packet.packetBytes = 100;
            packet.payloadBytes = 60;
            manager.handlePacket(packet);
        }

        manager.flushAll("app_shutdown");
        QCOMPARE(sessionSpy.count(), flowCount);
        QVERIFY2(timer.elapsed() < 5000, "High-volume synthetic flow handling was too slow.");
    }

    void pidReuseSnapshotIsRejectedWhenProcessIsNewerThanFlow()
    {
        FlowManager manager;
        QSignalSpy sessionSpy(&manager, &FlowManager::sessionClosed);
        QSignalSpy errorSpy(&manager, &FlowManager::errorOccurred);
        QVERIFY(sessionSpy.isValid());
        QVERIFY(errorSpy.isValid());
        manager.start("tester");

        PacketObservation packet;
        packet.timestampUtc = QDateTime::currentDateTimeUtc();
        packet.direction = Direction::Outbound;
        packet.transport = TransportProtocol::Tcp;
        packet.srcIp = IpAddress::fromV4Bytes(10, 0, 0, 2);
        packet.srcPort = 59000;
        packet.dstIp = IpAddress::fromV4Bytes(203, 0, 113, 30);
        packet.dstPort = 443;
        packet.packetBytes = 64;
        manager.handlePacket(packet);

        ProcessConnectionSnapshot reusedPid;
        reusedPid.timestampUtc = packet.timestampUtc.addSecs(5);
        reusedPid.pid = 9999;
        reusedPid.processName = "new-process.exe";
        reusedPid.processPath = "C:/Temp/new-process.exe";
        reusedPid.processCreationTimeUtc = packet.timestampUtc.addSecs(10);
        reusedPid.key = normalizePacketKey(packet);
        manager.handleProcessSnapshot(reusedPid);
        manager.flushAll("app_shutdown");

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(sessionSpy.count(), 1);
        const NetworkSessionRecord record = qvariant_cast<NetworkSessionRecord>(sessionSpy.takeFirst().at(0));
        QCOMPARE(record.process.name, QString("unknown"));
        QCOMPARE(record.process.confidence, QString("none"));
    }
};

QTEST_GUILESS_MAIN(FlowManagerTest)
#include "main.moc"
