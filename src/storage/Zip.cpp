#include "Zip.h"
#include <QFile>
#include <QSaveFile>
#include <QVector>
#include <zlib.h>

namespace lf {

static quint16 rd16(const QByteArray& d, qsizetype o) { return quint16(quint8(d[o]) | (quint8(d[o + 1]) << 8)); }
static quint32 rd32(const QByteArray& d, qsizetype o) { return quint32(rd16(d, o)) | (quint32(rd16(d, o + 2)) << 16); }
static void wr16(QByteArray& d, quint16 v) { d.append(char(v & 0xFF)); d.append(char(v >> 8)); }
static void wr32(QByteArray& d, quint32 v) { wr16(d, quint16(v & 0xFFFF)); wr16(d, quint16(v >> 16)); }

bool ZipReader::open(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { m_error = "cannot open " + path; return false; }
    m_data = f.readAll();
    return parse();
}
bool ZipReader::openData(const QByteArray& d) { m_data = d; return parse(); }

bool ZipReader::parse() {
    m_entries.clear();
    const qsizetype n = m_data.size();
    qsizetype eocd = -1;
    for (qsizetype i = n - 22; i >= 0 && i >= n - 65557; --i)
        if (rd32(m_data, i) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { m_error = "not a zip archive (no end of central directory)"; return false; }
    const int count = rd16(m_data, eocd + 10);
    qsizetype p = rd32(m_data, eocd + 16);
    for (int i = 0; i < count; ++i) {
        if (p + 46 > n || rd32(m_data, p) != 0x02014b50) { m_error = "corrupt central directory"; return false; }
        Entry e;
        e.method = rd16(m_data, p + 10); e.crc = rd32(m_data, p + 16);
        e.compSize = rd32(m_data, p + 20); e.size = rd32(m_data, p + 24);
        const int nameLen = rd16(m_data, p + 28), extraLen = rd16(m_data, p + 30), commentLen = rd16(m_data, p + 32);
        e.localOffset = rd32(m_data, p + 42);
        const QString name = QString::fromUtf8(m_data.constData() + p + 46, nameLen);
        if (!name.endsWith('/')) m_entries.insert(name, e);
        p += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

QByteArray ZipReader::read(const QString& name, bool* ok) const {
    if (ok) *ok = false;
    auto it = m_entries.find(name);
    if (it == m_entries.end()) return {};
    const Entry& e = *it;
    const qsizetype lh = e.localOffset;
    if (lh + 30 > m_data.size() || rd32(m_data, lh) != 0x04034b50) return {};
    const qsizetype dataStart = lh + 30 + rd16(m_data, lh + 26) + rd16(m_data, lh + 28);
    if (dataStart + e.compSize > m_data.size()) return {};
    QByteArray out;
    if (e.method == 0) out = m_data.mid(dataStart, e.compSize);
    else if (e.method == 8) {
        out.resize(e.size);
        z_stream zs{}; inflateInit2(&zs, -MAX_WBITS);
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(m_data.constData() + dataStart)); zs.avail_in = e.compSize;
        zs.next_out = reinterpret_cast<Bytef*>(out.data()); zs.avail_out = e.size;
        const int r = inflate(&zs, Z_FINISH); inflateEnd(&zs);
        if (r != Z_STREAM_END) return {};
    } else return {};
    if (crc32(0, reinterpret_cast<const Bytef*>(out.constData()), uInt(out.size())) != e.crc) return {};
    if (ok) *ok = true;
    return out;
}

void ZipWriter::add(const QString& name, const QByteArray& data, bool compress) {
    Entry e; e.name = name; e.offset = quint32(m_out.size()); e.size = quint32(data.size());
    e.crc = quint32(crc32(0, reinterpret_cast<const Bytef*>(data.constData()), uInt(data.size())));
    QByteArray payload = data; e.method = 0;
    if (compress && data.size() > 64) {
        QByteArray buf(int(compressBound(uLong(data.size()))) + 64, 0);
        z_stream zs{}; deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData())); zs.avail_in = uInt(data.size());
        zs.next_out = reinterpret_cast<Bytef*>(buf.data()); zs.avail_out = uInt(buf.size());
        if (deflate(&zs, Z_FINISH) == Z_STREAM_END && zs.total_out < uLong(data.size())) { buf.resize(int(zs.total_out)); payload = buf; e.method = 8; }
        deflateEnd(&zs);
    }
    e.compSize = quint32(payload.size());
    const QByteArray nm = name.toUtf8();
    wr32(m_out, 0x04034b50); wr16(m_out, 20); wr16(m_out, 0x0800); wr16(m_out, e.method); wr16(m_out, 0); wr16(m_out, 0x21);
    wr32(m_out, e.crc); wr32(m_out, e.compSize); wr32(m_out, e.size); wr16(m_out, quint16(nm.size())); wr16(m_out, 0);
    m_out.append(nm); m_out.append(payload);
    m_entries << e;
}

QByteArray ZipWriter::finish() {
    const quint32 cdStart = quint32(m_out.size());
    for (auto& e : m_entries) {
        const QByteArray nm = e.name.toUtf8();
        wr32(m_out, 0x02014b50); wr16(m_out, 20); wr16(m_out, 20); wr16(m_out, 0x0800); wr16(m_out, e.method); wr16(m_out, 0); wr16(m_out, 0x21);
        wr32(m_out, e.crc); wr32(m_out, e.compSize); wr32(m_out, e.size); wr16(m_out, quint16(nm.size()));
        wr16(m_out, 0); wr16(m_out, 0); wr16(m_out, 0); wr16(m_out, 0); wr32(m_out, 0); wr32(m_out, e.offset);
        m_out.append(nm);
    }
    const quint32 cdSize = quint32(m_out.size()) - cdStart;
    wr32(m_out, 0x06054b50); wr16(m_out, 0); wr16(m_out, 0); wr16(m_out, quint16(m_entries.size())); wr16(m_out, quint16(m_entries.size()));
    wr32(m_out, cdSize); wr32(m_out, cdStart); wr16(m_out, 0);
    return m_out;
}

bool ZipWriter::save(const QString& path) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(finish());
    return f.commit();
}

} // namespace lf
