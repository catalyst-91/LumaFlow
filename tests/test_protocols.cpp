#include <QtTest>
#include "protocols/ArtNet.h"
#include "protocols/Sacn.h"
#include "protocols/Osc.h"
#include "protocols/Midi.h"
#include "protocols/Timecode.h"
using namespace lf;

class TestProtocols : public QObject {
    Q_OBJECT
private slots:
    void artnetRoundTrip() {
        artnet::DmxPacket p; p.portAddress = artnet::portAddress(1, 2, 3); p.sequence = 7; p.data = QByteArray(512, 0);
        p.data[0] = char(255); p.data[511] = char(42);
        const QByteArray b = artnet::encodeDmx(p);
        QCOMPARE(b.size(), 18 + 512);
        QCOMPARE(b.left(8), QByteArray("Art-Net\0", 8));
        QCOMPARE(quint8(b[8]), quint8(0x00)); QCOMPARE(quint8(b[9]), quint8(0x50));   // OpDmx little endian
        QCOMPARE(quint8(b[11]), quint8(14));                                          // protocol version
        artnet::DmxPacket d; QVERIFY(artnet::decodeDmx(b, d));
        QCOMPARE(d.portAddress, p.portAddress); QCOMPARE(d.sequence, quint8(7)); QCOMPARE(quint8(d.data[511]), quint8(42));
    }
    void artnetOddLengthPadded() {
        artnet::DmxPacket p; p.data = QByteArray(3, 1);
        artnet::DmxPacket d; QVERIFY(artnet::decodeDmx(artnet::encodeDmx(p), d)); QCOMPARE(d.data.size(), 4);
    }
    void artnetRejectsGarbage() { artnet::DmxPacket d; QVERIFY(!artnet::decodeDmx("hello world, not artnet", d)); }
    void artnetPollReply() {
        artnet::PollReply r; r.ip = QHostAddress("10.0.0.5"); r.shortName = "LUMAFLOW"; r.longName = "LUMAFLOW test"; r.numPorts = 2; r.firstPortAddress = 0x0123;
        artnet::PollReply d; QVERIFY(artnet::decodePollReply(artnet::encodePollReply(r), d));
        QCOMPARE(d.ip.toString(), QString("10.0.0.5")); QCOMPARE(d.shortName, QString("LUMAFLOW")); QCOMPARE(d.numPorts, 2);
    }
    void sacnRoundTrip() {
        sacn::DataPacket p; p.cid = QUuid::createUuid(); p.sourceName = "LUMAFLOW"; p.priority = 150; p.universe = 63999; p.sequence = 9; p.data = QByteArray(512, char(200));
        const QByteArray b = sacn::encode(p);
        QCOMPARE(b.size(), 638);                                         // E1.31 full universe packet size
        QCOMPARE(quint8(b[16]) & 0xF0, 0x70);                             // flags
        QCOMPARE(((quint8(b[16]) & 0x0F) << 8) | quint8(b[17]), 638 - 16); // root PDU length
        QCOMPARE((quint8(b[123]) << 8) | quint8(b[124]), 513);           // property count incl. start code
        sacn::DataPacket d; QVERIFY(sacn::decode(b, d));
        QCOMPARE(d.cid, p.cid); QCOMPARE(d.universe, quint16(63999)); QCOMPARE(d.priority, quint8(150)); QCOMPARE(d.data, p.data); QCOMPARE(d.sourceName, QString("LUMAFLOW"));
    }
    void sacnMulticast() { QCOMPARE(sacn::multicastAddress(1).toString(), QString("239.255.0.1")); QCOMPARE(sacn::multicastAddress(256 + 7).toString(), QString("239.255.1.7")); }
    void oscRoundTrip() {
        osc::Message m; m.address = "/lumaflow/layer/1/speed"; m.args << 0.75 << 3 << QString("hello") << QByteArray("\x01\x02\x03", 3) << true;
        const QByteArray b = osc::encode(m);
        QCOMPARE(b.size() % 4, 0);
        QVector<osc::Message> out; QVERIFY(osc::decode(b, out)); QCOMPARE(out.size(), 1);
        QCOMPARE(out[0].address, m.address); QCOMPARE(out[0].args.size(), 5);
        QVERIFY(qAbs(out[0].args[0].toDouble() - 0.75) < 1e-6); QCOMPARE(out[0].args[1].toInt(), 3); QCOMPARE(out[0].args[2].toString(), QString("hello"));
        QCOMPARE(out[0].args[3].toByteArray().size(), 3); QCOMPARE(out[0].args[4].toBool(), true);
    }
    void oscBundle() {
        osc::Message a{"/a", {1}}, b{"/b", {2.5}};
        QVector<osc::Message> out; QVERIFY(osc::decode(osc::encodeBundle({a, b}), out));
        QCOMPARE(out.size(), 2); QCOMPARE(out[1].address, QString("/b"));
    }
    void oscPatterns() {
        QVERIFY(osc::matchPattern("/layer/*/speed", "/layer/2/speed"));
        QVERIFY(!osc::matchPattern("/layer/*/speed", "/layer/2/x/speed"));
        QVERIFY(osc::matchPattern("/fader/[1-3]", "/fader/2"));
        QVERIFY(!osc::matchPattern("/fader/[1-3]", "/fader/4"));
        QVERIFY(osc::matchPattern("/{go,back}", "/back"));
        QVERIFY(osc::matchPattern("/x?", "/x1"));
    }
    void midiRunningStatus() {
        midi::Parser p; QVector<midi::Message> out;
        p.feed(QByteArray("\xB0\x07\x64\x08\x20\x90\x3C\x00", 8), out);   // CC7=100, (running) CC8=32, NoteOn vel 0 -> NoteOff
        QCOMPARE(out.size(), 3);
        QCOMPARE(out[0].key(), QString("cc:0:7")); QCOMPARE(out[0].data2, 100);
        QCOMPARE(out[1].key(), QString("cc:0:8")); QCOMPARE(out[1].data2, 32);
        QVERIFY(out[2].type == midi::Type::NoteOff);
    }
    void midiRealtimeInterleaved() {
        midi::Parser p; QVector<midi::Message> out;
        p.feed(QByteArray("\xB1\x01\xF8\x7F", 4), out);
        QCOMPARE(out.size(), 2); QVERIFY(out[0].type == midi::Type::Clock); QCOMPARE(out[1].data2, 127); QCOMPARE(out[1].channel, 1);
    }
    void midiSysex() { midi::Parser p; QVector<midi::Message> out; p.feed(QByteArray("\xF0\x7E\x01\xF7", 4), out); QCOMPARE(out.size(), 1); QCOMPARE(out[0].sysex.size(), 4); }
    void timecodeDropFrame() {
        auto t = tc::Timecode::parse("00:10:00;00", tc::Rate::Fps2997Drop); QVERIFY(t);
        QCOMPARE(t->totalFrames(), 17982LL);                              // 10 minutes of 29.97 DF
        QCOMPARE(tc::Timecode::fromFrames(1800, tc::Rate::Fps2997Drop).toString(), QString("00:01:00;02"));
        for (long long f : {0LL, 1799LL, 1800LL, 17981LL, 17982LL, 107892LL}) QCOMPARE(tc::Timecode::fromFrames(f, tc::Rate::Fps2997Drop).totalFrames(), f);
    }
    void mtcRoundTrip() {
        tc::Timecode t{1, 2, 3, 4, tc::Rate::Fps25};
        tc::MtcDecoder d; bool got = false;
        for (auto& m : tc::mtcQuarterFrames(t)) got = d.feed(m);
        QVERIFY(got);
        QCOMPARE(d.current().toString(), QString("01:02:03:06"));        // +2 frames compensation
        midi::Message full; full.type = midi::Type::SysEx; full.sysex = tc::mtcFullFrame(t);
        QVERIFY(d.feed(full)); QCOMPARE(d.current().toString(), QString("01:02:03:04"));
    }
    void ltcRoundTrip() {
        tc::Timecode start{10, 59, 59, 20, tc::Rate::Fps25};
        auto audio = tc::ltcEncode(start, 12, 48000);
        tc::LtcDecoder d(48000); QVector<tc::Timecode> out;
        d.feed(audio.data(), int(audio.size()), out);
        QVERIFY2(out.size() >= 10, qPrintable(QString("decoded %1 frames").arg(out.size())));
        // frames decoded in order and consecutive
        for (int i = 1; i < out.size(); ++i) QCOMPARE(out[i].totalFrames(), out[i - 1].totalFrames() + 1);
        QVERIFY(out.first().totalFrames() >= start.totalFrames());
        QCOMPARE(out.last().hours, 11);   // crossed the hour boundary 10:59:59:24 -> 11:00:00:00
    }
};
QTEST_GUILESS_MAIN(TestProtocols)
#include "test_protocols.moc"
