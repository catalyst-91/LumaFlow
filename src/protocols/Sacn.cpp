#include "Sacn.h"
#include <cstring>

namespace lf::sacn {

static const char kAcnId[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0};

static void put16(QByteArray& b, int off, quint16 v) { b[off] = char(v >> 8); b[off + 1] = char(v & 0xFF); }
static void put32(QByteArray& b, int off, quint32 v) { b[off] = char(v >> 24); b[off+1] = char(v >> 16); b[off+2] = char(v >> 8); b[off+3] = char(v); }
static quint16 get16(const QByteArray& b, int off) { return quint16((quint8(b[off]) << 8) | quint8(b[off + 1])); }
static quint32 get32(const QByteArray& b, int off) { return (quint32(quint8(b[off])) << 24) | (quint32(quint8(b[off+1])) << 16) | (quint32(quint8(b[off+2])) << 8) | quint8(b[off+3]); }

QByteArray encode(const DataPacket& p) {
    const QByteArray data = p.data.left(512);
    const int n = data.size();
    QByteArray b(kHeaderSize + n, 0);
    // Root layer
    put16(b, 0, 0x0010); put16(b, 2, 0x0000);
    std::memcpy(b.data() + 4, kAcnId, 12);
    put16(b, 16, quint16(0x7000 | (b.size() - 16)));
    put32(b, 18, 0x00000004);                                     // VECTOR_ROOT_E131_DATA
    const QByteArray cid = p.cid.toRfc4122();
    std::memcpy(b.data() + 22, cid.constData(), 16);
    // Framing layer
    put16(b, 38, quint16(0x7000 | (b.size() - 38)));
    put32(b, 40, 0x00000002);                                     // VECTOR_E131_DATA_PACKET
    const QByteArray name = p.sourceName.toUtf8().left(63);
    std::memcpy(b.data() + 44, name.constData(), name.size());
    b[108] = char(p.priority);
    put16(b, 109, p.syncAddress);
    b[111] = char(p.sequence);
    b[112] = char(p.options);
    put16(b, 113, p.universe);
    // DMP layer
    put16(b, 115, quint16(0x7000 | (b.size() - 115)));
    b[117] = char(0x02);                                          // VECTOR_DMP_SET_PROPERTY
    b[118] = char(0xA1);                                          // address & data type
    put16(b, 119, 0x0000);                                        // first property address
    put16(b, 121, 0x0001);                                        // address increment
    put16(b, 123, quint16(n + 1));                                // property value count
    b[125] = char(p.startCode);
    std::memcpy(b.data() + 126, data.constData(), n);
    return b;
}

bool decode(const QByteArray& b, DataPacket& out) {
    if (b.size() < kHeaderSize) return false;
    if (get16(b, 0) != 0x0010 || std::memcmp(b.constData() + 4, kAcnId, 12) != 0) return false;
    if (get32(b, 18) != 0x00000004 || get32(b, 40) != 0x00000002) return false;
    if (quint8(b[117]) != 0x02 || quint8(b[118]) != 0xA1) return false;
    const int count = get16(b, 123);
    if (count < 1 || count > 513 || b.size() < 125 + count) return false;
    out.cid = QUuid::fromRfc4122(b.mid(22, 16));
    out.sourceName = QString::fromUtf8(b.constData() + 44, int(strnlen(b.constData() + 44, 64)));
    out.priority = quint8(b[108]);
    out.syncAddress = get16(b, 109);
    out.sequence = quint8(b[111]);
    out.options = quint8(b[112]);
    out.universe = get16(b, 113);
    out.startCode = quint8(b[125]);
    out.data = b.mid(126, count - 1);
    return true;
}

QHostAddress multicastAddress(quint16 u) {
    return QHostAddress((239u << 24) | (255u << 16) | (quint32(u >> 8) << 8) | (u & 0xFF));
}

} // namespace lf::sacn
