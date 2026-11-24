// GPU + engine tests. Require an OpenGL 3.3 context (run under a display or Xvfb).
#include <QtTest>
#include "app/Engine.h"
#include "renderer/GpuRenderer.h"
#include "core/Log.h"
#include <QLibrary>
using namespace lf;

static double meanLuma(const QImage& img) {
    double s = 0; const QImage i = img.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < i.height(); y += 4) for (int x = 0; x < i.width(); x += 4) s += qGray(i.pixel(x, y));
    return s / ((i.height() / 4) * (i.width() / 4));
}

class TestEngineGpu : public QObject {
    Q_OBJECT
    std::unique_ptr<Engine> e;
    GpuRenderer r;
    RenderOutput step(int n = 1) { RenderOutput o; for (int i = 0; i < n; ++i) { e->stepOffline(1 / 30.0); o = r.render(e->buildRenderInput(1 / 30.0)); e->onRenderOutput(o); } return o; }
private slots:
    void initTestCase() {
        Log::instance().setEcho(false);
        EngineOptions o; o.headless = true; o.startRenderer = false; o.startNetwork = false; o.canvasWidth = 320; o.canvasHeight = 180;
        o.dataDir = QDir::temp().filePath("lf-test-gpu");
        QDir(o.dataDir).removeRecursively();
        e = std::make_unique<Engine>(o);
        e->setOfflineClock(true);
        QString err;
        if (!r.initialize(&err)) QSKIP(qPrintable("no OpenGL 3.3: " + err));
    }
    void rendersNonBlack() {
        e->setLayerEffect(0, "plasma"); for (int i = 1; i < 4; ++i) e->program().params.set(Composition::pid(i, "opacity"), 0);
        const auto o = step(3);
        QCOMPARE(o.program.size(), QSize(320, 180));
        QVERIFY(meanLuma(o.program) > 20);
        QVERIFY(o.stats.shaderErrors.isEmpty());
    }
    void masterAndBlackout() {
        e->program().params.set("master.intensity", 0.0);
        QVERIFY(meanLuma(step().program) < 1);
        e->program().params.set("master.intensity", 1.0);
        e->setBlackout(true); QVERIFY(meanLuma(step().program) < 1);
        e->setBlackout(false); QVERIFY(meanLuma(step().program) > 20);
    }
    void invalidShaderIsolated() {
        EffectDef bad; bad.id = "broken"; bad.name = "Broken"; bad.source = "custom"; bad.glsl = "vec4 effect(vec2 p, vec2 uv, float t) { return syntax error; }";
        e->effects().add(bad);                       // bypass validation on purpose
        e->setLayerEffect(1, "plasma"); e->setLayerEffect(0, "broken");
        e->program().params.set("layer1.opacity", 1); e->program().params.set("layer2.opacity", 1);
        const auto o = step(2);
        QVERIFY(!o.stats.shaderErrors.isEmpty());   // error reported...
        QVERIFY(meanLuma(o.program) > 20);          // ...but layer 2 still renders, no crash
        QVERIFY(!r.validateEffect(bad.glsl, nullptr));
        QString log; QVERIFY(r.validateEffect(e->effects().get("plasma").glsl, &log));
        QVERIFY(!r.validateEffect("vec4 effect(vec2 p, vec2 uv, float t) { while (true) {} return vec4(1); }", &log));
        QVERIFY(log.contains("unbounded"));
    }
    void lastGoodProgramKept() {
        EffectDef v1; v1.id = "edit"; v1.source = "custom"; v1.glsl = "vec4 effect(vec2 p, vec2 uv, float t) { return vec4(1.0); }";
        e->effects().add(v1);
        e->setLayerEffect(0, "edit"); e->program().params.set("layer2.opacity", 0);
        QVERIFY(meanLuma(step().program) > 200);
        v1.glsl = "vec4 effect(vec2 p, vec2 uv, float t) { return oops; }"; e->effects().add(v1);
        QVERIFY(meanLuma(step().program) > 200);     // previous working version still displayed
    }
    void layersBlendAndMask() {
        e->setLayerEffect(0, "solid"); e->setLayerColors(0, {1, 0, 0}, {0, 0, 0});
        e->setLayerEffect(1, "solid"); e->setLayerColors(1, {0, 0, 1}, {0, 0, 0});
        e->program().params.set("layer1.opacity", 1); e->program().params.set("layer2.opacity", 1);
        e->program().params.set("layer2.blend", int(BlendMode::Additive));
        QImage img = step().program; QRgb c = img.pixel(160, 90);
        QVERIFY(qRed(c) > 240 && qBlue(c) > 240);                                // additive red + blue = magenta
        e->program().params.set("layer2.blend", int(BlendMode::Multiply));
        c = step().program.pixel(160, 90); QVERIFY(qRed(c) < 10 && qBlue(c) < 10);  // red * blue = black
        e->program().params.set("layer2.blend", int(BlendMode::Alpha));
        LayerSource src = e->program().layers[1]; src.mask = MaskType::Circle; src.maskSize = 0.3; src.maskFeather = 0.01; e->setLayerSource(1, src);
        img = step().program;
        QVERIFY(qBlue(img.pixel(160, 90)) > 240);                                  // inside circle: blue
        QVERIFY(qRed(img.pixel(5, 5)) > 240 && qBlue(img.pixel(5, 5)) < 10);       // outside: red layer shows
        src.mask = MaskType::None; src.solo = true; e->setLayerSource(1, src);
        QVERIFY(qRed(step().program.pixel(5, 5)) < 10);                            // solo hides layer 1
        src.solo = false; e->setLayerSource(1, src);
    }
    void sceneTransitionCrossfade() {
        e->program().params.set("layer2.opacity", 0);
        e->setLayerEffect(0, "solid"); e->setLayerColors(0, {1, 1, 1}, {0, 0, 0});
        const int s1 = e->storeScene("white");
        e->setLayerColors(0, {0, 0, 0}, {0, 0, 0});
        const int s2 = e->storeScene("black");
        e->recallScene(s1, 0); QVERIFY(meanLuma(step().program) > 240);
        e->recallScene(s2, 1.0);
        step(15);                                                                   // 0.5 s into a 1 s fade
        const double mid = meanLuma(e->lastProgramFrame());
        QVERIFY2(mid > 80 && mid < 180, qPrintable(QString::number(mid)));
        step(20); QVERIFY(meanLuma(e->lastProgramFrame()) < 5);
    }
    void previewTake() {
        e->setLayerEffect(0, "solid", true); 
        LayerSource s = e->preview().layers[0]; s.colorA = {0, 1, 0}; e->setLayerSource(0, s, true);
        e->preview().params.set("layer1.opacity", 1);
        const auto o = step();
        QVERIFY(qGreen(o.preview.pixel(80, 45)) > 240);
        QVERIFY(qGreen(o.program.pixel(160, 90)) < 10);
        e->take(0); QVERIFY(qGreen(step().program.pixel(160, 90)) > 240);
    }
    void strobeSafetyLimiter() {
        e->setLayerEffect(0, "solid"); e->setLayerColors(0, {1, 1, 1}, {0, 0, 0});
        e->program().params.set("master.strobe", 1.0);
        e->safety().maxStrobeHz = 3.0;
        int transitions = 0; bool last = true;
        for (int i = 0; i < 60; ++i) { const bool on = meanLuma(step().program) > 128; if (on != last) ++transitions; last = on; }   // 2 s
        QVERIFY2(transitions <= 2 * 3 * 2 + 1, qPrintable(QString::number(transitions)));    // <= 3 flashes/s
        QVERIFY(transitions >= 4);
        e->program().params.set("master.strobe", 0.0);
    }
    void calibration() {
        e->setLayerEffect(0, "solid"); e->setLayerColors(0, {1, 1, 1}, {0, 0, 0});
        e->calibration().gainB = 0.0;
        QVERIFY(qBlue(step().program.pixel(10, 10)) < 5);
        e->calibration().gainB = 1.0; e->safety().maxBrightness = 0.5;
        const int v = qRed(step().program.pixel(10, 10)); QVERIFY(v >= 125 && v <= 130);
        e->safety().maxBrightness = 1.0;
    }
    void textAndMediaLayers() {
        e->setLayerEffect(0, "text"); e->setLayerText(0, "HELLO");
        LayerSource s = e->program().layers[0]; s.colorA = {1, 1, 1}; e->setLayerSource(0, s);
        QVERIFY(meanLuma(step().program) > 5);
        QTemporaryDir dir; QImage img(64, 32, QImage::Format_RGB32); img.fill(qRgb(0, 255, 0)); img.save(dir.filePath("g.png"));
        e->setLayerMedia(0, dir.filePath("g.png"));
        const QImage out = step(2).program;
        QVERIFY(qGreen(out.pixel(160, 90)) > 240 && qRed(out.pixel(160, 90)) < 10);
    }
    void nodeEffectOnGpu() {
        const NodeGraph g = NodeGraph::defaultGraph();
        const auto cr = g.compile(); QVERIFY(cr.ok);
        EffectDef d; d.id = "node_test"; d.glsl = cr.glsl; d.source = "custom"; e->effects().add(d);
        QString log; QVERIFY2(r.validateEffect(cr.glsl, &log), qPrintable(log));
        e->setLayerEffect(0, "node_test");
        const auto o = step(2); QVERIFY(meanLuma(o.program) > 5); QVERIFY(o.stats.shaderErrors.filter("node_test").isEmpty());
    }
    void pixelMapFromRender() {
        e->setLayerEffect(0, "solid"); e->setLayerColors(0, {1, 0, 0}, {0, 0, 0});
        e->pixelMap().elements.clear();
        e->pixelMap().elements << PixelMap::makeStrip("strip", {0.1, 0.5}, {0.9, 0.5}, 10, 5, 1);
        e->pixelMap().globalGamma = 1.0;
        step();
        const auto dmx = e->lastDmxOutput();
        QVERIFY(dmx.contains(5)); QCOMPARE(int(dmx[5][0]), 255); QCOMPARE(int(dmx[5][1]), 0); QCOMPARE(int(dmx[5][27]), 255);
        QVERIFY(!dmx.contains(6));
    }
    void demoShowAndProject() {
        e->createDemoShow();
        QVERIFY(e->scenes().size() == 6); QVERIFY(e->patch().validate().isEmpty());
        QVERIFY(e->pixelMap().analyze(e->patch()).conflicts.isEmpty());
        step(3);
        const auto dmx = e->lastDmxOutput();
        QVERIFY(dmx.contains(1) && dmx.contains(2) && dmx.contains(3) && dmx.contains(4));
        QTemporaryDir dir; QString err;
        QVERIFY(e->saveProject(dir.filePath("demo.lumaflow"), &err));
        const QJsonObject before = e->toProjectJson();
        e->newProject(); QVERIFY(e->scenes().isEmpty());
        QStringList w; QVERIFY(e->loadProject(dir.filePath("demo.lumaflow"), &w, &err));
        QCOMPARE(e->scenes().size(), 6); QCOMPARE(e->patch().fixtures.size(), before["patch"].toObject()["fixtures"].toArray().size());
        QCOMPARE(e->toProjectJson()["composition"], before["composition"]);
        QVERIFY(!e->dirty());
    }
    void streamOutputsFromRenderer() {
        qputenv("LUMAFLOW_NDI_LIB", LF_MOCK_NDI);
        QLibrary lib(LF_MOCK_NDI); lib.setLoadHints(QLibrary::PreventUnloadHint); QVERIFY(lib.load());
        auto* frames = reinterpret_cast<int*>(reinterpret_cast<char*>(reinterpret_cast<void* (*)()>(lib.resolve("mock_ndi_state"))()) + 4 * sizeof(int));
        StreamConfig c; c.type = "ndi"; c.name = "Engine Program"; c.enabled = true;
        e->applyStreamConfigs({c});
        const int before = *frames;
        for (int i = 0; i < 10; ++i) { step(); QTest::qWait(15); }
        QTRY_VERIFY_WITH_TIMEOUT(*frames > before, 3000);              // rendered program frames reach the NDI sender
        QTemporaryDir dir; QString err; QVERIFY(e->saveProject(dir.filePath("s.lumaflow"), &err));
        e->applyStreamConfigs({}); QVERIFY(e->streams().configs().isEmpty());
        QStringList w; QVERIFY(e->loadProject(dir.filePath("s.lumaflow"), &w, &err));
        QCOMPARE(e->streams().configs().size(), 1); QCOMPARE(e->streams().configs()[0].name, QString("Engine Program"));
        e->applyStreamConfigs({});
    }
    void consoleExports() {
        QTemporaryDir dir; QString rep;
        QVERIFY2(e->exportConsoleFiles(dir.path(), &rep), qPrintable(rep));
        QVERIFY(QFile::exists(dir.filePath("LUMAFLOW_MediaServer.gdtf")));
        QVERIFY(QFile::exists(dir.filePath("magicq/LUMAFLOW_Standard_channels.csv")));
        QVERIFY(rep.contains("footprints match"));
        // re-export into the same folder must overwrite the console copies (regression)
        QFile f(dir.filePath("grandma3/LUMAFLOW_MediaServer.gdtf")); f.open(QIODevice::WriteOnly); f.write("stale"); f.close();
        QVERIFY(e->exportConsoleFiles(dir.path(), &rep));
        QFile a(dir.filePath("LUMAFLOW_MediaServer.gdtf")), b(dir.filePath("grandma3/LUMAFLOW_MediaServer.gdtf"));
        a.open(QIODevice::ReadOnly); b.open(QIODevice::ReadOnly); QCOMPARE(a.readAll(), b.readAll());
    }
};
QTEST_MAIN(TestEngineGpu)
#include "test_engine_gpu.moc"
