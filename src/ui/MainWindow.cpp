#include "MainWindow.h"
#include "UiContext.h"
#include "Widgets.h"
#include "ShowPanels.h"
#include "EditorPanels.h"
#include "StagePanels.h"
#include "ConsolePanel.h"
#include "SystemPanels.h"
#include "app/Engine.h"
#include "renderer/GpuRenderer.h"
#include "storage/ProjectIO.h"
#include "core/Log.h"
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QInputDialog>
#include <QPushButton>
#include <QSlider>
#include <QDesktopServices>
#include <QUrl>
#include <QCryptographicHash>
#include <QPainter>

namespace lf {

// ---------------------------------------------------------------- thumbnails
void ThumbnailWorker::run() {
    GpuRenderer r; QString err;
    if (!r.initialize(&err)) { logWarn("ui", "thumbnail renderer unavailable: " + err); return; }
    QDir().mkpath(m_dir);
    QHash<QString, QVector<QImage>> out;
    const auto effects = m_e->effects().all();
    const int frames = 8, W = 128, H = 72;
    for (const auto& e : effects) {
        if (isInterruptionRequested()) return;
        const QString key = QCryptographicHash::hash(e.glsl.toUtf8(), QCryptographicHash::Md5).toHex().left(10);
        const QString file = QDir(m_dir).filePath(e.id + "_" + key + ".png");
        QImage strip(file);
        if (strip.isNull()) {
            strip = QImage(W * frames, H, QImage::Format_RGB32);
            QPainter p(&strip);
            for (int f = 0; f < frames; ++f) {
                RenderInput in; in.width = W; in.height = H; in.renderPreview = false;
                LayerSnapshot s; s.visible = true; s.programKey = e.id; s.glsl = e.glsl; s.glslHash = qHash(e.glsl); s.time = 1.0f + f * 0.25f;
                for (int k = 0; k < 4; ++k) s.p[k] = float(e.params[k].defaultValue);
                s.colorA = {float(e.colorA[0]), float(e.colorA[1]), float(e.colorA[2])}; s.colorB = {float(e.colorB[0]), float(e.colorB[1]), float(e.colorB[2])};
                if (e.id == "text") { QImage t(400, 100, QImage::Format_RGBA8888); t.fill(Qt::transparent); QPainter tp(&t); tp.setPen(Qt::white); QFont fo("Sans", 60, QFont::Bold); tp.setFont(fo); tp.drawText(t.rect(), Qt::AlignCenter, "TEXT"); tp.end(); s.texture = t; }
                if (e.id == "logo") s.texture = QImage(":/lumaflow/icons/lumaflow.png");
                if (e.id == "spectrum" || e.id == "waveform" || e.id == "audio_particles") { for (int k = 0; k < 32; ++k) in.spectrum[k] = 0.3f + 0.6f * std::abs(std::sin(k * 0.4f + f)); in.audio = {0.7f, 0.5f, 0.4f, 0.5f}; in.beat = f % 4 == 0 ? 1.f : 0.3f; }
                if (e.id == "strobe") s.p[0] = 0.0f;
                in.program.layers << s;
                const RenderOutput o = r.render(in);
                p.drawImage(f * W, 0, o.program);
            }
            p.end();
            strip.save(file);
        }
        QVector<QImage> fr; for (int f = 0; f < frames; ++f) fr << strip.copy(f * W, 0, W, H);
        out.insert(e.id, fr);
    }
    r.releaseResources();
    emit ready(out);
}

// ---------------------------------------------------------------- main window
MainWindow::MainWindow(Engine* e, QWidget* parent) : QMainWindow(parent), m_e(e), m_ctx(new UiContext(e, this)) {
    setWindowIcon(appIcon());
    setDockNestingEnabled(true);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea); setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    m_preview = new PreviewPanel(m_ctx);
    setCentralWidget(m_preview);
    m_library = new LibraryPanel(m_ctx);
    addPanel("Effects Library", m_library, Qt::LeftDockWidgetArea);
    addPanel("Layers", new LayerPanel(m_ctx), Qt::LeftDockWidgetArea);
    addPanel("Parameters", new ParameterPanel(m_ctx), Qt::RightDockWidgetArea);
    addPanel("Show Control", new ShowPanel(m_ctx), Qt::RightDockWidgetArea);
    addPanel("Timeline", new TimelinePanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Node Editor", new NodeEditorPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Pixel Mapper", new PixelMapperPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("3D Stage", new StagePanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Spatial Effects", new SpatialPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Fixture Manager", new FixturePanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Console Mapping", new ConsolePanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Output Configuration", new OutputPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Media Library", new MediaPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Audio", new AudioPanel(m_ctx), Qt::RightDockWidgetArea);
    addPanel("Calibration", new CalibrationPanel(m_ctx), Qt::RightDockWidgetArea);
    addPanel("Remote & Sync", new RemoteSyncPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Plugin Manager", new PluginPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("AI Effect Assistant", new AiPanel(m_ctx), Qt::BottomDockWidgetArea);
    addPanel("Performance Monitor", new PerformancePanel(m_ctx), Qt::RightDockWidgetArea);
    addPanel("Recovery", new RecoveryPanel(m_ctx), Qt::BottomDockWidgetArea);
    buildMenus();
    m_stFps = new QLabel; m_stNet = new QLabel; m_stTc = new QLabel; m_stFaults = new QLabel; m_stMsg = new QLabel;
    statusBar()->addWidget(m_stMsg, 1);
    for (auto* l : {m_stTc, m_stNet, m_stFaults, m_stFps}) { l->setStyleSheet("padding: 0 8px;"); statusBar()->addPermanentWidget(l); }
    connect(m_ctx, &UiContext::status, this, [this](const QString& s) { m_stMsg->setText(s); });
    connect(e, &Engine::message, this, [this](const QString& s) { m_stMsg->setText(s); });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &MainWindow::updateStatus); t->start(500);
    QSettings st; m_recent = st.value("recent").toStringList();
    applyWorkspace("Live");
    if (!st.value("geometry").isNull()) restoreGeometry(st.value("geometry").toByteArray()); else resize(1600, 960);
    m_thumbs = new ThumbnailWorker(e, QDir(e->options().dataDir).filePath("thumbs"));
    connect(m_thumbs, &ThumbnailWorker::ready, m_library, &LibraryPanel::setThumbnails, Qt::QueuedConnection);
    connect(e, &Engine::showStateChanged, this, &MainWindow::updateTitle);
    m_thumbs->start(QThread::LowPriority);
    updateTitle();
}

MainWindow::~MainWindow() { if (m_thumbs) { m_thumbs->requestInterruption(); m_thumbs->wait(10000); } }

QDockWidget* MainWindow::addPanel(const QString& name, QWidget* w, Qt::DockWidgetArea area) {
    auto* d = new QDockWidget(name, this);
    d->setObjectName(name); d->setWidget(w);
    w->setMinimumSize(120, 80);
    d->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(area, d);
    m_docks.insert(name, d);
    return d;
}

void MainWindow::applyWorkspace(const QString& name) {
    QSettings st;
    const QByteArray saved = st.value("workspace/" + name).toByteArray();
    if (!saved.isEmpty() && restoreState(saved)) return;
    for (auto* d : m_docks) { d->setFloating(false); d->hide(); }
    auto showIn = [&](Qt::DockWidgetArea a, QStringList names) {
        QDockWidget* first = nullptr;
        for (auto& n : names) { auto* d = m_docks.value(n); if (!d) continue; addDockWidget(a, d); d->show(); if (first) tabifyDockWidget(first, d); else first = d; }
        if (first) first->raise();
    };
    auto later = [this](QList<QDockWidget*> d, QList<int> sizes, Qt::Orientation o) { QTimer::singleShot(0, this, [=] { resizeDocks(d, sizes, o); }); };
    if (name == "Live") {
        showIn(Qt::LeftDockWidgetArea, {"Effects Library"}); showIn(Qt::LeftDockWidgetArea, {"Layers"});
        showIn(Qt::RightDockWidgetArea, {"Show Control"}); showIn(Qt::RightDockWidgetArea, {"Parameters", "Audio", "Performance Monitor"});
        showIn(Qt::BottomDockWidgetArea, {"Timeline", "3D Stage", "Pixel Mapper", "Console Mapping", "Recovery"});
        resizeDocks({m_docks["Effects Library"], m_docks["Show Control"]}, {380, 420}, Qt::Horizontal);
        later({m_docks["Timeline"]}, {int(height() * 0.3)}, Qt::Vertical);
        m_docks["Timeline"]->raise(); m_docks["Parameters"]->raise();
    } else if (name == "Programming") {
        showIn(Qt::LeftDockWidgetArea, {"Effects Library", "Media Library"}); showIn(Qt::LeftDockWidgetArea, {"Layers"});
        showIn(Qt::RightDockWidgetArea, {"Parameters", "AI Effect Assistant"});
        showIn(Qt::BottomDockWidgetArea, {"Node Editor", "Timeline"});
        later({m_docks["Node Editor"]}, {int(height() * 0.45)}, Qt::Vertical);
        m_docks["Node Editor"]->raise(); m_docks["Effects Library"]->raise(); m_docks["Parameters"]->raise();
    } else if (name == "Mapping") {
        showIn(Qt::RightDockWidgetArea, {"Spatial Effects", "Output Configuration"});
        showIn(Qt::BottomDockWidgetArea, {"Pixel Mapper", "Fixture Manager"}); showIn(Qt::BottomDockWidgetArea, {"3D Stage"});
        later({m_docks["Pixel Mapper"]}, {int(height() * 0.55)}, Qt::Vertical);
        m_docks["Pixel Mapper"]->raise(); m_docks["Spatial Effects"]->raise();
    } else if (name == "Console") {
        showIn(Qt::RightDockWidgetArea, {"Show Control"});
        showIn(Qt::BottomDockWidgetArea, {"Console Mapping", "Output Configuration", "Remote & Sync"});
        later({m_docks["Console Mapping"]}, {int(height() * 0.55)}, Qt::Vertical);
        m_docks["Console Mapping"]->raise();
    } else if (name == "System") {
        showIn(Qt::RightDockWidgetArea, {"Performance Monitor", "Calibration", "Audio"});
        showIn(Qt::BottomDockWidgetArea, {"Recovery", "Plugin Manager", "Remote & Sync", "Output Configuration"});
        later({m_docks["Recovery"]}, {int(height() * 0.5)}, Qt::Vertical);
        m_docks["Recovery"]->raise(); m_docks["Performance Monitor"]->raise();
    }
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu("&File");
    file->addAction("&New project", QKeySequence::New, this, [this] { if (maybeSave()) { m_e->newProject(); m_ctx->undo.clear(); } });
    file->addAction("&Open project...", QKeySequence::Open, this, [this] { if (!maybeSave()) return; const QString f = QFileDialog::getOpenFileName(this, "Open project", {}, "LUMAFLOW project (*.lumaflow)"); if (!f.isEmpty()) openProject(f); });
    auto* recent = file->addMenu("Open recent");
    connect(recent, &QMenu::aboutToShow, this, [this, recent] { recent->clear(); for (auto& r : m_recent) recent->addAction(r, this, [this, r] { if (maybeSave()) openProject(r); }); });
    file->addAction("&Save", QKeySequence::Save, this, [this] { if (m_e->projectPath().isEmpty()) saveAs(); else { QString err; if (!m_e->saveProject(m_e->projectPath(), &err)) QMessageBox::warning(this, "Save failed", err); } updateTitle(); });
    file->addAction("Save &as...", QKeySequence::SaveAs, this, &MainWindow::saveAs);
    file->addSeparator();
    file->addAction("Import scene (.lfscene)...", this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Import scene", {}, "LUMAFLOW scene (*.lfscene)"); if (f.isEmpty()) return;
        const auto r = ProjectIO::loadTyped(f, "lfscene"); if (!r.ok()) { QMessageBox::warning(this, "Import", r.error); return; }
        m_e->scenes() << Scene::fromJson(r.doc); emit m_e->showStateChanged();
    });
    file->addAction("Import preset (.lfpreset)...", this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Import preset", {}, "LUMAFLOW preset (*.lfpreset)"); if (f.isEmpty()) return;
        const auto r = ProjectIO::loadTyped(f, "lfpreset"); if (!r.ok()) { QMessageBox::warning(this, "Import", r.error); return; }
        m_e->presets() << Preset::fromJson(r.doc); emit m_e->showStateChanged();
    });
    file->addAction("Load demo show", this, [this] { if (maybeSave()) m_e->createDemoShow(); });
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    auto* edit = menuBar()->addMenu("&Edit");
    auto* undo = m_ctx->undo.createUndoAction(this, "&Undo"); undo->setShortcut(QKeySequence::Undo); edit->addAction(undo);
    auto* redo = m_ctx->undo.createRedoAction(this, "&Redo"); redo->setShortcut(QKeySequence::Redo); edit->addAction(redo);
    auto* show = menuBar()->addMenu("&Show");
    show->addAction("GO (next cue)", m_e, &Engine::cueGo);
    show->addAction("Back", m_e, &Engine::cueBack);
    show->addAction("Take preview → program", m_e, [this] { m_e->take(); });
    show->addAction("Tap tempo", m_e, &Engine::tapTempo);
    auto* bo = show->addAction("Blackout"); bo->setCheckable(true); connect(bo, &QAction::toggled, m_e, &Engine::setBlackout);
    auto* view = menuBar()->addMenu("&View");
    auto* ws = view->addMenu("Workspace");
    for (const char* w : {"Live", "Programming", "Mapping", "Console", "System"}) ws->addAction(w, this, [this, w] { applyWorkspace(w); });
    ws->addSeparator();
    ws->addAction("Save current layout as...", this, [this] { bool ok; const QString n = QInputDialog::getText(this, "Workspace", "Name:", QLineEdit::Normal, "My layout", &ok); if (ok) QSettings().setValue("workspace/" + n, saveState()); });
    ws->addAction("Reset built-in layouts", this, [] { QSettings s; for (const char* w : {"Live", "Programming", "Mapping", "Console", "System"}) s.remove(QString("workspace/") + w); });
    view->addSeparator();
    QStringList names = m_docks.keys(); names.sort();
    for (auto& n : names) view->addAction(m_docks[n]->toggleViewAction());
    view->addSeparator();
    view->addAction("Full screen", QKeySequence::FullScreen, this, [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    auto* help = menuBar()->addMenu("&Help");
    help->addAction("User manual (PDF)", this, [] {
        for (const QString& p : {QApplication::applicationDirPath() + "/../share/doc/lumaflow/LUMAFLOW_Professional_User_Manual.pdf", QApplication::applicationDirPath() + "/../../documentation/LUMAFLOW_Professional_User_Manual.pdf"})
            if (QFileInfo::exists(p)) { QDesktopServices::openUrl(QUrl::fromLocalFile(p)); return; }
    });
    help->addAction("Open data folder", this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_e->options().dataDir)); });
    help->addAction("About LUMAFLOW", this, [this] {
        QMessageBox::about(this, "About LUMAFLOW", QString("<h2>LUMAFLOW %1</h2><p>Real-time visual effects, media server, pixel mapping, spatial lighting effects and show control.</p>"
            "<p>GPU: %2</p><p>Licensed under the MIT licence. Uses Qt 6 (LGPLv3), FFmpeg (LGPL), zlib and SQLite.</p>").arg(LUMAFLOW_VERSION, m_e->renderThread() ? m_e->renderThread()->glInfo() : "n/a"));
    });
    // toolbar
    auto* tb = addToolBar("Show"); tb->setObjectName("ShowToolbar"); tb->setMovable(false);
    auto* logo = new QLabel; logo->setPixmap(appIcon().pixmap(28, 28)); tb->addWidget(logo);
    auto* name = new QLabel(" LUMAFLOW "); name->setStyleSheet("font-weight: bold; font-size: 13pt; color: #8fb4ff;"); tb->addWidget(name);
    tb->addSeparator();
    for (const char* w : {"Live", "Programming", "Mapping", "Console", "System"}) { auto* b = new QPushButton(w); b->setFlat(true); connect(b, &QPushButton::clicked, this, [this, w] { applyWorkspace(w); }); tb->addWidget(b); }
    tb->addSeparator();
    auto* go = new QPushButton("GO"); go->setObjectName("go"); connect(go, &QPushButton::clicked, m_e, &Engine::cueGo); tb->addWidget(go);
    auto* take = new QPushButton("TAKE"); take->setObjectName("take"); connect(take, &QPushButton::clicked, m_e, [this] { m_e->take(); }); tb->addWidget(take);
    auto* tap = new QPushButton("TAP"); connect(tap, &QPushButton::clicked, m_e, &Engine::tapTempo); tb->addWidget(tap);
    auto* master = new QSlider(Qt::Horizontal); master->setRange(0, 1000); master->setFixedWidth(160); master->setToolTip("Master intensity");
    master->setValue(int(m_e->params().value("master.intensity") * 1000));
    connect(master, &QSlider::valueChanged, m_e, [this](int v) { m_e->mapping().write("master.intensity", v / 1000.0, ControlSource::Touch); });
    connect(m_e, &Engine::parameterChanged, master, [master](const QString& id, double v) { if (id == "master.intensity" && !master->isSliderDown()) { master->blockSignals(true); master->setValue(int(v * 1000)); master->blockSignals(false); } });
    tb->addWidget(new QLabel(" Master ")); tb->addWidget(master);
    auto* blackout = new QPushButton("BLACKOUT"); blackout->setObjectName("blackout"); blackout->setCheckable(true);
    connect(blackout, &QPushButton::toggled, m_e, &Engine::setBlackout);
    connect(m_e, &Engine::showStateChanged, blackout, [this, blackout] { blackout->blockSignals(true); blackout->setChecked(m_e->blackout()); blackout->blockSignals(false); });
    tb->addWidget(blackout);
}

