// lumaflow-cli: headless tools for rendering, exporting, validating and testing.
#include "app/Engine.h"
#include "core/Log.h"
#include "fixtures/Gdtf.h"
#include "fixtures/Mvr.h"
#include "plugins/PluginHost.h"
#include "protocols/ArtNet.h"
#include "protocols/Sacn.h"
#include "storage/ProjectIO.h"
#include "renderer/GpuRenderer.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QTemporaryDir>
#include <cstdio>

using namespace lf;

static void out(const QString& s) { std::fprintf(stdout, "%s\n", s.toUtf8().constData()); std::fflush(stdout); }
static int fail(const QString& s) { std::fprintf(stderr, "error: %s\n", s.toUtf8().constData()); return 1; }

static EngineOptions headless(int w, int h) {
    EngineOptions o; o.headless = true; o.startRenderer = false; o.startNetwork = false; o.canvasWidth = w; o.canvasHeight = h;
    o.dataDir = QDir::temp().filePath("lumaflow-cli-data");
    return o;
}

// Deterministic offline render loop driving the GPU renderer directly.
struct Offline {
    Engine& e; GpuRenderer r; double fps;
    Offline(Engine& eng, double f) : e(eng), fps(f) { e.setOfflineClock(true); }
    bool init(QString* err) { return r.initialize(err); }
    RenderOutput step() { e.stepOffline(1.0 / fps); RenderInput in = e.buildRenderInput(1.0 / fps); in.qualityScale = 1.0; RenderOutput o = r.render(in); e.onRenderOutput(o); return o; }
};

