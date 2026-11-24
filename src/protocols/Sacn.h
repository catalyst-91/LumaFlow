#pragma once
// ANSI E1.31-2018 (Streaming ACN / sACN) data packet encoding and decoding.
#include "Dmx.h"
#include <QHostAddress>
#include <QUuid>
#include <QString>

namespace lf::sacn {

constexpr quint16 kPort = 5568;
constexpr int kHeaderSize = 126;          // bytes before the DMX payload (incl. start code)
constexpr quint8 kDefaultPriority = 100;
constexpr quint8 kOptionPreview = 0x80;
constexpr quint8 kOptionStreamTerminated = 0x40;

struct DataPacket {
    QUuid cid;
    QString sourceName;
    quint8 priority = kDefaultPriority;
    quint16 syncAddress = 0;
    quint8 sequence = 0;
    quint8 options = 0;
    quint16 universe = 1;      // 1..63999
    quint8 startCode = 0;
    QByteArray data;           // up to 512 dmxSlots
};

QByteArray encode(const DataPacket& p);
bool decode(const QByteArray& pkt, DataPacket& out);
QHostAddress multicastAddress(quint16 universe);   // 239.255.{hi}.{lo}

} // namespace lf::sacn
