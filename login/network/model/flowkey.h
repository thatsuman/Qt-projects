#ifndef NETWORK_MODEL_FLOWKEY_H
#define NETWORK_MODEL_FLOWKEY_H

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <array>

namespace Network {

enum class Direction {
    Inbound,
    Outbound,
    Unknown
};

enum class TransportProtocol {
    Tcp,
    Udp,
    Icmp,
    Other
};

inline QString transportToString(TransportProtocol transport)
{
    switch (transport) {
    case TransportProtocol::Tcp: return "TCP";
    case TransportProtocol::Udp: return "UDP";
    case TransportProtocol::Icmp: return "ICMP";
    case TransportProtocol::Other: return "Other";
    }
    return "Other";
}

class IpAddress
{
public:
    IpAddress() = default;

    static IpAddress fromV4(quint32 hostOrderAddress)
    {
        IpAddress ip;
        ip.m_ipv6 = false;
        ip.m_bytes[10] = 0xff;
        ip.m_bytes[11] = 0xff;
        ip.m_bytes[12] = static_cast<quint8>((hostOrderAddress >> 24) & 0xff);
        ip.m_bytes[13] = static_cast<quint8>((hostOrderAddress >> 16) & 0xff);
        ip.m_bytes[14] = static_cast<quint8>((hostOrderAddress >> 8) & 0xff);
        ip.m_bytes[15] = static_cast<quint8>(hostOrderAddress & 0xff);
        return ip;
    }

    static IpAddress fromV4Bytes(quint8 a, quint8 b, quint8 c, quint8 d)
    {
        return fromV4((static_cast<quint32>(a) << 24)
                    | (static_cast<quint32>(b) << 16)
                    | (static_cast<quint32>(c) << 8)
                    | static_cast<quint32>(d));
    }

    static IpAddress fromV6(const quint8 *bytes)
    {
        IpAddress ip;
        ip.m_ipv6 = true;
        for (int i = 0; i < 16; ++i) {
            ip.m_bytes[i] = bytes[i];
        }
        return ip;
    }

    bool isV6() const { return m_ipv6; }
    bool isNull() const { return m_bytes == std::array<quint8, 16>{}; }

    bool isAny() const
    {
        if (isNull()) return true;
        if (!m_ipv6) {
            return m_bytes[12] == 0 && m_bytes[13] == 0 && m_bytes[14] == 0 && m_bytes[15] == 0;
        }
        for (int i = 0; i < 16; ++i) {
            if (m_bytes[i] != 0) return false;
        }
        return true;
    }

    bool isPrivateOrLocal() const
    {
        if (isNull() || isAny() || isLoopback()) {
            return true;
        }

        if (!m_ipv6) {
            const quint8 a = m_bytes[12];
            const quint8 b = m_bytes[13];
            if (a == 10) return true;
            if (a == 172 && (b >= 16 && b <= 31)) return true;
            if (a == 192 && b == 168) return true;
            if (a == 169 && b == 254) return true;
            if (a >= 224 && a <= 239) return true;
            if (a >= 240) return true;
            return false;
        }

        if (m_bytes[0] == 0xff) return true;
        if (m_bytes[0] == 0xfe && (m_bytes[1] & 0xc0) == 0x80) return true;
        return false;
    }

    bool isMulticast() const
    {
        if (!m_ipv6) {
            const quint8 a = m_bytes[12];
            return a >= 224 && a <= 239;
        }
        return m_bytes[0] == 0xff;
    }

    bool isLoopback() const
    {
        if (!m_ipv6) {
            return m_bytes[12] == 127;
        }

        for (int i = 0; i < 15; ++i) {
            if (m_bytes[i] != 0) {
                return false;
            }
        }
        return m_bytes[15] == 1;
    }

    QString toString() const
    {
        if (!m_ipv6) {
            return QString("%1.%2.%3.%4")
                    .arg(m_bytes[12])
                    .arg(m_bytes[13])
                    .arg(m_bytes[14])
                    .arg(m_bytes[15]);
        }

        QStringList groups;
        groups.reserve(8);
        for (int i = 0; i < 16; i += 2) {
            const quint16 group = (static_cast<quint16>(m_bytes[i]) << 8)
                                | static_cast<quint16>(m_bytes[i + 1]);
            groups.append(QString::number(group, 16));
        }
        return groups.join(":");
    }

    const std::array<quint8, 16> &bytes() const { return m_bytes; }

    friend bool operator==(const IpAddress &lhs, const IpAddress &rhs)
    {
        return lhs.m_ipv6 == rhs.m_ipv6 && lhs.m_bytes == rhs.m_bytes;
    }

    friend bool operator!=(const IpAddress &lhs, const IpAddress &rhs)
    {
        return !(lhs == rhs);
    }

private:
    std::array<quint8, 16> m_bytes{};
    bool m_ipv6 = false;
};

inline uint qHash(const IpAddress &ip, uint seed = 0)
{
    uint h = seed ^ (ip.isV6() ? 0x9e3779b9U : 0x85ebca6bU);
    for (quint8 byte : ip.bytes()) {
        h = (h * 33U) ^ byte;
    }
    return h;
}

struct FlowKey
{
    IpAddress localIp;
    quint16 localPort = 0;
    IpAddress remoteIp;
    quint16 remotePort = 0;
    TransportProtocol transport = TransportProtocol::Other;
    int ipVersion = 4;

    bool operator==(const FlowKey &other) const
    {
        return localIp == other.localIp
            && localPort == other.localPort
            && remoteIp == other.remoteIp
            && remotePort == other.remotePort
            && transport == other.transport
            && ipVersion == other.ipVersion;
    }
};

inline uint qHash(const FlowKey &key, uint seed = 0)
{
    uint h = seed;
    h ^= qHash(key.localIp, h) + 0x9e3779b9U + (h << 6) + (h >> 2);
    h ^= qHash(key.remoteIp, h) + 0x9e3779b9U + (h << 6) + (h >> 2);
    h ^= ::qHash(key.localPort, h);
    h ^= ::qHash(key.remotePort, h);
    h ^= ::qHash(static_cast<int>(key.transport), h);
    h ^= ::qHash(key.ipVersion, h);
    return h;
}

} // namespace Network

Q_DECLARE_METATYPE(Network::FlowKey)
Q_DECLARE_METATYPE(Network::IpAddress)

#endif // NETWORK_MODEL_FLOWKEY_H
