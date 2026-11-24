#pragma once
// MIDI 1.0 message model, byte-stream parser (with running status) and encoder.
// Device I/O is provided by MidiBackend (ALSA sequencer on Linux, WinMM on
// Windows; CoreMIDI backend is listed as future work in BLOCKERS.md).
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QStringList>
#include <functional>
#include <memory>

namespace lf::midi {

enum class Type : quint8 { NoteOff = 0x80, NoteOn = 0x90, PolyAftertouch = 0xA0, ControlChange = 0xB0,
                           ProgramChange = 0xC0, ChannelPressure = 0xD0, PitchBend = 0xE0,
                           QuarterFrame = 0xF1, SongPosition = 0xF2, Clock = 0xF8, Start = 0xFA,
                           Continue = 0xFB, Stop = 0xFC, SysEx = 0xF0 };

struct Message {
    Type type = Type::ControlChange;
    int channel = 0;      // 0..15
    int data1 = 0;        // note / controller / program
    int data2 = 0;        // velocity / value
    QByteArray sysex;     // full F0 .. F7 for SysEx
    double normalized() const;           // value as 0..1
    QString key() const;                 // mapping key e.g. "cc:1:7", "note:0:60"
    QString toString() const;
};

QByteArray encode(const Message& m);

class Parser {
public:
    // Feed raw bytes; complete messages are appended to out.
    void feed(const QByteArray& bytes, QVector<Message>& out);
private:
    quint8 m_status = 0;
    QByteArray m_buf;
    bool m_inSysex = false;
};

// Device backend abstraction.
class Backend {
public:
    virtual ~Backend() = default;
    virtual QString name() const = 0;
    virtual bool available() const = 0;
    virtual QStringList inputPorts() = 0;
    virtual QStringList outputPorts() = 0;
    virtual bool openVirtual(const QString& clientName) = 0;     // creates LUMAFLOW in/out ports
    virtual bool connectInput(const QString& port) = 0;
    virtual bool connectOutput(const QString& port) = 0;
    virtual bool send(const Message& m) = 0;
    virtual void poll(QVector<Message>& out) = 0;                // non-blocking
    static std::unique_ptr<Backend> createDefault();
};

} // namespace lf::midi