void MainWindow::updateStatus() {
    const EngineStatus s = m_e->status();
    m_stFps->setText(QString("%1 fps").arg(s.render.fps, 0, 'f', 1));
    m_stNet->setText(QString("Art-Net %1  sACN %2  OSC %3").arg(s.network.artnetOut).arg(s.network.sacnOut).arg(s.network.oscIn));
    m_stTc->setText("TC " + s.timecode + (s.recording ? "  ● REC" : ""));
    m_stFaults->setText(s.faults ? QString("<span style='color:#ff8080'>⚠ %1 fault(s)</span>").arg(s.faults) : "<span style='color:#6c6'>OK</span>");
}

void MainWindow::updateTitle() { setWindowTitle(QString("LUMAFLOW %1 - %2%3%4").arg(LUMAFLOW_VERSION, m_e->projectPath().isEmpty() ? "Untitled" : QFileInfo(m_e->projectPath()).fileName(), m_e->dirty() ? " *" : "", m_e->options().safeMode ? "  [SAFE MODE]" : "")); }

bool MainWindow::openProject(const QString& path) {
    QStringList w; QString err;
    if (!m_e->loadProject(path, &w, &err)) { QMessageBox::warning(this, "Open failed", err); return false; }
    m_recent.removeAll(path); m_recent.prepend(path); while (m_recent.size() > 10) m_recent.removeLast(); QSettings().setValue("recent", m_recent);
    m_ctx->undo.clear();
    if (!w.isEmpty()) QMessageBox::information(this, "Project opened with warnings", w.join("\n"));
    updateTitle();
    return true;
}

