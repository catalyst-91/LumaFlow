#include "Osc.h"
#include <QtEndian>
#include <cstring>

namespace lf::osc {

static void padString(QByteArray& b, const QByteArray& s) {
    b.append(s); b.append(char(0));
    while (b.size() % 4) b.append(char(0));
}
static void put32(QByteArray& b, quint32 v) { char t[4]; qToBigEndian(v, t); b.append(t, 4); }

QString Message::typeTags() const {
    QString t = ",";
    for (const auto& a : args) {
        switch (a.typeId()) {
        case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: t += 'i'; break;
        case QMetaType::Float: case QMetaType::Double: t += 'f'; break;
        case QMetaType::QByteArray: t += 'b'; break;
        case QMetaType::Bool: t += a.toBool() ? 'T' : 'F'; break;
        default: t += 's'; break;
        }
    }
    return t;
}

double Message::numberArg(int i, double fallback) const {
    if (i < 0 || i >= args.size()) return fallback;
    bool ok = false; double v = args[i].toDouble(&ok);
    if (args[i].typeId() == QMetaType::Bool) return args[i].toBool() ? 1.0 : 0.0;
    return ok ? v : fallback;
}

QString Message::toString() const {
    QStringList parts;
    for (auto& a : args) parts << a.toString();
    return address + " " + typeTags() + " " + parts.join(' ');
}

QByteArray encode(const Message& m) {
    QByteArray b;
    padString(b, m.address.toUtf8());
    padString(b, m.typeTags().toLatin1());
    for (const auto& a : m.args) {
        switch (a.typeId()) {
        case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: put32(b, quint32(qint32(a.toLongLong()))); break;
        case QMetaType::Float: case QMetaType::Double: { float f = a.toFloat(); quint32 u; std::memcpy(&u, &f, 4); put32(b, u); break; }
        case QMetaType::QByteArray: { QByteArray d = a.toByteArray(); put32(b, quint32(d.size())); b.append(d); while (b.size() % 4) b.append(char(0)); break; }
        case QMetaType::Bool: break;
        default: padString(b, a.toString().toUtf8()); break;
        }
    }
    return b;
}

QByteArray encodeBundle(const QVector<Message>& msgs, quint64 timetag) {
    QByteArray b;
    padString(b, "#bundle");
    char t[8]; qToBigEndian(timetag, t); b.append(t, 8);
    for (const auto& m : msgs) { QByteArray e = encode(m); put32(b, quint32(e.size())); b.append(e); }
    return b;
}

static bool readString(const QByteArray& b, int& pos, QString& out) {
    const int start = pos;
    while (pos < b.size() && b[pos] != 0) ++pos;
    if (pos >= b.size()) return false;
    out = QString::fromUtf8(b.constData() + start, pos - start);
    ++pos; while (pos % 4) ++pos;
    return pos <= b.size();
}
static bool read32(const QByteArray& b, int& pos, quint32& v) {
    if (pos + 4 > b.size()) return false;
    v = qFromBigEndian<quint32>(b.constData() + pos); pos += 4; return true;
}

static bool decodeMessage(const QByteArray& b, Message& m) {
    int pos = 0; QString tags;
    if (!readString(b, pos, m.address) || !m.address.startsWith('/')) return false;
    if (pos >= b.size()) { return true; }                       // no type tags: legal (old style)
    if (!readString(b, pos, tags) || !tags.startsWith(',')) return false;
    for (int i = 1; i < tags.size(); ++i) {
        const QChar c = tags[i];
        quint32 u = 0;
        if (c == 'i') { if (!read32(b, pos, u)) return false; m.args << int(qint32(u)); }
        else if (c == 'f') { if (!read32(b, pos, u)) return false; float f; std::memcpy(&f, &u, 4); m.args << double(f); }
        else if (c == 's' || c == 'S') { QString s; if (!readString(b, pos, s)) return false; m.args << s; }
        else if (c == 'b') { if (!read32(b, pos, u) || pos + int(u) > b.size()) return false; m.args << b.mid(pos, int(u)); pos += int(u); while (pos % 4) ++pos; }
        else if (c == 'T') m.args << true;
        else if (c == 'F') m.args << false;
        else if (c == 'N' || c == 'I') m.args << QVariant();
        else if (c == 'h' || c == 'd' || c == 't') {
            if (pos + 8 > b.size()) return false;
            quint64 v = qFromBigEndian<quint64>(b.constData() + pos); pos += 8;
            if (c == 'd') { double d; std::memcpy(&d, &v, 8); m.args << d; } else m.args << qlonglong(v);
        }
        else return false;
    }
    return true;
}

bool decode(const QByteArray& b, QVector<Message>& out) {
    if (b.startsWith("#bundle")) {
        int pos = 16;
        while (pos + 4 <= b.size()) {
            quint32 sz; read32(b, pos, sz);
            if (pos + int(sz) > b.size()) return false;
            if (!decode(b.mid(pos, int(sz)), out)) return false;
            pos += int(sz);
        }
        return true;
    }
    Message m;
    if (!decodeMessage(b, m)) return false;
    out << m;
    return true;
}

static bool matchImpl(const QString& p, int pi, const QString& a, int ai) {
    while (pi < p.size()) {
        const QChar c = p[pi];
        if (c == '*') {
            for (int k = ai; k <= a.size(); ++k) {
                if (k > ai && a[k - 1] == '/') break;
                if (matchImpl(p, pi + 1, a, k)) return true;
            }
            return false;
        }
        if (ai >= a.size()) return false;
        if (c == '?') { if (a[ai] == '/') return false; ++pi; ++ai; continue; }
        if (c == '[') {
            int end = p.indexOf(']', pi); if (end < 0) return false;
            QString set = p.mid(pi + 1, end - pi - 1); bool neg = set.startsWith('!'); if (neg) set.remove(0, 1);
            bool hit = false;
            for (int k = 0; k < set.size(); ++k) {
                if (k + 2 < set.size() && set[k + 1] == '-') { if (a[ai] >= set[k] && a[ai] <= set[k + 2]) hit = true; k += 2; }
                else if (set[k] == a[ai]) hit = true;
            }
            if (hit == neg) return false;
            pi = end + 1; ++ai; continue;
        }
        if (c == '{') {
            int end = p.indexOf('}', pi); if (end < 0) return false;
            const QStringList alts = p.mid(pi + 1, end - pi - 1).split(',');
            for (const auto& alt : alts) if (a.mid(ai, alt.size()) == alt && matchImpl(p, end + 1, a, ai + int(alt.size()))) return true;
            return false;
        }
        if (c != a[ai]) return false;
        ++pi; ++ai;
    }
    return ai == a.size();
}

bool matchPattern(const QString& pattern, const QString& address) { return matchImpl(pattern, 0, address, 0); }

} // namespace lf::osc
