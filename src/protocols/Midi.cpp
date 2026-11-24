#include "Midi.h"
#include "core/Log.h"
#ifdef LF_HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

namespace lf::midi {

double Message::normalized() const {
    switch (type) {
    case Type::PitchBend: return ((data2 << 7) | data1) / 16383.0;
    case Type::NoteOff: return 0.0;
    case Type::ProgramChange: return data1 / 127.0;
    case Type::ChannelPressure: return data1 / 127.0;
    default: return data2 / 127.0;
    }
}

QString Message::key() const {
    switch (type) {
    case Type::ControlChange: return QString("cc:%1:%2").arg(channel).arg(data1);
    case Type::NoteOn: case Type::NoteOff: return QString("note:%1:%2").arg(channel).arg(data1);
    case Type::ProgramChange: return QString("pc:%1").arg(channel);
    case Type::PitchBend: return QString("pb:%1").arg(channel);
    case Type::ChannelPressure: return QString("at:%1").arg(channel);
    case Type::PolyAftertouch: return QString("pat:%1:%2").arg(channel).arg(data1);
    default: return QString("sys:%1").arg(int(type));
    }
}

QString Message::toString() const { return QString("%1 v=%2").arg(key()).arg(data2); }

QByteArray encode(const Message& m) {
    QByteArray b;
    const quint8 t = quint8(m.type);
    if (t >= 0xF0) {
        if (m.type == Type::SysEx) return m.sysex;
        b.append(char(t));
        if (m.type == Type::QuarterFrame) b.append(char(m.data1 & 0x7F));
        if (m.type == Type::SongPosition) { b.append(char(m.data1 & 0x7F)); b.append(char(m.data2 & 0x7F)); }
        return b;
    }
    b.append(char(t | (m.channel & 0x0F)));
    b.append(char(m.data1 & 0x7F));
    if (m.type != Type::ProgramChange && m.type != Type::ChannelPressure) b.append(char(m.data2 & 0x7F));
    return b;
}

static int dataBytesFor(quint8 status) {
    const quint8 hi = status & 0xF0;
    if (hi == 0xC0 || hi == 0xD0) return 1;
    if (hi >= 0x80 && hi <= 0xE0) return 2;
    if (status == 0xF1 || status == 0xF3) return 1;
    if (status == 0xF2) return 2;
    return 0;
}

void Parser::feed(const QByteArray& bytes, QVector<Message>& out) {
    for (char ch : bytes) {
        const quint8 b = quint8(ch);
        if (b >= 0xF8) {                                // realtime: may interleave anything
            Message m; m.type = Type(b); out << m; continue;
        }
        if (b == 0xF0) { m_inSysex = true; m_buf = QByteArray(1, char(b)); continue; }
        if (m_inSysex) {
            m_buf.append(char(b));
            if (b == 0xF7) { Message m; m.type = Type::SysEx; m.sysex = m_buf; out << m; m_inSysex = false; m_buf.clear(); }
            continue;
        }
        if (b & 0x80) { m_status = b; m_buf.clear(); if (dataBytesFor(b) == 0 && b >= 0xF0) { m_status = 0; } continue; }
        if (!m_status) continue;                        // stray data byte
        m_buf.append(char(b));
        const int need = dataBytesFor(m_status);
        if (m_buf.size() < need) continue;
        Message m;
        if (m_status >= 0xF0) { m.type = Type(m_status); m.data1 = quint8(m_buf[0]); if (need > 1) m.data2 = quint8(m_buf[1]); m_status = 0; }
        else {
            m.type = Type(m_status & 0xF0); m.channel = m_status & 0x0F;
            m.data1 = quint8(m_buf[0]); if (need > 1) m.data2 = quint8(m_buf[1]);
            if (m.type == Type::NoteOn && m.data2 == 0) m.type = Type::NoteOff;   // velocity 0 = note off
        }
        out << m;
        m_buf.clear();                                  // keep running status
    }
}

// ---------------------------------------------------------------- backends
class NullBackend : public Backend {
public:
    QString name() const override { return "none"; }
    bool available() const override { return false; }
    QStringList inputPorts() override { return {}; }
    QStringList outputPorts() override { return {}; }
    bool openVirtual(const QString&) override { return false; }
    bool connectInput(const QString&) override { return false; }
    bool connectOutput(const QString&) override { return false; }
    bool send(const Message&) override { return false; }
    void poll(QVector<Message>&) override {}
};

#ifdef LF_HAVE_ALSA
class AlsaBackend : public Backend {
public:
    ~AlsaBackend() override { if (m_seq) snd_seq_close(m_seq); }
    QString name() const override { return "ALSA sequencer"; }
    bool available() const override { return m_seq != nullptr; }
    bool init() {
        if (snd_seq_open(&m_seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) { m_seq = nullptr; return false; }
        return true;
    }
    QStringList listPorts(unsigned caps) {
        QStringList out; if (!m_seq) return out;
        snd_seq_client_info_t* ci; snd_seq_port_info_t* pi;
        snd_seq_client_info_alloca(&ci); snd_seq_port_info_alloca(&pi);
        snd_seq_client_info_set_client(ci, -1);
        while (snd_seq_query_next_client(m_seq, ci) >= 0) {
            const int c = snd_seq_client_info_get_client(ci);
            snd_seq_port_info_set_client(pi, c); snd_seq_port_info_set_port(pi, -1);
            while (snd_seq_query_next_port(m_seq, pi) >= 0) {
                if ((snd_seq_port_info_get_capability(pi) & caps) == caps && c != snd_seq_client_id(m_seq))
                    out << QString("%1:%2 %3").arg(c).arg(snd_seq_port_info_get_port(pi)).arg(snd_seq_port_info_get_name(pi));
            }
        }
        return out;
    }
    QStringList inputPorts() override { return listPorts(SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ); }
    QStringList outputPorts() override { return listPorts(SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE); }
    bool openVirtual(const QString& client) override {
        if (!m_seq) return false;
        snd_seq_set_client_name(m_seq, client.toUtf8().constData());
        m_in = snd_seq_create_simple_port(m_seq, "LUMAFLOW In", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        m_out = snd_seq_create_simple_port(m_seq, "LUMAFLOW Out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        snd_midi_event_new(256, &m_enc); snd_midi_event_new(256, &m_dec);
        return m_in >= 0 && m_out >= 0;
    }
    static bool parseAddr(const QString& port, int& c, int& p) {
        const QStringList cp = port.section(' ', 0, 0).split(':');
        if (cp.size() != 2) return false; c = cp[0].toInt(); p = cp[1].toInt(); return true;
    }
    bool connectInput(const QString& port) override { int c, p; return m_seq && parseAddr(port, c, p) && snd_seq_connect_from(m_seq, m_in, c, p) >= 0; }
    bool connectOutput(const QString& port) override { int c, p; return m_seq && parseAddr(port, c, p) && snd_seq_connect_to(m_seq, m_out, c, p) >= 0; }
    bool send(const Message& m) override {
        if (!m_seq || m_out < 0) return false;
        const QByteArray b = encode(m);
        snd_seq_event_t ev; snd_seq_ev_clear(&ev);
        snd_midi_event_reset_encode(m_enc);
        if (snd_midi_event_encode(m_enc, reinterpret_cast<const unsigned char*>(b.constData()), b.size(), &ev) <= 0) return false;
        snd_seq_ev_set_source(&ev, m_out); snd_seq_ev_set_subs(&ev); snd_seq_ev_set_direct(&ev);
        snd_seq_event_output_direct(m_seq, &ev);
        return true;
    }
    void poll(QVector<Message>& out) override {
        if (!m_seq || !m_dec) return;
        snd_seq_event_t* ev = nullptr;
        while (snd_seq_event_input_pending(m_seq, 1) > 0 && snd_seq_event_input(m_seq, &ev) >= 0 && ev) {
            unsigned char buf[256];
            const long n = snd_midi_event_decode(m_dec, buf, sizeof buf, ev);
            if (n > 0) m_parser.feed(QByteArray(reinterpret_cast<char*>(buf), int(n)), out);
        }
    }
private:
    snd_seq_t* m_seq = nullptr;
    int m_in = -1, m_out = -1;
    snd_midi_event_t* m_enc = nullptr; snd_midi_event_t* m_dec = nullptr;
    Parser m_parser;
};
#endif

std::unique_ptr<Backend> Backend::createDefault() {
#ifdef LF_HAVE_ALSA
    auto a = std::make_unique<AlsaBackend>();
    if (a->init()) return a;
    logWarn("midi", "ALSA sequencer unavailable (no /dev/snd/seq?) - MIDI device I/O disabled");
#endif
    return std::make_unique<NullBackend>();
}

} // namespace lf::midi