void MainWindow::saveAs() {
    const QString f = QFileDialog::getSaveFileName(this, "Save project", m_e->projectPath().isEmpty() ? QDir::home().filePath("show.lumaflow") : m_e->projectPath(), "LUMAFLOW project (*.lumaflow)");
    if (f.isEmpty()) return;
    QString err; if (!m_e->saveProject(f.endsWith(".lumaflow") ? f : f + ".lumaflow", &err)) QMessageBox::warning(this, "Save failed", err);
    updateTitle();
}

bool MainWindow::maybeSave() {
    if (m_automation || !m_e->dirty()) return true;
    const auto r = QMessageBox::question(this, "Unsaved changes", "Save changes to the current project?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) { if (m_e->projectPath().isEmpty()) saveAs(); else m_e->saveProject(m_e->projectPath()); }
    return true;
}

void MainWindow::checkCrashRecovery() {
    if (!m_e->session().previousSessionCrashed()) return;
    const QString autosave = m_e->session().autosavePath();
    if (!QFileInfo::exists(autosave)) return;
    const auto r = QMessageBox::question(this, "Recover previous session",
        QString("LUMAFLOW did not shut down cleanly last time.\n\nRecover the autosave from %1?\n(Last project: %2)")
            .arg(QFileInfo(autosave).lastModified().toString(), m_e->session().previousSessionInfo()["project"].toString()), QMessageBox::Yes | QMessageBox::No);
    if (r == QMessageBox::Yes) { QStringList w; QString err; if (m_e->loadProject(autosave, &w, &err)) { m_stMsg->setText("Recovered autosave"); } }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) { e->ignore(); return; }
    QSettings().setValue("geometry", saveGeometry());
    e->accept();
}

} // namespace lf
