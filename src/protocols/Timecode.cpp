#include "Timecode.h"
#include <QRegularExpression>
#include <cmath>
#include <cstring>

namespace lf::tc {

double rateFps(Rate r) {
    switch (r) { case Rate::Fps24: return 24.0; case Rate::Fps25: return 25.0; case Rate::Fps2997Drop: return 30000.0 / 1001.0; case Rate::Fps30: return 30.0; }
    return 25.0;
}
QString rateName(Rate r) {
    switch (r) { case Rate::Fps24: return "24"; case Rate::Fps25: return "25"; case Rate::Fps2997Drop: return "29.97DF"; case Rate::Fps30: return "30"; }
    return "25";
}
static int nominal(Rate r) { return r == Rate::Fps24 ? 24 : r == Rate::Fps25 ? 25 : 30; }

QString Timecode::toString() const {
    return QString("%1:%2:%3%4%5").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')).arg(rate == Rate::Fps2997Drop ? ';' : ':').arg(frames, 2, 10, QChar('0'));
}

std::optional<Timecode> Timecode::parse(const QString& s, Rate r) {
    static const QRegularExpression re("^(\\d{1,2}):(\\d{2}):(\\d{2})[:;.](\\d{2})$");
    auto m = re.match(s.trimmed());
    if (!m.hasMatch()) return std::nullopt;
    Timecode t{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(), m.captured(4).toInt(), r};
    if (t.minutes > 59 || t.seconds > 59 || t.frames >= nominal(r) || t.hours > 23) return std::nullopt;
    return t;
}

long long Timecode::totalFrames() const {
    const int fps = nominal(rate);
    long long total = ((hours * 60LL + minutes) * 60 + seconds) * fps + frames;
    if (rate == Rate::Fps2997Drop) {
        const long long totalMinutes = hours * 60LL + minutes;
        total -= 2 * (totalMinutes - totalMinutes / 10);
    }
    return total;
}

Timecode Timecode::fromFrames(long long f, Rate r) {
    Timecode t; t.rate = r;
    const int fps = nominal(r);
    if (f < 0) f = 0;
    if (r == Rate::Fps2997Drop) {
        const long long perTen = 17982, perMin = 1798;   // frames per 10 min / per dropped minute
        const long long d = f / perTen, m = f % perTen;
        f += 18 * d + (m > 1 ? 2 * ((m - 2) / perMin) : 0);
    }
    t.frames = int(f % fps); f /= fps;
    t.seconds = int(f % 60); f /= 60;
    t.minutes = int(f % 60); f /= 60;
    t.hours = int(f % 24);
    return t;
}

// ---------------------------------------------------------------- MTC
QVector<midi::Message> mtcQuarterFrames(const Timecode& t) {
    const int v[8] = { t.frames & 0x0F, (t.frames >> 4) & 0x01, t.seconds & 0x0F, (t.seconds >> 4) & 0x03,
                       t.minutes & 0x0F, (t.minutes >> 4) & 0x03, t.hours & 0x0F, ((t.hours >> 4) & 0x01) | (int(t.rate) << 1) };
    QVector<midi::Message> out;
    for (int i = 0; i < 8; ++i) { midi::Message m; m.type = midi::Type::QuarterFrame; m.data1 = (i << 4) | v[i]; out << m; }
    return out;
}

QByteArray mtcFullFrame(const Timecode& t) {
    QByteArray b; const char hdr[] = {char(0xF0), 0x7F, 0x7F, 0x01, 0x01};
    b.append(hdr, 5);
    b.append(char((int(t.rate) << 5) | (t.hours & 0x1F))); b.append(char(t.minutes)); b.append(char(t.seconds)); b.append(char(t.frames));
    b.append(char(0xF7));
    return b;
}

bool MtcDecoder::feed(const midi::Message& m) {
    if (m.type == midi::Type::SysEx && m.sysex.size() == 10 && quint8(m.sysex[1]) == 0x7F && quint8(m.sysex[3]) == 0x01 && quint8(m.sysex[4]) == 0x01) {
        const int h = quint8(m.sysex[5]);
        m_tc = Timecode{h & 0x1F, quint8(m.sysex[6]), quint8(m.sysex[7]), quint8(m.sysex[8]), Rate((h >> 5) & 3)};
        m_locked = true; m_mask = 0; return true;
    }
    if (m.type != midi::Type::QuarterFrame) return false;
    const int piece = (m.data1 >> 4) & 7;
    if (m_lastPiece >= 0 && piece != (m_lastPiece + 1) % 8) m_mask = 0;   // discontinuity
    m_lastPiece = piece;
    m_pieces[piece] = m.data1 & 0x0F;
    m_mask |= (1 << piece);
    if (piece == 7 && m_mask == 0xFF) {
        Timecode t;
        t.rate = Rate((m_pieces[7] >> 1) & 3);
        t.frames = m_pieces[0] | ((m_pieces[1] & 1) << 4);
        t.seconds = m_pieces[2] | ((m_pieces[3] & 3) << 4);
        t.minutes = m_pieces[4] | ((m_pieces[5] & 3) << 4);
        t.hours = m_pieces[6] | ((m_pieces[7] & 1) << 4);
        // The 8 quarter frames span two frames; the value refers to the first.
        m_tc = Timecode::fromFrames(t.totalFrames() + 2, t.rate);
        m_locked = true; m_mask = 0;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- LTC
static void setBits(quint8* bits, int start, int count, int value) { for (int i = 0; i < count; ++i) bits[start + i] = (value >> i) & 1; }

static void ltcFrameBits(const Timecode& t, quint8* bits) {
    std::fill(bits, bits + 80, 0);
    setBits(bits, 0, 4, t.frames % 10); setBits(bits, 8, 2, t.frames / 10);
    bits[10] = t.rate == Rate::Fps2997Drop ? 1 : 0;
    setBits(bits, 16, 4, t.seconds % 10); setBits(bits, 24, 3, t.seconds / 10);
    setBits(bits, 32, 4, t.minutes % 10); setBits(bits, 40, 3, t.minutes / 10);
    setBits(bits, 48, 4, t.hours % 10); setBits(bits, 56, 2, t.hours / 10);
    static const int sync[16] = {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1};
    for (int i = 0; i < 16; ++i) bits[64 + i] = quint8(sync[i]);
    // polarity correction bit (27 for 25fps is bit 59; use 27 for 30/24) keeps even number of zeros
    int zeros = 0; for (int i = 0; i < 80; ++i) if (!bits[i]) ++zeros;
    const int pcBit = t.rate == Rate::Fps25 ? 59 : 27;
    if (zeros % 2) bits[pcBit] = 1;
}

std::vector<float> ltcEncode(const Timecode& start, int frameCount, int sr, float amp) {
    std::vector<float> out;
    const double spb = sr / (rateFps(start.rate) * 80.0);
    double pos = 0; float level = amp;
    long long f0 = start.totalFrames();
    for (int f = 0; f < frameCount; ++f) {
        quint8 bits[80]; ltcFrameBits(Timecode::fromFrames(f0 + f, start.rate), bits);
        for (int b = 0; b < 80; ++b) {
            level = -level;                                    // transition at bit start
            const double half = pos + spb / 2, end = pos + spb;
            while (double(out.size()) < half) out.push_back(level);
            if (bits[b]) level = -level;                        // mid-bit transition for '1'
            while (double(out.size()) < end) out.push_back(level);
            pos = end;
        }
    }
    return out;
}

void LtcDecoder::pushBit(int bit, QVector<Timecode>& out) {
    std::memmove(m_bits, m_bits + 1, 79); m_bits[79] = quint8(bit);
    if (m_count < 80) { ++m_count; if (m_count < 80) return; }
    static const int sync[16] = {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1};
    for (int i = 0; i < 16; ++i) if (m_bits[64 + i] != sync[i]) return;
    auto val = [&](int s, int n) { int v = 0; for (int i = 0; i < n; ++i) v |= m_bits[s + i] << i; return v; };
    Timecode t;
    t.rate = m_bits[10] ? Rate::Fps2997Drop : m_rate;
    t.frames = val(0, 4) + 10 * val(8, 2);
    t.seconds = val(16, 4) + 10 * val(24, 3);
    t.minutes = val(32, 4) + 10 * val(40, 3);
    t.hours = val(48, 4) + 10 * val(56, 2);
    out << t;
    m_count = 0;
}

void LtcDecoder::feed(const float* s, int n, QVector<Timecode>& out) {
    const double nominalSpb = m_sr / (rateFps(m_rate) * 80.0);
    if (m_bitPeriod <= 0) m_bitPeriod = nominalSpb;
    for (int i = 0; i < n; ++i, ++m_sampleIndex) {
        const float v = s[i];
        const bool cross = (m_prev <= 0.f && v > 0.f) || (m_prev >= 0.f && v < 0.f);
        m_prev = v;
        if (!cross) continue;
        if (m_lastCross < 0) { m_lastCross = m_sampleIndex; continue; }
        const double dt = double(m_sampleIndex - m_lastCross);
        m_lastCross = m_sampleIndex;
        if (dt > m_bitPeriod * 0.75) {                         // long interval -> '0'
            if (dt < m_bitPeriod * 1.5) m_bitPeriod = 0.9 * m_bitPeriod + 0.1 * dt;
            m_halfPending = false;
            pushBit(0, out);
        } else {                                                // short interval -> half of a '1'
            if (m_halfPending) { m_halfPending = false; pushBit(1, out); m_bitPeriod = 0.9 * m_bitPeriod + 0.1 * dt * 2; }
            else m_halfPending = true;
        }
    }
}

} // namespace lf::tc
