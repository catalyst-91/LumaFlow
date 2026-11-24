#include "ArtNet.h"
#include <cstring>

namespace lf::artnet {

static const char kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};

quint16 opcode(const QByteArray& p) {
    if (p.size() < 10 || std::memcmp(p.constData(), kId, 8) != 0) return 0;
    return quint16(quint8(p[8]) | (quint8(p[9]) << 8));
}

static void header(QByteArray& b, quint16 op) {
    b.append(kId, 8);
    b.append(char(op & 0xFF)); b.append(char(op >> 8));         // opcode little endian
}

QByteArray encodeDmx(const DmxPacket& p) {
    QByteArray data = p.data.left(512);
    if (data.size() < 2) data.append(QByteArray(2 - data.size(), 0));
    if (data.size() % 2) data.append(char(0));
    QByteArray b; b.reserve(18 + data.size());
    header(b, kOpDmx);
    b.append(char(0)); b.append(char(kProtocolVersion));        // ProtVer big endian
    b.append(char(p.sequence)); b.append(char(p.physical));
    b.append(char(p.portAddress & 0xFF)); b.append(char((p.portAddress >> 8) & 0x7F)); // SubUni, Net
    b.append(char(data.size() >> 8)); b.append(char(data.size() & 0xFF));             // length BE
    b.append(data);
    return b;
}

bool decodeDmx(const QByteArray& p, DmxPacket& out) {
    if (opcode(p) != kOpDmx || p.size() < 20) return false;
    const int ver = (quint8(p[10]) << 8) | quint8(p[11]);
    if (ver < 14) return false;
    out.sequence = quint8(p[12]); out.physical = quint8(p[13]);
    out.portAddress = quint16(quint8(p[14]) | ((quint8(p[15]) & 0x7F) << 8));
    const int len = (quint8(p[16]) << 8) | quint8(p[17]);
    if (len < 2 || len > 512 || p.size() < 18 + len) return false;
    out.data = p.mid(18, len);
    return true;
}

QByteArray encodePoll(bool sendReplyOnChange) {
    QByteArray b; header(b, kOpPoll);
    b.append(char(0)); b.append(char(kProtocolVersion));
    b.append(char(sendReplyOnChange ? 0x02 : 0x00)); b.append(char(0));  // flags, diag priority
    return b;
}

QByteArray encodePollReply(const PollReply& r) {
    QByteArray b(239, 0);
    std::memcpy(b.data(), kId, 8);
    b[8] = char(kOpPollReply & 0xFF); b[9] = char(kOpPollReply >> 8);
    const quint32 ip = r.ip.toIPv4Address();
    b[10] = char(ip >> 24); b[11] = char(ip >> 16); b[12] = char(ip >> 8); b[13] = char(ip);
    b[14] = char(kPort & 0xFF); b[15] = char(kPort >> 8);
    b[18] = char((r.firstPortAddress >> 8) & 0x7F);  // NetSwitch
    b[19] = char((r.firstPortAddress >> 4) & 0x0F);  // SubSwitch
    const QByteArray sn = r.shortName.toLatin1().left(17), ln = r.longName.toLatin1().left(63);
    std::memcpy(b.data() + 26, sn.constData(), sn.size());
    std::memcpy(b.data() + 44, ln.constData(), ln.size());
    const int ports = std::min(4, std::max(0, r.numPorts));
    b[172] = 0; b[173] = char(ports);
    for (int i = 0; i < ports; ++i) {
        b[174 + i] = char(0x80 | 0x40);                 // can output & input DMX512
        b[190 + i] = char((r.firstPortAddress + i) & 0x0F); // SwOut
        b[186 + i] = char((r.firstPortAddress + i) & 0x0F); // SwIn
    }
    b[200] = char(0x05);                               // Style: StVisual (media server)
    return b;
}

bool decodePollReply(const QByteArray& p, PollReply& out) {
    if (opcode(p) != kOpPollReply || p.size() < 207) return false;
    out.ip = QHostAddress((quint32(quint8(p[10])) << 24) | (quint32(quint8(p[11])) << 16) | (quint32(quint8(p[12])) << 8) | quint8(p[13]));
    out.shortName = QString::fromLatin1(p.constData() + 26, int(strnlen(p.constData() + 26, 18)));
    out.longName = QString::fromLatin1(p.constData() + 44, int(strnlen(p.constData() + 44, 64)));
    out.numPorts = quint8(p[173]);
    out.firstPortAddress = quint16(((quint8(p[18]) & 0x7F) << 8) | ((quint8(p[19]) & 0x0F) << 4) | (quint8(p[190]) & 0x0F));
    return true;
}

QByteArray encodeSync() {
    QByteArray b; header(b, kOpSync);
    b.append(char(0)); b.append(char(kProtocolVersion)); b.append(char(0)); b.append(char(0));
    return b;
}

} // namespace lf::artnet
