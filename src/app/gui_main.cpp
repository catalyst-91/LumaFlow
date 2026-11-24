// LUMAFLOW desktop application entry point.
#include "app/Engine.h"
#include "ui/MainWindow.h"
#include "ui/UiContext.h"
#include "ui/Widgets.h"
#include "core/Log.h"
#include "core/Resources.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDockWidget>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QScreen>
#include <QStyleFactory>
#include <QElapsedTimer>
#include <QTabWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QUdpSocket>
#include <QDateTime>
#include <cstdio>
#include "protocols/ArtNet.h"

using namespace lf;

// Plays a JSON script of timed UI actions with on-screen captions. Used to
// record the demonstration video from the real application.
class DemoDirector : public QObject {
public:
    DemoDirector(MainWindow* w, Engine* e, const QJsonArray& steps) : m_w(w), m_e(e), m_steps(steps) {
        m_caption = new QLabel(w);
        m_caption->setStyleSheet("background: rgba(10,10,20,215); color: white; font-size: 20px; padding: 10px 18px; border-radius: 8px; border: 1px solid #4b6bd6;");
        m_caption->setWordWrap(true); m_caption->hide();
        m_title = new QLabel(w);
        m_title->setStyleSheet("background: rgba(8,8,16,235); color: white; font-size: 44px; font-weight: bold; padding: 40px; border-radius: 16px; border: 2px solid #9a5cff;");
        m_title->setAlignment(Qt::AlignCenter); m_title->hide();
        m_clock.start();
        std::fprintf(stderr, "DEMO_START %lld\n", (long long)QDateTime::currentMSecsSinceEpoch()); std::fflush(stderr);
        connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
        m_timer.start(20);
    }
private:
    void tick() {
        const double t = m_clock.elapsed() / 1000.0;
        while (m_i < m_steps.size() && m_steps[m_i].toObject()["t"].toDouble() <= t) run(m_steps[m_i++].toObject());
        if (m_i >= m_steps.size() && t > m_end) { m_timer.stop(); QApplication::quit(); }
    }
    QWidget* findPanel(const QString& dock) { auto* d = m_w->dock(dock); return d ? d->widget() : nullptr; }
    void run(const QJsonObject& s) {
        const QString a = s["action"].toString();
        if (a == "caption") {
            const QString txt = s["text"].toString();
            if (txt.isEmpty()) { m_caption->hide(); return; }
            m_caption->setText(txt); m_caption->setFixedWidth(std::min(1100, m_w->width() - 80)); m_caption->adjustSize();
            m_caption->move((m_w->width() - m_caption->width()) / 2, m_w->height() - m_caption->height() - 46); m_caption->show(); m_caption->raise();
        } else if (a == "title") {
            const QString txt = s["text"].toString();
            if (txt.isEmpty()) { m_title->hide(); return; }
            m_title->setText(txt); m_title->setFixedSize(std::min(1300, m_w->width() - 100), s["height"].toInt(300));
            m_title->move((m_w->width() - m_title->width()) / 2, (m_w->height() - m_title->height()) / 2); m_title->show(); m_title->raise();
        } else if (a == "workspace") m_w->applyWorkspace(s["name"].toString());
        else if (a == "raise") { if (auto* d = m_w->dock(s["dock"].toString())) { d->show(); d->raise(); } }
        else if (a == "float") { if (auto* d = m_w->dock(s["dock"].toString())) { d->setFloating(true); d->resize(s["w"].toInt(900), s["h"].toInt(600)); d->move(m_w->x() + s["x"].toInt(200), m_w->y() + s["y"].toInt(150)); d->show(); d->raise(); } }
        else if (a == "unfloat") { if (auto* d = m_w->dock(s["dock"].toString())) d->setFloating(false); }
        else if (a == "tab") { if (auto* p = findPanel(s["dock"].toString())) if (auto* tw = p->findChild<QTabWidget*>()) tw->setCurrentIndex(s["index"].toInt()); }
        else if (a == "demoShow") m_e->createDemoShow();
        else if (a == "scene") m_e->recallScene(s["index"].toInt(), s["transition"].toDouble(-1));
        else if (a == "effect") { m_e->setLayerEffect(s["layer"].toInt(), s["id"].toString(), s["preview"].toBool()); if (s.contains("opacity")) (s["preview"].toBool() ? m_e->preview().params : m_e->params()).set(Composition::pid(s["layer"].toInt(), "opacity"), s["opacity"].toDouble()); }
        else if (a == "param") (s["preview"].toBool() ? m_e->preview().params : m_e->params()).set(s["id"].toString(), s["value"].toDouble(), "demo");
        else if (a == "ramp") { const QString id = s["id"].toString(); const double from = s["from"].toDouble(), to = s["to"].toDouble(), dur = s["duration"].toDouble(1); auto* t = new QElapsedTimer; t->start(); auto* tm = new QTimer(this);
            connect(tm, &QTimer::timeout, this, [=] { const double u = std::min(1.0, t->elapsed() / 1000.0 / dur); m_e->params().set(id, from + (to - from) * u, "demo"); if (u >= 1) { tm->stop(); tm->deleteLater(); delete t; } }); tm->start(30); }
        else if (a == "layer") m_w->context()->setLayer(s["index"].toInt());
        else if (a == "take") m_e->take(s["transition"].toDouble(-1));
        else if (a == "cue") m_e->cueGo();
        else if (a == "blackout") m_e->setBlackout(s["on"].toBool());
        else if (a == "timeline") { if (s["play"].toBool()) { m_e->timeline().seek(s["seek"].toDouble()); m_e->timeline().play(); } else m_e->timeline().pause(); }
        else if (a == "spatial") { QMutexLocker l(&m_e->stateMutex()); const int i = s["index"].toInt(); if (i < m_e->spatial().size()) { m_e->spatial()[i].enabled = s["enabled"].toBool(true); if (s.contains("order")) m_e->spatial()[i].fx.order = spatialOrderFromName(s["order"].toString()); if (s.contains("waveform")) m_e->spatial()[i].fx.waveform = waveformFromName(s["waveform"].toString()); } }
        else if (a == "pixelTest") m_e->setPixelTestPattern(s["pattern"].toInt());
        else if (a == "audio") { AudioSettings as = m_e->audioSettings(); as.device = s["device"].toString("test:128"); as.enabled = s["enabled"].toBool(true); m_e->applyAudioSettings(as); }
        else if (a == "aiPrompt") { if (auto* p = findPanel("AI Effect Assistant")) { if (auto* le = p->findChild<QLineEdit*>()) le->setText(s["text"].toString()); for (auto* b : p->findChildren<QPushButton*>()) if (b->text() == s["button"].toString("Generate (local)")) b->click(); } }
        else if (a == "click") { if (auto* p = findPanel(s["dock"].toString())) for (auto* b : p->findChildren<QPushButton*>()) if (b->text().startsWith(s["button"].toString())) { b->click(); break; } }
        else if (a == "osc") { osc::Message m; m.address = s["address"].toString(); for (auto v : s["args"].toArray()) m.args << v.toVariant(); m_e->network().sendOsc(m, QHostAddress::LocalHost, quint16(m_e->network().config().oscListenPort)); }
        else if (a == "network") { NetworkConfig nc = m_e->network().config(); nc.artnetOut = s["artnetOut"].toBool(); nc.sacnOut = s["sacnOut"].toBool(); nc.artnetIn = s["artnetIn"].toBool(); nc.artnetTarget = QHostAddress(s["target"].toString("127.0.0.1")); nc.inputUniverses = {1, 2}; m_e->applyNetworkConfig(nc); }
        else if (a == "console") { auto& c = m_e->consoles()[s["index"].toInt()]; c.enabled = s["enabled"].toBool(true); c.universe = s["universe"].toInt(c.universe); c.address = s["address"].toInt(c.address); }
        else if (a == "artnet") {
            // Simulated console: sends ArtDmx to this machine (labelled as simulation in the demo captions)
            artnet::DmxPacket pk; pk.portAddress = quint16(s["port"].toInt(0)); pk.data = QByteArray(512, 0);
            const QJsonObject ch = s["channels"].toObject();
            for (auto it = ch.begin(); it != ch.end(); ++it) pk.data[it.key().toInt() - 1] = char(it.value().toInt());
            static quint8 seq = 0;
            for (int i = 0; i < 3; ++i) { pk.sequence = ++seq; m_udp.writeDatagram(artnet::encodeDmx(pk), QHostAddress::LocalHost, artnet::kPort); }
        }
        else if (a == "brokenEffect") {
            EffectDef bad; bad.id = "broken_demo"; bad.name = "Broken shader (demo)"; bad.category = "Custom"; bad.source = "custom";
            bad.glsl = "vec4 effect(vec2 p, vec2 uv, float t) { return vec4(undefined_symbol, 1.0); }";
            m_e->effects().add(bad); m_e->setLayerEffect(s["layer"].toInt(1), "broken_demo"); m_e->params().set(Composition::pid(s["layer"].toInt(1), "opacity"), 1.0);
        }
        else if (a == "testPattern") m_e->setTestPattern(s["pattern"].toInt(-1));
        else if (a == "end") m_end = m_clock.elapsed() / 1000.0 + s["after"].toDouble(1.0);
        else if (a == "resize") { m_w->showNormal(); m_w->setGeometry(0, 0, s["w"].toInt(1920), s["h"].toInt(1080)); }
        else if (a == "layerSource") { LayerSource src = m_e->program().layers[s["layer"].toInt()]; if (s.contains("mask")) src.mask = MaskType(s["mask"].toInt()); if (s.contains("text")) src.text = s["text"].toString(); m_e->setLayerSource(s["layer"].toInt(), src); }
    }
    MainWindow* m_w; Engine* m_e; QJsonArray m_steps; int m_i = 0; QTimer m_timer; QElapsedTimer m_clock; QLabel* m_caption; QLabel* m_title; double m_end = 1e9; QUdpSocket m_udp;
};

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    app.setApplicationName("LUMAFLOW"); app.setOrganizationName("LUMAFLOW"); app.setApplicationVersion(LUMAFLOW_VERSION);
    app.setStyle(QStyleFactory::create("Fusion"));
    QCommandLineParser p;
    p.setApplicationDescription("LUMAFLOW - real-time visual effects, media server, pixel mapping and show control");
    p.addHelpOption(); p.addVersionOption();
    QCommandLineOption safe("safe-mode", "Start without plugins, custom effects, DMX output or remote control");
    QCommandLineOption demo("demo", "Load the demo show on startup");
    QCommandLineOption shots("screenshots", "Capture screenshots of every workspace/panel into <dir> and exit", "dir");
    QCommandLineOption script("demo-script", "Play a scripted demonstration (JSON)", "file");
    QCommandLineOption dataDir("data-dir", "Override the application data directory", "dir");
    QCommandLineOption size("window-size", "Initial window size WxH", "size");
    QCommandLineOption canvas("canvas", "Canvas resolution WxH", "size");
    p.addOptions({safe, demo, shots, script, dataDir, size, canvas});
    p.addPositionalArgument("project", "Project file to open", "[project.lumaflow]");
    p.process(app);
    initResources();
    app.setStyleSheet(darkStyleSheet());
    app.setWindowIcon(appIcon());

    EngineOptions opt; opt.safeMode = p.isSet(safe);
    if (p.isSet(dataDir)) opt.dataDir = p.value(dataDir);
    if (p.isSet(canvas)) { opt.canvasWidth = p.value(canvas).section('x', 0, 0).toInt(); opt.canvasHeight = p.value(canvas).section('x', 1, 1).toInt(); }
    Engine engine(opt);
    engine.start();
    MainWindow w(&engine);
    if (p.isSet(size)) { const QSize ws(p.value(size).section('x', 0, 0).toInt(), p.value(size).section('x', 1, 1).toInt()); w.setMaximumSize(ws); w.setGeometry(QRect(QPoint(0, 0), ws)); }
    if (p.isSet(script) || p.isSet(shots)) w.setAutomation(true);
    w.show();
    if (!p.positionalArguments().isEmpty()) w.openProject(p.positionalArguments().first());
    else if (p.isSet(demo) || p.isSet(shots)) engine.createDemoShow();
    else QTimer::singleShot(300, &w, [&w] { w.checkCrashRecovery(); });
    if (!engine.renderThread() || (QTimer::singleShot(4000, [&] { if (engine.renderThread() && !engine.renderThread()->initialised()) logError("app", "GPU renderer unavailable: " + engine.renderThread()->initError()); }), false)) {}

    DemoDirector* director = nullptr;
    if (p.isSet(script)) {
        QFile f(p.value(script));
        if (!f.open(QIODevice::ReadOnly)) { logError("app", "cannot open demo script"); return 2; }
        director = new DemoDirector(&w, &engine, QJsonDocument::fromJson(f.readAll()).array());
    }
    if (p.isSet(shots)) {
        // Screenshot automation for the manual: each entry = (file, workspace, docks to raise, setup)
        const QString dir = p.value(shots); QDir().mkpath(dir);
        struct Shot { QString file, workspace; QStringList raise; std::function<void()> setup; };
        auto* shotsList = new QVector<Shot>{
            {"01_main_live", "Live", {"Timeline", "Parameters"}, [&] { engine.recallScene(1, 0); }},
            {"02_effects_library", "Programming", {"Effects Library", "Parameters"}, [&] { engine.recallScene(2, 0); }},
            {"03_node_editor", "Programming", {"Node Editor"}, nullptr},
            {"04_pixel_mapper", "Mapping", {"Pixel Mapper", "Spatial Effects"}, [&] { engine.recallScene(0, 0); }},
            {"05_fixture_manager", "Mapping", {"Fixture Manager", "Output Configuration"}, nullptr},
            {"06_stage_3d", "Mapping", {"3D Stage", "Spatial Effects"}, [&] { QMutexLocker l(&engine.stateMutex()); if (engine.spatial().size() > 1) engine.spatial()[1].enabled = true; }},
            {"07_console_links", "Console", {"Console Mapping"}, nullptr},
            {"08_console_mapping", "Console", {"Console Mapping"}, [&] { if (auto* t = w.dock("Console Mapping")->widget()->findChild<QTabWidget*>()) t->setCurrentIndex(1); }},
            {"09_personality", "Console", {"Console Mapping"}, [&] { if (auto* t = w.dock("Console Mapping")->widget()->findChild<QTabWidget*>()) t->setCurrentIndex(3); }},
            {"10_output_config", "Console", {"Output Configuration"}, nullptr},
            {"11_timeline", "Live", {"Timeline", "Show Control"}, [&] { engine.timeline().seek(20); }},
            {"12_audio", "System", {"Audio", "Recovery"}, [&] { AudioSettings a; a.device = "test:128"; a.enabled = true; engine.applyAudioSettings(a); }},
            {"13_performance_recovery", "System", {"Performance Monitor", "Recovery"}, nullptr},
            {"14_calibration", "System", {"Calibration", "Plugin Manager"}, nullptr},
            {"15_remote_sync", "System", {"Remote & Sync"}, [&] { engine.startRemote(18090); }},
            {"16_ai_assistant", "Programming", {"AI Effect Assistant"}, [&] { if (auto* pnl = w.dock("AI Effect Assistant")->widget()) { if (auto* le = pnl->findChild<QLineEdit*>()) le->setText("Create a blue particle vortex"); for (auto* b : pnl->findChildren<QPushButton*>()) if (b->text() == "Generate (local)") b->click(); } }},
            {"17_media_library", "Programming", {"Media Library"}, nullptr},
            {"18_layers_compositing", "Live", {"Parameters", "3D Stage"}, [&] { w.context()->setLayer(1); }},
        };
        auto* idx = new int(0);
        auto* timer = new QTimer(&app);
        auto* phase = new int(0);
        QObject::connect(timer, &QTimer::timeout, [&, shotsList, idx, timer, phase, dir] {
            if (*idx >= shotsList->size()) { timer->stop(); QApplication::quit(); return; }
            Shot& s = (*shotsList)[*idx];
            if (*phase == 0) { w.applyWorkspace(s.workspace); for (auto& d : s.raise) if (auto* dk = w.dock(d)) { dk->show(); dk->raise(); } if (s.setup) s.setup(); *phase = 1; return; }
            if (*phase < 8) { ++*phase; return; }    // let the renderer produce frames and UI update
            w.grab().save(QDir(dir).filePath(s.file + ".png"));
            logInfo("app", "screenshot " + s.file);
            *phase = 0; ++*idx;
        });
        QTimer::singleShot(6000, [timer] { timer->start(350); });   // allow thumbnails/renderer to warm up
    }
    const int rc = app.exec();
    delete director;
    engine.shutdown();
    return rc;
}
