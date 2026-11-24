#pragma once
// Art-Net 4 packet encoding/decoding (ArtDmx, ArtPoll, ArtPollReply, ArtSync).
// Implemented from the publicly available Art-Net 4 specification.
#include "Dmx.h"
#include <QHostAddress>
#include <QString>

namespace lf::artnet {

constexpr quint16 kPort = 6454;
constexpr quint16 kOpPoll = 0x2000;
constexpr quint16 kOpPollReply = 0x2100;
constexpr quint16 kOpDmx = 0x5000;
constexpr quint16 kOpSync = 0x5200;
constexpr quint16 kProtocolVersion = 14;

struct DmxPacket {
    quint16 portAddress = 0;   // 15-bit: net(7) subnet(4) universe(4)
    quint8 sequence = 0;
    quint8 physical = 0;
    QByteArray data;           // 2..512 bytes, even length
};

struct PollReply {
    QHostAddress ip;
    QString shortName;
    QString longName;
    quint16 firstPortAddress = 0;
    int numPorts = 0;
};

// Returns the opcode of a valid Art-Net packet or 0.
quint16 opcode(const QByteArray& pkt);
QByteArray encodeDmx(const DmxPacket& p);
bool decodeDmx(const QByteArray& pkt, DmxPacket& out);
QByteArray encodePoll(bool sendReplyOnChange = true);
QByteArray encodePollReply(const PollReply& r);
bool decodePollReply(const QByteArray& pkt, PollReply& out);
QByteArray encodeSync();

// LUMAFLOW universes are numbered from 0 (Art-Net "port address").
inline quint16 portAddress(int net, int subnet, int universe) {
    return quint16(((net & 0x7F) << 8) | ((subnet & 0x0F) << 4) | (universe & 0x0F));
}

} // namespace lf::artnet
