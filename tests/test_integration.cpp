// Network integration tests over the loopback interface (simulated consoles / nodes).
#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include "app/Engine.h"
#include "protocols/ArtNet.h"
#include "protocols/Sacn.h"
#include "core/Log.h"
#include "storage/ProjectIO.h"
using namespace lf;

class TestIntegration : public QObject {
    Q_OBJECT
    std::unique_ptr<Engine> e;
    NetworkConfig base;
private slots:
    void initTestCase() {
        Log::instance().setEcho(false);
        EngineOptions o; o.headless = true; o.startRenderer = false; o.startNetwork = true; o.dataDir = QDir::temp().filePath("lf-test-int");
        e = std::make_unique<Engine>(o);
        base.oscIn = true; base.oscListenPort = 18000; base.artnetTarget = QHostAddress::LocalHost; base.bindAddress = QHostAddress::AnyIPv4;
        e->network().applyConfig(base);
        e->start();
    }
    void cleanupTestCase() { e->shutdown(); }
    void artnetOutput() {
        QUdpSocket rx; QVERIFY(rx.bind(QHostAddress::AnyIPv4, artnet::kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint));
        NetworkConfig c = base; c.artnetOut = true; c.enabledOutputUniverses = {1};
        e->applyNetworkConfig(c);
        DmxFrame f{}; f[0] = 77; f[511] = 9;
        e->network().setOutput(1, f);
        DmxFrame g{}; g[0] = 1; e->network().setOutput(2, g);          // not enabled: must never be sent
        QSet<int> seen; int value = -1, last = -1;
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            while (rx.hasPendingDatagrams()) { const auto d = rx.receiveDatagram(); artnet::DmxPacket p; if (artnet::decodeDmx(d.data(), p)) { seen.insert(p.portAddress); if (p.portAddress == 0) { value = quint8(p.data[0]); last = quint8(p.data[511]); } } }
            return value == 77;
        }(), 3000);
        QCOMPARE(last, 9);
        QVERIFY(!seen.contains(1));                                     // universe 2 (port address 1) blocked
        // blackout zeroes output
        e->setBlackout(true);
        QTRY_VERIFY_WITH_TIMEOUT([&] { while (rx.hasPendingDatagrams()) { artnet::DmxPacket p; if (artnet::decodeDmx(rx.receiveDatagram().data(), p) && p.portAddress == 0) value = quint8(p.data[0]); } return value == 0; }(), 3000);
        e->setBlackout(false);
        // rate limiting: unchanged universes are refreshed at the keep-alive interval only
        QThread::msleep(100); while (rx.hasPendingDatagrams()) rx.receiveDatagram();
        QElapsedTimer t; t.start(); int n = 0;
        while (t.elapsed() < 1500) { QCoreApplication::processEvents(); QThread::msleep(10); while (rx.hasPendingDatagrams()) { artnet::DmxPacket p; if (artnet::decodeDmx(rx.receiveDatagram().data(), p) && p.portAddress == 0) ++n; } }
        QVERIFY2(n >= 1 && n <= 4, qPrintable(QString("%1 keep-alive packets in 1.5 s").arg(n)));
        QVERIFY(e->network().stats().artnetOut > 0);
    }
    void sacnOutput() {
        QUdpSocket rx; QVERIFY(rx.bind(QHostAddress::AnyIPv4, sacn::kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint));
        NetworkConfig c = base; c.sacnOut = true; c.sacnUnicastTargets = {QHostAddress::LocalHost}; c.enabledOutputUniverses = {3}; c.sacnPriority = 120;
        e->applyNetworkConfig(c);
        DmxFrame f{}; f[10] = 200; e->network().setOutput(3, f);
        sacn::DataPacket got;
        QTRY_VERIFY_WITH_TIMEOUT([&] { while (rx.hasPendingDatagrams()) sacn::decode(rx.receiveDatagram().data(), got); return got.universe == 3 && got.data.size() > 10 && quint8(got.data[10]) == 200; }(), 3000);
        QCOMPARE(got.priority, quint8(120)); QCOMPARE(got.cid, e->network().cid()); QCOMPARE(got.sourceName, QString("LUMAFLOW"));
    }
    void oscControl() {
        QUdpSocket tx;
        osc::Message m{"/lumaflow/param/layer1.speed", {0.5}};
        tx.writeDatagram(osc::encode(m), QHostAddress::LocalHost, 18000);
        QTRY_COMPARE_WITH_TIMEOUT(e->params().value("layer1.speed"), 2.0, 3000);
        tx.writeDatagram(osc::encode({"/lumaflow/layer/2/effect", {QString("fractal")}}), QHostAddress::LocalHost, 18000);
        QTRY_COMPARE_WITH_TIMEOUT(e->program().layers[1].effectId, QString("fractal"), 3000);
        e->storeScene("A"); e->storeScene("B");
        tx.writeDatagram(osc::encode({"/lumaflow/scene/recall", {2}}), QHostAddress::LocalHost, 18000);
        QTRY_COMPARE_WITH_TIMEOUT(e->activeScene(), 1, 3000);
        // mapping from a console-specific OSC address
        ControlMapping cm; cm.console = "grandMA3"; cm.source = ControlSource::Osc; cm.address = "/gma3/Page1/Fader201"; cm.target = "master.intensity"; cm.inMin = 0; cm.inMax = 100;
        e->mapping().add(cm);
        tx.writeDatagram(osc::encode({"/gma3/Page1/Fader201", {25.0}}), QHostAddress::LocalHost, 18000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(e->params().value("master.intensity") - 0.25) < 1e-6, 3000);
    }
    void oscFeedback() {
        QUdpSocket rx; QVERIFY(rx.bind(QHostAddress::LocalHost, 18001));
        NetworkConfig c = e->network().config(); c.oscFeedbackTargets = {qMakePair(QHostAddress(QHostAddress::LocalHost), quint16(18001))};
        e->applyNetworkConfig(c);
        QTest::qWait(200);
        e->params().set("layer1.hue", 0.3, "touch");
        QString addr;
        QTRY_VERIFY_WITH_TIMEOUT([&] { while (rx.hasPendingDatagrams()) { QVector<osc::Message> ms; osc::decode(rx.receiveDatagram().data(), ms); for (auto& m : ms) if (m.address.endsWith("layer1.hue")) addr = m.address; } return !addr.isEmpty(); }(), 3000);
        QCOMPARE(addr, QString("/lumaflow/status/param/layer1.hue"));
    }
    void artnetInputConsolePersonality() {
        NetworkConfig c = e->network().config(); c.artnetIn = true; c.inputUniverses = {1}; c.artnetOut = false;
        e->applyNetworkConfig(c);
        e->consoles()[0].enabled = true; e->consoles()[0].universe = 1; e->consoles()[0].address = 101; e->consoles()[0].mode = PersonalityMode::Basic16;
        QTest::qWait(300);
        QUdpSocket tx;
        artnet::DmxPacket p; p.portAddress = 0; p.data = QByteArray(512, 0);
        p.data[100] = char(128); p.data[101] = 0;                  // master intensity 16-bit = 0x8000
        p.data[102] = char(3 * 8);                                 // layer 1 effect index 3
        p.data[103] = char(255);                                   // layer 1 opacity
        for (int i = 0; i < 3; ++i) { p.sequence = quint8(i + 1); tx.writeDatagram(artnet::encodeDmx(p), QHostAddress::LocalHost, artnet::kPort); QTest::qWait(50); }
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(e->params().value("master.intensity") - 32768 / 65535.0) < 1e-4, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(e->program().layers[0].effectId, e->effects().idAt(3), 3000);
        e->consoles()[0].enabled = false;
    }
    void remoteControl() {
        QVERIFY(e->startRemote(18080));
        const auto users = e->remote().users();
        QNetworkAccessManager nam;
        auto get = [&](const QString& path, const QString& token) {
            QNetworkRequest r(QUrl("http://127.0.0.1:18080" + path)); if (!token.isEmpty()) r.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
            QNetworkReply* rep = nam.get(r); QSignalSpy s(rep, &QNetworkReply::finished); s.wait(5000);
            const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); rep->deleteLater(); return code;
        };
        auto post = [&](const QString& path, const QString& token, const QByteArray& body) {
            QNetworkRequest r(QUrl("http://127.0.0.1:18080" + path)); r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            r.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
            QNetworkReply* rep = nam.post(r, body); QSignalSpy s(rep, &QNetworkReply::finished); s.wait(5000);
            const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); rep->deleteLater(); return code;
        };
        QCOMPARE(get("/api/ping", ""), 200);
        QCOMPARE(get("/", ""), 200);
        QCOMPARE(get("/api/state", ""), 401);
        QCOMPARE(get("/api/state", users["viewer"].first), 200);
        QCOMPARE(post("/api/param", users["viewer"].first, R"({"id":"layer1.p2","value":0.9})"), 403);
        QCOMPARE(post("/api/param", users["operator"].first, R"({"id":"layer1.p2","value":0.9})"), 200);
        QTRY_COMPARE_WITH_TIMEOUT(e->params().value("layer1.p2"), 0.9, 2000);
        QCOMPARE(post("/api/blackout", users["operator"].first, R"({"on":true})"), 403);
        QCOMPARE(post("/api/blackout", users["admin"].first, R"({"on":true})"), 200);
        QVERIFY(e->blackout()); e->setBlackout(false);
        QCOMPARE(post("/api/param", users["operator"].first, R"({"id":"nope","value":1})"), 400);
        e->remote().stop();
        QCOMPARE(e->params().value("layer1.p2"), 0.9);              // engine unaffected by remote shutdown
    }
    void multiInstanceSync() {
        SyncService primary, secondary;
        QMap<QString, double> applied; QJsonObject snap; int events = 0;
        SyncService::Callbacks pc; pc.snapshot = [] { return QJsonObject{{"state", 42}}; }; pc.showTime = [] { return 12.5; };
        SyncService::Callbacks sc;
        sc.applyParam = [&](const QString& id, double v) { applied[id] = v; };
        sc.applySnapshot = [&](const QJsonObject& o) { snap = o; };
        sc.applyEvent = [&](const QJsonObject&) { ++events; };
        primary.setCallbacks(pc); secondary.setCallbacks(sc);
        QVERIFY(primary.start(SyncRole::Primary, 17701, {}, {QHostAddress::LocalHost}, 17702));
        QVERIFY(secondary.start(SyncRole::Secondary, 17702, {}, {QHostAddress::LocalHost}, 17701));
        QTRY_VERIFY_WITH_TIMEOUT(snap["state"].toInt() == 42, 3000);       // snapshot on join
        primary.publishParam("layer1.speed", 2.5);
        primary.publishEvent({{"type", "scene"}, {"index", 1}});
        QTRY_COMPARE_WITH_TIMEOUT(applied.value("layer1.speed"), 2.5, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(events, 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(secondary.status().connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(secondary.status().rttValid, 6000);
        QVERIFY(qAbs(secondary.primaryTime() - 12.5) < 1.0);
        primary.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!secondary.status().connected, 4000);       // peer loss detected, secondary keeps running
        QVERIFY(primary.start(SyncRole::Primary, 17701, {}, {QHostAddress::LocalHost}, 17702));
        snap = {};
        QTRY_VERIFY_WITH_TIMEOUT(snap["state"].toInt() == 42, 5000);       // re-snapshot after reconnect
        secondary.stop(); primary.stop();
    }
    void autosaveAndCrashRecovery() {
        const QString dir = QDir::temp().filePath("lf-test-session");
        QDir(dir).removeRecursively();
        { SessionGuard g(dir); QVERIFY(!g.previousSessionCrashed()); g.begin("/tmp/x.lumaflow"); }   // no end() -> simulated crash
        SessionGuard g2(dir); QVERIFY(g2.previousSessionCrashed()); QCOMPARE(g2.previousSessionInfo()["project"].toString(), QString("/tmp/x.lumaflow"));
        g2.end(); SessionGuard g3(dir); QVERIFY(!g3.previousSessionCrashed());
        e->params().set("layer1.p3", 0.123);
        QVERIFY(e->autosaveNow());
        const auto r = ProjectIO::load(e->session().autosavePath());
        QVERIFY(r.ok()); QCOMPARE(r.doc["composition"].toObject()["values"].toObject()["layer1.p3"].toDouble(), 0.123);
    }
    void watchdog() {
        RenderWatchdog w; int stalls = 0, recovers = 0;
        w.start(100, [&](int) { ++stalls; }, [&] { ++recovers; });
        for (int i = 0; i < 10; ++i) { w.beat(); QThread::msleep(20); }
        QCOMPARE(stalls, 0);
        QThread::msleep(300); QCOMPARE(stalls, 1); QVERIFY(w.stalled());
        w.beat(); QThread::msleep(120); QCOMPARE(recovers, 1);
        w.stop();
    }
};
QTEST_MAIN(TestIntegration)
#include "test_integration.moc"