int main(int argc, char** argv) {
    // plugin-probe must be minimal and never touch the GPU
    if (argc >= 3 && QString(argv[1]) == "plugin-probe") {
        QCoreApplication app(argc, argv);
        QJsonObject info; QString err;
        if (!PluginHost::probeInProcess(QString::fromLocal8Bit(argv[2]), info, &err)) return fail(err);
        out(QJsonDocument(info).toJson(QJsonDocument::Compact));
        return 0;
    }
#ifndef Q_OS_MACOS
    // Headless by default on Linux/Windows. On macOS the native cocoa plugin is used: it renders
    // to offscreen GL surfaces without opening a window, and it is the only platform plugin
    // macdeployqt bundles (the offscreen plugin has no OpenGL support on macOS).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QGuiApplication app(argc, argv);
    app.setApplicationName("LUMAFLOW"); app.setOrganizationName("LUMAFLOW"); app.setApplicationVersion(LUMAFLOW_VERSION);
    Log::instance().setEcho(qEnvironmentVariableIsSet("LUMAFLOW_VERBOSE"));
    QCommandLineParser p;
    p.setApplicationDescription("LUMAFLOW command-line tools");
    p.addHelpOption(); p.addVersionOption();
    p.addPositionalArgument("command", "info | effects | render-frame | render-video | render-all | benchmark | export-profiles | import-gdtf | import-mvr | export-mvr | validate-shader | compile-graph | demo-project | project-info | send-osc | send-artnet | artnet-monitor | ai | plugin-probe");
    QCommandLineOption oOut({"o", "out"}, "Output file or directory", "path");
    QCommandLineOption oEffect({"e", "effect"}, "Effect id", "id", "plasma");
    QCommandLineOption oW("width", "Canvas width", "px", "1280"), oH("height", "Canvas height", "px", "720");
    QCommandLineOption oTime({"t", "time"}, "Time / duration in seconds", "s", "2");
    QCommandLineOption oFps("fps", "Frames per second", "fps", "30");
    QCommandLineOption oProject("project", "Project file", "path");
    QCommandLineOption oFrames("frames", "Frame count", "n", "120");
    p.addOptions({oOut, oEffect, oW, oH, oTime, oFps, oProject, oFrames});
    p.process(app);
    const QStringList args = p.positionalArguments();
    const QString cmd = args.value(0, "info");
    const int W = p.value(oW).toInt(), H = p.value(oH).toInt();

    if (cmd == "info") {
        GpuRenderer r; QString err;
        const bool ok = r.initialize(&err);
        out(QString("LUMAFLOW %1").arg(LUMAFLOW_VERSION));
        out(QString("Qt %1, GPU: %2").arg(qVersion(), ok ? r.glInfo() : "unavailable (" + err + ")"));
#ifdef LF_HAVE_FFMPEG
        out("FFmpeg media decoding: yes");
#else
        out("FFmpeg media decoding: no");
#endif
        auto mb = midi::Backend::createDefault(); out("MIDI backend: " + mb->name() + (mb->available() ? " (available)" : " (unavailable)"));
        out("Audio inputs: " + AudioInput::devices().join(", "));
        return ok ? 0 : 2;
    }
    if (cmd == "effects") {
        EffectLibrary lib; lib.loadBuiltin();
        for (auto& e : lib.all()) out(QString("%1\t%2\t%3\t[%4, %5, %6, %7]%8").arg(e.id, e.category, e.name, e.params[0].name, e.params[1].name, e.params[2].name, e.params[3].name).arg(e.photosensitive ? "\tPHOTOSENSITIVE" : ""));
        return 0;
    }
    if (cmd == "render-frame" || cmd == "render-all") {
        Engine e(headless(W, H)); Offline off(e, p.value(oFps).toDouble()); QString err;
        if (!off.init(&err)) return fail(err);
        QStringList ids = cmd == "render-all" ? e.effects().ids() : QStringList{p.value(oEffect)};
        const QString dir = p.value(oOut).isEmpty() ? "." : p.value(oOut);
        if (cmd == "render-all") QDir().mkpath(dir);
        const int frames = std::max(1, int(p.value(oTime).toDouble() * p.value(oFps).toDouble()));
        for (auto& id : ids) {
            if (id == "text") e.setLayerText(0, "LUMAFLOW");
            e.setLayerEffect(0, id);
            for (int i = 1; i < 4; ++i) e.program().params.set(Composition::pid(i, "opacity"), 0);
            e.program().params.set("layer1.opacity", 1);
            RenderOutput o;
            for (int i = 0; i < frames; ++i) o = off.step();
            const QString path = cmd == "render-all" ? QDir(dir).filePath(id + ".png") : (p.value(oOut).isEmpty() ? id + ".png" : p.value(oOut));
            if (!o.program.save(path)) return fail("cannot save " + path);
            out(QString("%1 -> %2 (%3x%4, %5 ms/frame)").arg(id, path).arg(o.program.width()).arg(o.program.height()).arg(o.stats.frameMs, 0, 'f', 2));
        }
        const auto errs = off.r.lastStats().shaderErrors;
        for (auto& s : errs) out("SHADER ERROR: " + s);
        return errs.isEmpty() ? 0 : 3;
    }
    if (cmd == "benchmark") {
        Engine e(headless(W, H)); Offline off(e, 60); QString err;
        if (!off.init(&err)) return fail(err);
        out(QString("GPU: %1  canvas %2x%3").arg(off.r.glInfo()).arg(W).arg(H));
        QJsonArray results;
        const int n = p.value(oFrames).toInt();
        for (auto& id : e.effects().ids()) {
            e.setLayerEffect(0, id);
            for (int i = 1; i < 4; ++i) e.program().params.set(Composition::pid(i, "opacity"), 0);
            for (int i = 0; i < 3; ++i) off.step();              // warm-up incl. shader compile
            QElapsedTimer t; t.start(); double gpu = 0;
            for (int i = 0; i < n; ++i) gpu += off.step().stats.frameMs;
            const double ms = t.nsecsElapsed() / 1e6 / n;
            results << QJsonObject{{"effect", id}, {"msPerFrame", ms}, {"renderMs", gpu / n}, {"fps", 1000.0 / ms}};
            out(QString("%1\t%2 ms\t%3 fps").arg(id, -22).arg(ms, 7, 'f', 2).arg(1000.0 / ms, 7, 'f', 1));
        }
        if (!p.value(oOut).isEmpty()) { QFile f(p.value(oOut)); f.open(QIODevice::WriteOnly); f.write(QJsonDocument(QJsonObject{{"gpu", off.r.glInfo()}, {"width", W}, {"height", H}, {"results", results}}).toJson()); }
        return 0;
    }
    if (cmd == "render-video") {
        Engine e(headless(W, H)); Offline off(e, p.value(oFps).toDouble()); QString err;
        if (!off.init(&err)) return fail(err);
        if (p.isSet(oProject)) { QStringList w; if (!e.loadProject(p.value(oProject), &w, &err)) return fail(err); }
        else e.createDemoShow();
        e.timeline().seek(0); e.timeline().play();
        Recorder rec;
        const QString path = p.value(oOut).isEmpty() ? "lumaflow_render.mp4" : p.value(oOut);
        if (!rec.start(path, W, H, p.value(oFps).toDouble(), {}, &err)) return fail(err);
        const int frames = int(p.value(oTime).toDouble() * p.value(oFps).toDouble());
        for (int i = 0; i < frames; ++i) { RenderOutput o = off.step(); rec.addFrame(o.program); }
        if (!rec.finish(&err)) return fail(err);
        out(QString("rendered %1 frames to %2").arg(frames).arg(path));
        return 0;
    }
    if (cmd == "export-profiles") {
        Engine e(headless(W, H));
        e.createDemoShow();
        QString report;
        const QString dir = p.value(oOut).isEmpty() ? "fixture-profiles" : p.value(oOut);
        if (!e.exportConsoleFiles(dir, &report)) return fail(report);
        out(report);
        return 0;
    }
    if (cmd == "import-gdtf") {
        const auto r = gdtf::importFile(args.value(1));
        if (!r.ok()) return fail(r.error);
        QJsonObject o = r.type.toJson(); o["warnings"] = QJsonArray::fromStringList(r.warnings);
        QJsonArray caps; for (auto& m : r.type.modes) caps << QJsonObject{{"mode", m.name}, {"footprint", m.footprint()}, {"capabilities", analyzeCapabilities(m).describe().join(", ")}};
        o["capabilities"] = caps;
        out(QJsonDocument(o).toJson());
        return 0;
    }
    if (cmd == "import-mvr") {
        const auto r = mvr::importFile(args.value(1));
        if (!r.ok()) return fail(r.error);
        out(QString("%1 fixtures, %2 types, %3 groups").arg(r.patch.fixtures.size()).arg(r.patch.types.ids().size()).arg(r.groups.size()));
        for (auto& f : r.patch.fixtures) out(QString("  #%1 %2  %3.%4  pos(%5,%6,%7) m  %8 / %9").arg(f.fid).arg(f.name).arg(f.universe).arg(f.address).arg(f.position.x, 0, 'f', 2).arg(f.position.y, 0, 'f', 2).arg(f.position.z, 0, 'f', 2).arg(f.typeId, f.modeName));
        for (auto& w : r.warnings) out("  warning: " + w);
        return 0;
    }
    if (cmd == "export-mvr") {
        Engine e(headless(W, H)); e.createDemoShow(); QString err;
        const QString path = p.value(oOut).isEmpty() ? "demo_rig.mvr" : p.value(oOut);
        if (!mvr::exportFile(path, e.patch(), &err)) return fail(err);
        out("wrote " + path);
        return 0;
    }
    if (cmd == "validate-shader") {
        QFile f(args.value(1)); if (!f.open(QIODevice::ReadOnly)) return fail("cannot open shader");
        GpuRenderer r; QString err; if (!r.initialize(&err)) return fail(err);
        QString log; const bool ok = r.validateEffect(QString::fromUtf8(f.readAll()), &log);
        out(ok ? "OK" : "FAILED:\n" + log);
        return ok ? 0 : 4;
    }
    if (cmd == "compile-graph") {
        QFile f(args.value(1)); if (!f.open(QIODevice::ReadOnly)) return fail("cannot open graph");
        const NodeGraph g = NodeGraph::fromJson(QJsonDocument::fromJson(f.readAll()).object());
        const CompileResult r = g.compile();
        if (!r.ok) return fail(r.errors.join("\n"));
        out(r.glsl);
        return 0;
    }
    if (cmd == "demo-project") {
        Engine e(headless(W, H)); e.createDemoShow(); QString err;
        const QString path = p.value(oOut).isEmpty() ? "demo.lumaflow" : p.value(oOut);
        if (!e.saveProject(path, &err)) return fail(err);
        out("wrote " + path);
        return 0;
    }
    if (cmd == "project-info") {
        const LoadResult r = ProjectIO::load(args.value(1));
        if (!r.ok()) return fail(r.error);
        out(QString("schema %1 (file %2)").arg(r.doc["schemaVersion"].toInt()).arg(r.originalVersion));
        out(QString("scenes %1, presets %2, fixtures %3, pixel elements %4").arg(r.doc["scenes"].toArray().size()).arg(r.doc["presets"].toArray().size())
            .arg(r.doc["patch"].toObject()["fixtures"].toArray().size()).arg(r.doc["pixelMap"].toObject()["elements"].toArray().size()));
        for (auto& w : r.warnings) out("migration: " + w);
        return 0;
    }
    if (cmd == "send-osc") {
        QUdpSocket s; osc::Message m; m.address = args.value(3);
        for (int i = 4; i < args.size(); ++i) { bool ok; double d = args[i].toDouble(&ok); if (ok) m.args << (args[i].contains('.') ? QVariant(d) : QVariant(int(d))); else m.args << args[i]; }
        s.writeDatagram(osc::encode(m), QHostAddress(args.value(1)), quint16(args.value(2).toUInt()));
        out("sent " + m.toString());
        return 0;
    }
    if (cmd == "send-artnet") {
        // send-artnet <host> <universe(0-based port address)> <ch=value,...>
        QUdpSocket s; artnet::DmxPacket pk; pk.portAddress = quint16(args.value(2).toUInt()); pk.data = QByteArray(512, 0);
        for (auto& kv : args.value(3).split(',', Qt::SkipEmptyParts)) { const auto a = kv.split('='); pk.data[a[0].toInt() - 1] = char(a[1].toInt()); }
        for (int i = 0; i < 5; ++i) { pk.sequence = quint8(i + 1); s.writeDatagram(artnet::encodeDmx(pk), QHostAddress(args.value(1)), artnet::kPort); }
        out("sent ArtDmx");
        return 0;
    }
    if (cmd == "artnet-monitor") {
        QUdpSocket s;
        if (!s.bind(QHostAddress::AnyIPv4, artnet::kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) return fail("cannot bind 6454");
        const int seconds = int(p.value(oTime).toDouble());
        QObject::connect(&s, &QUdpSocket::readyRead, [&] {
            while (s.hasPendingDatagrams()) {
                const auto d = s.receiveDatagram(); artnet::DmxPacket pk;
                if (artnet::decodeDmx(d.data(), pk)) { QStringList first; for (int i = 0; i < 16; ++i) first << QString::number(quint8(pk.data[i])); out(QString("ArtDmx from %1 port %2 seq %3: %4 ...").arg(d.senderAddress().toString()).arg(pk.portAddress).arg(pk.sequence).arg(first.join(' '))); }
            }
        });
        QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    if (cmd == "ai") {
        EffectAssistant a; EffectLibrary lib; lib.loadBuiltin();
        const AiSuggestion s = a.generateLocal(args.mid(1).join(' '), lib.ids());
        out(QJsonDocument(s.toJson()).toJson());
        if (s.kind == AiSuggestion::NodeEffect) { const auto r = s.graph.compile(); out(r.ok ? "graph compiles: " + QString::number(r.nodeCount) + " nodes" : "graph errors: " + r.errors.join("; ")); }
        return 0;
    }
    return fail("unknown command '" + cmd + "' (see --help)");
}
