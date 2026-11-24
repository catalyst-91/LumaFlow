#pragma once
// SMPTE timecode model plus MIDI Time Code (quarter frame + full frame) and
// Linear Timecode (LTC, SMPTE 12M biphase-mark audio) encoders/decoders.
#include <QString>
#include <QVector>
#include <QByteArray>
#include <optional>
#include <vector>
#include "Midi.h"

namespace lf::tc {

enum class Rate { Fps24 = 0, Fps25 = 1, Fps2997Drop = 2, Fps30 = 3 };
double rateFps(Rate r);
QString rateName(Rate r);

struct Timecode {
    int hours = 0, minutes = 0, seconds = 0, frames = 0;
    Rate rate = Rate::Fps25;
    QString toString() const;
    static std::optional<Timecode> parse(const QString& s, Rate r = Rate::Fps25);
    long long totalFrames() const;                      // handles drop-frame counting
    static Timecode fromFrames(long long frames, Rate r);
    double toSeconds() const { return totalFrames() / rateFps(rate); }
    static Timecode fromSeconds(double s, Rate r) { return fromFrames((long long)(s * rateFps(r) + 1e-6), r); }
    bool operator==(const Timecode& o) const { return hours == o.hours && minutes == o.minutes && seconds == o.seconds && frames == o.frames; }
};

// ---------------------------------------------------------------- MTC
QVector<midi::Message> mtcQuarterFrames(const Timecode& t);       // 8 messages, piece 0..7
QByteArray mtcFullFrame(const Timecode& t);                       // SysEx F0 7F 7F 01 01 hh mm ss ff F7

class MtcDecoder {
public:
    // Returns true when a new complete timecode was assembled.
    bool feed(const midi::Message& m);
    Timecode current() const { return m_tc; }
    bool locked() const { return m_locked; }
private:
    int m_pieces[8] = {0};
    int m_mask = 0;
    int m_lastPiece = -1;
    Timecode m_tc;
    bool m_locked = false;
};

// ---------------------------------------------------------------- LTC
std::vector<float> ltcEncode(const Timecode& start, int frameCount, int sampleRate, float amplitude = 0.5f);

class LtcDecoder {
public:
    explicit LtcDecoder(int sampleRate) : m_sr(sampleRate) {}
    // Feed mono samples; decoded frames are appended.
    void feed(const float* samples, int count, QVector<Timecode>& out);
    void setRateHint(Rate r) { m_rate = r; }
private:
    void pushBit(int bit, QVector<Timecode>& out);
    int m_sr;
    Rate m_rate = Rate::Fps25;
    float m_prev = 0.f;
    long long m_sampleIndex = 0, m_lastCross = -1;
    double m_bitPeriod = 0.0;     // estimated samples per bit
    bool m_halfPending = false;
    quint8 m_bits[80] = {0};
    int m_count = 0;
};

} // namespace lf::tc
