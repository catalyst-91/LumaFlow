#include <QtTest>
#include "timeline/Timeline.h"
#include "storage/ProjectIO.h"
#include "nodes/NodeGraph.h"
#include "audio/AudioAnalyzer.h"
#include "ai/EffectAssistant.h"
#include "effects/EffectLibrary.h"
#include "core/Parameters.h"
using namespace lf;

class TestShow : public QObject {
    Q_OBJECT
private slots:
    void keyframes() {
        Track t; t.target = "x"; t.addKey(0, 0); t.addKey(10, 1); t.addKey(20, 0, Curve::Step);
        QCOMPARE(t.evaluate(5), 0.5); QCOMPARE(t.evaluate(-1), 0.0); QCOMPARE(t.evaluate(25), 0.0);
        t.keys[0].curve = Curve::EaseInOut; QVERIFY(t.evaluate(2) < 0.2);
        t.addKey(10, 0.8); QCOMPARE(t.keys.size(), 3); QCOMPARE(t.evaluate(10), 0.8);
    }
    void timelineEventsAndLoop() {
        Timeline tl; tl.duration = 10; tl.loop = true; tl.loopIn = 2; tl.loopOut = 6;
        Track ev; ev.events << TimelineEvent{1, "a", {}, ""} << TimelineEvent{3, "b", {}, ""} << TimelineEvent{7, "c", {}, ""};
        Track au; au.target = "p"; au.addKey(0, 0); au.addKey(10, 10);
        tl.tracks << ev << au;
        QStringList fired; QMap<QString, double> vals;
        auto w = [&](const QString& id, double v) { vals[id] = v; };
        auto h = [&](const TimelineEvent& e) { fired << e.action; };
        tl.play();
        for (int i = 0; i < 80; ++i) tl.advance(0.1, w, h);     // 8 s with loop 2..6
        QVERIFY(fired.count("a") == 1); QVERIFY(fired.count("b") >= 2); QCOMPARE(fired.count("c"), 0);
        QVERIFY(tl.position() >= 2 && tl.position() < 6);
        QVERIFY(qAbs(vals["p"] - tl.position()) < 1e-6);
        QCOMPARE(tl.snap(0.26, 1.0), 0.5);                       // 120 BPM grid
        const Timeline back = Timeline::fromJson(tl.toJson()); QCOMPARE(back.tracks.size(), 2); QCOMPARE(back.loopOut, 6.0);
    }
    void cueListFollow() {
        CueList cl;
        for (int i = 0; i < 3; ++i) { Cue c; c.number = i + 1; c.fadeTime = 1; cl.cues << c; }
        cl.cues[0].follow = Cue::AutoFollow; cl.cues[1].follow = Cue::Wait; cl.cues[1].followTime = 2;
        QCOMPARE(cl.go()->number, 1.0); cl.markFired(0);
        QVERIFY(!cl.update(0.5)); QCOMPARE(cl.update(1.01)->number, 2.0); cl.markFired(1.01);
        QVERIFY(!cl.update(2.5)); QCOMPARE(cl.update(4.1)->number, 3.0);
        QCOMPARE(cl.back()->number, 2.0); QCOMPARE(cl.gotoCue(1)->number, 1.0); QVERIFY(!cl.gotoCue(9));
    }
    void projectRoundTripAndBackups() {
        QTemporaryDir dir;
        const QString path = dir.filePath("show.lumaflow"), bk = dir.filePath("backups");
        QJsonObject doc{{"scenes", QJsonArray{QJsonObject{{"name", "A"}}}}, {"composition", QJsonObject{{"values", QJsonObject{{"master.intensity", 0.5}}}}}};
        QString err; QVERIFY2(ProjectIO::save(path, doc, &err, bk, 3), qPrintable(err));
        for (int i = 0; i < 5; ++i) { QThread::msleep(5); doc["n"] = i; QVERIFY(ProjectIO::save(path, doc, &err, bk, 3)); }
        QCOMPARE(ProjectIO::listBackups(bk, "show").size(), 3);  // retention
        const LoadResult r = ProjectIO::load(path);
        QVERIFY(r.ok()); QCOMPARE(r.doc["n"].toInt(), 4); QCOMPARE(r.doc["schemaVersion"].toInt(), kProjectSchemaVersion);
        const QString oldest = ProjectIO::listBackups(bk, "show").last();
        QVERIFY(ProjectIO::restoreBackup(oldest, path, &err));
        QVERIFY(ProjectIO::load(path).doc["n"].toInt() < 4);
        QVERIFY(QFile::exists(path + ".before-restore"));
    }
    void migrationAndSafety() {
        const QByteArray v1 = R"({"format":"lumaflow-project","schemaVersion":1,"layers":[{"effect":"plasma"}],"values":{"master.intensity":0.3},"cues":[{"number":1}],"artnet":{"enabled":true,"universes":[1,2]}})";
        const LoadResult r = ProjectIO::parse(v1);
        QVERIFY(r.ok()); QCOMPARE(r.originalVersion, 1);
        QVERIFY(r.doc.contains("composition")); QVERIFY(!r.doc.contains("layers"));
        QCOMPARE(r.doc["cueLists"].toArray().size(), 1);
        QCOMPARE(r.doc["network"].toObject()["outputUniverses"].toArray().size(), 2);
        QVERIFY(r.warnings.size() >= 3);
        const LoadResult future = ProjectIO::parse(R"({"format":"lumaflow-project","schemaVersion":99})");
        QVERIFY(!future.ok()); QVERIFY(future.error.contains("newer"));
        QVERIFY(!ProjectIO::parse("{broken").ok());
        QVERIFY(!ProjectIO::parse(R"({"format":"lfpreset"})").ok());
    }
    void dependencyCheck() {
        QJsonObject doc{{"composition", QJsonObject{{"layers", QJsonArray{QJsonObject{{"type", 0}, {"effect", "ghost"}}, QJsonObject{{"type", 1}, {"media", "/nonexistent/clip.mp4"}}}}}},
                        {"patch", QJsonObject{{"fixtures", QJsonArray{QJsonObject{{"type", "unresolved:Foo.gdtf"}}}}}}, {"plugins", QJsonArray{QJsonObject{{"id", "acme.x"}}}}};
        const auto d = ProjectIO::checkDependencies(doc, {"plasma"}, {}, "/");
        QCOMPARE(d.missingEffects, QStringList{"ghost"}); QCOMPARE(d.missingMedia.size(), 1); QCOMPARE(d.unresolvedFixtureTypes.size(), 1); QCOMPARE(d.missingPlugins.size(), 1);
        QTemporaryDir dir; QFile f(dir.filePath("clip.mp4")); f.open(QIODevice::WriteOnly); f.write("x"); f.close();
        QCOMPARE(ProjectIO::relinkMedia(doc, dir.path()), 1);
        QVERIFY(ProjectIO::checkDependencies(doc, {"plasma"}, {}, "/").missingMedia.isEmpty());
    }
    void nodeGraphCompile() {
        const NodeGraph g = NodeGraph::defaultGraph();
        const auto r = g.compile();
        QVERIFY2(r.ok, qPrintable(r.errors.join("; ")));
        QVERIFY(r.glsl.contains("vec4 effect(vec2 p, vec2 uv, float t)"));
        QCOMPARE(r.exposedParams[0], QString("Warp"));
        const NodeGraph back = NodeGraph::fromJson(g.toJson()); QCOMPARE(back.compile().glsl, r.glsl);
        QCOMPARE(back.structureHash(), g.structureHash());
    }
    void nodeGraphValidation() {
        NodeGraph g;
        const QString uv = g.addNode("uv"), col = g.addNode("color"), out = g.addNode("output"), add = g.addNode("add"), mul = g.addNode("mul");
        QVERIFY(!g.connect(uv, "p", out, "color").isEmpty());          // vec2 -> colour rejected
        QVERIFY(g.connect(col, "color", out, "color").isEmpty());
        QVERIFY(g.connect(add, "value", mul, "a").isEmpty());
        QVERIFY(g.connect(mul, "value", add, "a").contains("cycle"));  // cycle rejected
        QVERIFY(g.compile().ok);
        g.connections << Connection{mul, "value", add, "a"};            // force a cycle into the data
        QVERIFY(g.validate().join(' ').contains("cycle"));
        NodeGraph none; none.addNode("uv"); QVERIFY(none.validate().join(' ').contains("no Output"));
    }
    void nodeGraphCustomAndSubgraph() {
        NodeGraph g = NodeGraph::defaultGraph();
        QStringList sel; for (auto& n : g.nodes) if (n.type == "noise" || n.type == "rings" || n.type == "mul" || n.type == "palette") sel << n.id;
        const QString sub = g.makeSubgraph(sel, "Pattern");
        QVERIFY(!sub.isEmpty());
        const auto r = g.compile(); QVERIFY2(r.ok, qPrintable(r.errors.join("; ")));
        NodeGraph c; const QString gl = c.addNode("glsl"), o = c.addNode("output"); c.connect(gl, "color", o, "color");
        QVERIFY(c.compile().ok);
        c.node(gl)->code = "vec3(1.0); discard";
        QVERIFY(!c.compile().ok);
        // copy / paste
        NodeGraph d = NodeGraph::defaultGraph(); const int n = int(d.nodes.size());
        QStringList all; for (auto& x : d.nodes) all << x.id;
        d.paste(d.copy(all), {50, 50});
        QCOMPARE(d.nodes.size(), qsizetype(n * 2 - 1));               // second Output skipped
    }
    void fftAndBeats() {
        std::vector<std::complex<float>> a(1024);
        for (int i = 0; i < 1024; ++i) a[i] = std::sin(2 * M_PI * 64 * i / 1024.0);
        fft(a);
        int peak = 0; for (int k = 1; k < 512; ++k) if (std::abs(a[k]) > std::abs(a[peak])) peak = k;
        QCOMPARE(peak, 64);
        // synthetic kick at 120 BPM for 8 s
        AudioAnalyzer an(48000, 1024);
        std::vector<float> s(48000 * 8);
        for (size_t i = 0; i < s.size(); ++i) { const double t = double(i) / 48000, ph = std::fmod(t, 0.5); s[i] = float(0.8 * std::sin(2 * M_PI * (50 + 100 * std::exp(-ph * 30)) * ph) * std::exp(-ph * 9)); }
        for (size_t i = 0; i < s.size(); i += 480) an.process(s.data() + i, 480);
        const AudioFeatures f = an.features();
        QVERIFY2(f.beatCount >= 12, qPrintable(QString("beats %1").arg(f.beatCount)));
        QVERIFY2(qAbs(f.bpm - 120) < 3, qPrintable(QString("bpm %1").arg(f.bpm)));
        QVERIFY(f.bands[1] > f.bands[5]);                              // bass dominant
        an.setNoiseGate(1.0f); an.reset(); an.process(s.data(), 48000); QCOMPARE(an.features().beatCount, quint64(0));
    }
    void wavIo() {
        QTemporaryDir dir; std::vector<float> s(4800); for (size_t i = 0; i < s.size(); ++i) s[i] = float(std::sin(i * 0.1) * 0.5);
        QVERIFY(writeWav(dir.filePath("a.wav"), s, 48000));
        std::vector<float> back; int sr = 0; QVERIFY(readWav(dir.filePath("a.wav"), back, sr));
        QCOMPARE(sr, 48000); QCOMPARE(back.size(), s.size()); QVERIFY(qAbs(back[100] - s[100]) < 1e-3);
    }
    void aiLocal() {
        EffectLibrary lib; lib.loadBuiltin();
        EffectAssistant a;
        auto s = a.generateLocal("Create a blue particle vortex", lib.ids());
        QCOMPARE(int(s.kind), int(AiSuggestion::LayerPreset)); QCOMPARE(s.effectId, QString("particle_vortex")); QVERIFY(s.colorA[2] > 0.9);
        s = a.generateLocal("Generate a red and white centre-out chase", lib.ids());
        QCOMPARE(int(s.kind), int(AiSuggestion::Spatial)); QCOMPARE(int(s.spatial.order), int(SpatialOrder::CentreOut)); QCOMPARE(int(s.spatial.waveform), int(Waveform::Chase));
        QCOMPARE(s.spatial.colorA.r, 1.0); QCOMPARE(s.spatial.colorB.g, 1.0);
        s = a.generateLocal("Create an audio-reactive plasma effect", lib.ids());
        QCOMPARE(s.effectId, QString("plasma")); QCOMPARE(s.layerValues["audio"].toDouble(), 0.8);
        s = a.generateLocal("Generate a symmetrical effect using fixture positions", lib.ids());
        QCOMPARE(int(s.spatial.order), int(SpatialOrder::Symmetrical));
        s = a.generateLocal("Create a slow colour wave across LED strips", lib.ids());
        QVERIFY(s.spatial.colorSweep); QVERIFY(s.spatial.speed < 0.5);
        s = a.generateLocal("combine plasma with particle rings and music", lib.ids());
        QCOMPARE(int(s.kind), int(AiSuggestion::NodeEffect)); QVERIFY(s.graph.compile().ok);
        QVERIFY(EffectAssistant::repairGlsl("vec4 effect(vec2 p, vec2 uv, float t){ gl_FragColor = texture2D(uTex, uv); }", "").contains("texture(uTex"));
    }
    void parameterRegistry() {
        ParameterRegistry r; ParameterDef d; d.id = "x"; d.minimum = 0; d.maximum = 10; d.defaultValue = 5; r.define(d);
        int calls = 0; r.addListener([&](const QString&, double, const QString&) { ++calls; });
        QVERIFY(r.set("x", 20)); QCOMPARE(r.value("x"), 10.0); QVERIFY(r.setNormalized("x", 0.25)); QCOMPARE(r.value("x"), 2.5);
        QVERIFY(!r.set("nope", 1)); QCOMPARE(calls, 2); r.set("x", 2.5); QCOMPARE(calls, 2);
    }
};
QTEST_GUILESS_MAIN(TestShow)
#include "test_show.moc"
