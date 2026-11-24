// Stream outputs: NDI sink against a mock NDI runtime (ABI-level test, not real NDI),
// output-module plugins, region cropping, frame dropping and engine integration.
#include <QtTest>
#include <QLibrary>
#include "output/StreamSinks.h"
#include "plugins/PluginHost.h"
#include "core/Log.h"
using namespace lf;

struct MockState { int initialized, destroyed, created, send_destroyed, frames; char name[128]; char groups[128];
                   int xres, yres, fourcc, stride, fps_n, fps_d, format; qint64 timecode; quint8 first[8]; quint8 last_row_first[4]; };

static QImage testFrame(int w, int h) {
    QImage img(w, h, QImage::Format_RGBA8888); img.fill(QColor(10, 20, 30, 255));
    img.setPixelColor(0, 0, QColor(255, 0, 0)); img.setPixelColor(1, 0, QColor(0, 255, 0)); img.setPixelColor(0, h - 1, QColor(0, 0, 255));
    return img;
}

class TestStreams : public QObject {
    Q_OBJECT
    MockState* mock = nullptr;
    QLibrary mockLib;
private slots:
    void initTestCase() {
        Log::instance().setEcho(false);
        qputenv("LUMAFLOW_NDI_LIB", LF_MOCK_NDI);
        mockLib.setFileName(LF_MOCK_NDI);
        mockLib.setLoadHints(QLibrary::PreventUnloadHint);   // the sink unloads the runtime on release; keep our state pointer valid
        QVERIFY(mockLib.load());
        mock = reinterpret_cast<MockState* (*)()>(mockLib.resolve("mock_ndi_state"))();
        QVERIFY(mock);
    }
    void runtimeDiscovery() {
        QCOMPARE(findNdiRuntime(), QString(LF_MOCK_NDI));
        qputenv("LUMAFLOW_NDI_LIB", "/nonexistent/libndi.so.6");
        QStringList searched; findNdiRuntime(&searched);
        QVERIFY(searched.contains("/nonexistent/libndi.so.6"));
        QVERIFY(!StreamOutputManager::probe("ndi", nullptr).detail.isEmpty());
        qputenv("LUMAFLOW_NDI_LIB", LF_MOCK_NDI);
        QVERIFY(StreamOutputManager::probe("ndi", nullptr).available);
    }
    void ndiSendsFrames() {
        StreamOutputManager m; m.setFrameRate(50);
        StreamConfig c; c.type = "ndi"; c.name = "LUMAFLOW Test"; c.groups = "stage"; c.enabled = true;
        m.setConfigs({c});
        const QImage f = testFrame(64, 36);
        for (int i = 0; i < 20; ++i) { m.submit(f, i / 50.0); QTest::qWait(10); }
        QTRY_VERIFY_WITH_TIMEOUT(mock->frames >= 5, 3000);
        QCOMPARE(QString(mock->name), QString("LUMAFLOW Test")); QCOMPARE(QString(mock->groups), QString("stage"));
        QCOMPARE(mock->xres, 64); QCOMPARE(mock->yres, 36); QCOMPARE(mock->stride, 64 * 4);
        QCOMPARE(mock->fourcc, int('R' | ('G' << 8) | ('B' << 16) | ('A' << 24)));
        QCOMPARE(mock->format, 1);                                         // progressive
        QCOMPARE(mock->timecode, std::numeric_limits<qint64>::max());     // synthesize
        QCOMPARE(mock->fps_n * 1.0 / mock->fps_d, 50.0);
        QCOMPARE(int(mock->first[0]), 255); QCOMPARE(int(mock->first[5]), 255);   // pixel 0 red, pixel 1 green (RGBA order)
        QCOMPARE(int(mock->last_row_first[2]), 255);                        // bottom-left blue: rows top-down
        QTRY_VERIFY_WITH_TIMEOUT(!m.status().isEmpty() && m.status()[0].state == "running", 2000);
        QCOMPARE(m.status()[0].connections, 2);
        const int destroyedBefore = mock->send_destroyed;
        m.setConfigs({});                                                   // removing the stream closes the sender
        QTRY_COMPARE_WITH_TIMEOUT(mock->send_destroyed, destroyedBefore + 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(mock->destroyed, mock->initialized, 2000);      // runtime released
    }
    void regionCropAndResize() {
        StreamOutputManager m;
        OutputRegion r; r.name = "left half"; r.source = QRectF(0, 0, 0.5, 1.0);
        m.setRegions({r});
        StreamConfig c; c.type = "ndi"; c.name = "Left"; c.enabled = true; c.region = 0;
        m.setConfigs({c});
        const int before = mock->frames;
        m.submit(testFrame(100, 50), 0);
        QTRY_VERIFY_WITH_TIMEOUT(mock->frames > before, 2000);
        QCOMPARE(mock->xres, 50); QCOMPARE(mock->yres, 50);
        const int created = mock->created;
        m.setRegions({}); c.region = -1; m.setConfigs({c});
        m.submit(testFrame(80, 40), 0);
        QTRY_VERIFY_WITH_TIMEOUT(mock->xres == 80, 2000);                  // size change re-creates the sender
        QVERIFY(mock->created > created);
    }
    void disabledAndUnavailable() {
        StreamOutputManager m;
        StreamConfig off; off.type = "ndi"; off.enabled = false;
        StreamConfig spout; spout.type = "spout"; spout.enabled = true;
        m.setConfigs({off, spout});
        const int before = mock->frames;
        m.submit(testFrame(32, 32), 0);
        QTRY_VERIFY_WITH_TIMEOUT(m.status().size() == 2 && m.status()[1].state.startsWith("error"), 2000);
        QCOMPARE(m.status()[0].state, QString("off"));
#ifndef LF_HAVE_SPOUT
        QVERIFY(m.status()[1].state.contains("Windows"));
#endif
        QCOMPARE(mock->frames, before);                                     // disabled stream never sends
    }
    void slowSinkDoesNotBlockSubmit() {
        // submit() must return immediately even while the stream thread is busy: frames are dropped, not queued
        StreamOutputManager m; StreamConfig c; c.type = "ndi"; c.enabled = true; m.setConfigs({c});
        const QImage big = testFrame(1920, 1080);
        QElapsedTimer t; t.start();
        for (int i = 0; i < 200; ++i) m.submit(big, i);
        QVERIFY2(t.elapsed() < 200, qPrintable(QString::number(t.elapsed())));
    }
    void pluginOutputModule() {
        QTemporaryDir dir;
        qputenv("LUMAFLOW_FRAMEGRAB_PATH", dir.filePath("grab.ppm").toLocal8Bit()); qputenv("LUMAFLOW_FRAMEGRAB_EVERY", "1");
        PluginHost host; host.setPluginDirs({LF_OUTPUT_PLUGIN_DIR});
        EffectLibrary lib; const auto recs = host.scanAndLoad(&lib, false);
        QCOMPARE(recs.size(), 1); QVERIFY(recs[0].loaded); QCOMPARE(recs[0].outputs, 1);
        StreamOutputManager m(&host);
        const QString type = "plugin:io.lumaflow.example.framegrab/framegrab";
        QVERIFY(StreamOutputManager::probe(type, &host).available);
        StreamConfig c; c.type = type; c.enabled = true; m.setConfigs({c});
        m.submit(testFrame(40, 20), 0);
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(dir.filePath("grab.ppm")), 3000);
        QTest::qWait(100);
        QImage ppm(dir.filePath("grab.ppm"));
        QCOMPARE(ppm.size(), QSize(40, 20)); QCOMPARE(ppm.pixelColor(0, 0), QColor(255, 0, 0));
    }
    void configJson() {
        StreamConfig c; c.type = "syphon"; c.name = "X"; c.enabled = true; c.region = 2; c.groups = "a,b";
        const StreamConfig d = StreamConfig::fromJson(c.toJson());
        QCOMPARE(d.type, c.type); QCOMPARE(d.name, c.name); QCOMPARE(d.region, 2); QCOMPARE(d.groups, c.groups); QVERIFY(d.enabled);
    }
};
QTEST_MAIN(TestStreams)
#include "test_streams.moc"
