#include "SystemPanels.h"
#include "Widgets.h"
#include "UndoCommands.h"
#include "app/Engine.h"
#include "core/Log.h"
#include "storage/ProjectIO.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QSplitter>
#include <QScrollArea>
#include <QDir>
#include <QJsonDocument>

namespace lf {

static QString setToString(const QSet<int>& s) { QList<int> l = s.values(); std::sort(l.begin(), l.end()); QStringList o; for (int v : l) o << QString::number(v); return o.join(","); }
static QSet<int> parseSet(const QString& s) {
    QSet<int> out;
    for (const QString& part : s.split(',', Qt::SkipEmptyParts)) {
        if (part.contains('-')) { const int a = part.section('-', 0, 0).toInt(), b = part.section('-', 1, 1).toInt(); for (int i = a; i <= b && i - a < 1024; ++i) out.insert(i); }
        else if (part.trimmed().toInt() > 0) out.insert(part.trimmed().toInt());
    }
    return out;
}

// ================================================================ Output window
OutputWindow::OutputWindow(Engine* e, int index, QWidget* parent) : QWidget(parent, Qt::Window | Qt::FramelessWindowHint), m_e(e), m_index(index) {
    setAttribute(Qt::WA_DeleteOnClose); setAttribute(Qt::WA_OpaquePaintEvent); setCursor(Qt::BlankCursor);
    connect(e, &Engine::frameReady, this, qOverload<>(&QWidget::update));
}
void OutputWindow::setRegion(const OutputRegion& r) {
    m_region = r; setWindowTitle("LUMAFLOW output: " + r.name);
    const auto screens = QGuiApplication::screens();
    if (r.screen >= 0 && r.screen < screens.size()) { setGeometry(screens[r.screen]->geometry()); showFullScreen(); }
    else { resize(960, 540); show(); }
}
void OutputWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    if (!m_region.enabled) { p.fillRect(rect(), Qt::black); return; }
    if (m_region.testPattern >= 0) {
        p.fillRect(rect(), Qt::black); p.setPen(QPen(Qt::white, 3)); p.drawRect(rect().adjusted(2, 2, -3, -3));
        p.drawLine(rect().topLeft(), rect().bottomRight()); p.drawLine(rect().topRight(), rect().bottomLeft());
        QFont f = font(); f.setPointSize(48); f.setBold(true); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QString("%1\n%2 x %3").arg(m_region.name).arg(width()).arg(height()));
        return;
    }
    p.drawImage(rect(), m_region.process(m_e->lastProgramFrame(), size()));
}
void OutputWindow::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape && e->modifiers() & Qt::ShiftModifier) close(); }

// ================================================================ Output panel
OutputPanel::OutputPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(4, 4, 4, 4);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget; auto* v = new QVBoxLayout(inner); scroll->setWidget(inner); outer->addWidget(scroll);
    auto* net = new QGroupBox("DMX network (Art-Net / sACN) and OSC"); auto* f = new QFormLayout(net);
    m_anOut = new QCheckBox("Art-Net output"); m_anIn = new QCheckBox("Art-Net input"); m_saOut = new QCheckBox("sACN output"); m_saIn = new QCheckBox("sACN input"); m_oscIn = new QCheckBox("OSC input");
    auto* rowA = new QHBoxLayout; rowA->addWidget(m_anOut); rowA->addWidget(m_anIn); rowA->addWidget(m_saOut); rowA->addWidget(m_saIn); rowA->addWidget(m_oscIn); f->addRow(rowA);
    m_anTarget = new QLineEdit; m_anTarget->setToolTip("Broadcast (e.g. 2.255.255.255 / 10.255.255.255) or unicast node IP"); f->addRow("Art-Net target", m_anTarget);
    m_offset = new QSpinBox; m_offset->setRange(0, 1); m_offset->setToolTip("1: LUMAFLOW universe 1 = Art-Net port address 0"); f->addRow("Art-Net universe offset", m_offset);
    m_saTargets = new QLineEdit; m_saTargets->setPlaceholderText("empty = multicast 239.255.x.y"); f->addRow("sACN unicast targets", m_saTargets);
    m_prio = new QSpinBox; m_prio->setRange(0, 200); f->addRow("sACN priority", m_prio);
    m_outU = new QLineEdit; m_outU->setToolTip("Only these universes are transmitted, e.g. 1-4,10"); f->addRow("Output universes (enabled)", m_outU);
    m_inU = new QLineEdit; f->addRow("Input universes", m_inU);
    m_rate = new QDoubleSpinBox; m_rate->setRange(1, 44); m_rate->setSuffix(" Hz"); f->addRow("DMX refresh rate", m_rate);
    m_oscPort = new QSpinBox; m_oscPort->setRange(1, 65535); f->addRow("OSC listen port", m_oscPort);
    m_oscFb = new QLineEdit; m_oscFb->setPlaceholderText("host:port, host:port"); f->addRow("OSC feedback targets", m_oscFb);
    auto* applyBtn = new QPushButton("Apply network settings"); f->addRow(applyBtn);
    v->addWidget(net);
    auto* diag = new QGroupBox("Output diagnostics"); auto* dv = new QVBoxLayout(diag);
    m_diag = new QTableWidget(0, 6); m_diag->setHorizontalHeaderLabels({"Universe", "Out pkts", "Out rate", "In pkts", "In rate", "Last source"});
    m_diag->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_diag->verticalHeader()->hide(); m_diag->setMinimumHeight(140);
    dv->addWidget(m_diag); v->addWidget(diag);
    auto* vid = new QGroupBox("Video outputs"); auto* vv = new QVBoxLayout(vid);
    auto* cv = new QHBoxLayout; m_cw = new QSpinBox; m_ch = new QSpinBox; m_cw->setRange(16, 16384); m_ch->setRange(16, 16384);
    auto* setCanvas = new QPushButton("Set canvas"); auto* preset = new QComboBox; preset->addItems({"Presets...", "1280x720", "1920x1080", "3840x2160", "3840x1080 (ultra-wide)", "1024x512 (LED)"});
    cv->addWidget(new QLabel("Canvas")); cv->addWidget(m_cw); cv->addWidget(new QLabel("x")); cv->addWidget(m_ch); cv->addWidget(preset); cv->addWidget(setCanvas); cv->addStretch(); vv->addLayout(cv);
    m_outputs = new QTableWidget(0, 7); m_outputs->setHorizontalHeaderLabels({"Name", "Screen", "Crop x,y,w,h", "Rotation", "Size WxH", "Test", "Blackout"});
    m_outputs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_outputs->verticalHeader()->hide(); m_outputs->setMinimumHeight(110); vv->addWidget(m_outputs);
    auto* ob = new QHBoxLayout; auto* addOut = new QPushButton("Add output"); auto* openOut = new QPushButton("Open output windows"); auto* closeOut = new QPushButton("Close outputs");
    ob->addWidget(addOut); ob->addWidget(openOut); ob->addWidget(closeOut); ob->addStretch(); vv->addLayout(ob);
    auto* scr = new QLabel; QStringList sl; int i = 0; for (auto* s : QGuiApplication::screens()) sl << QString("%1: %2 %3x%4").arg(i++).arg(s->name()).arg(s->geometry().width()).arg(s->geometry().height());
    scr->setText("Screens: " + sl.join("   ") + "   (Shift+Esc closes a fullscreen output)"); scr->setWordWrap(true); vv->addWidget(scr);
    auto* na = new QLabel("NDI, Spout (Windows), Syphon (macOS) and SDI are available as output plugins via the Plugin SDK output module interface; none are bundled in this build."); na->setWordWrap(true); na->setStyleSheet("color:#7a80a8;"); vv->addWidget(na);
    v->addWidget(vid);
    auto* rec = new QGroupBox("Recording"); auto* rv = new QHBoxLayout(rec);
    auto* recBtn = new QPushButton("● Record program..."); auto* stopBtn = new QPushButton("■ Stop"); m_rec = new QLabel;
    rv->addWidget(recBtn); rv->addWidget(stopBtn); rv->addWidget(m_rec, 1); v->addWidget(rec);
    v->addStretch();
    connect(applyBtn, &QPushButton::clicked, this, &OutputPanel::applyNetwork);
    connect(preset, &QComboBox::activated, this, [this, preset](int i) { if (i == 0) return; const QString s = preset->itemText(i).section(' ', 0, 0); m_cw->setValue(s.section('x', 0, 0).toInt()); m_ch->setValue(s.section('x', 1, 1).toInt()); preset->setCurrentIndex(0); });
    connect(setCanvas, &QPushButton::clicked, this, [this] { QMutexLocker l(&c->engine->stateMutex()); c->engine->videoOutputs().canvasWidth = m_cw->value(); c->engine->videoOutputs().canvasHeight = m_ch->value(); });
    connect(addOut, &QPushButton::clicked, this, [this] { OutputRegion r; r.name = QString("Output %1").arg(c->engine->videoOutputs().outputs.size() + 1); c->engine->videoOutputs().outputs << r; loadConfig(); });
    connect(openOut, &QPushButton::clicked, this, [this] {
        for (auto& w : m_windows) if (w) w->close();
        m_windows.clear();
        const auto& outs = c->engine->videoOutputs().outputs;
        for (int i = 0; i < outs.size(); ++i) if (outs[i].enabled) { auto* w = new OutputWindow(c->engine, i); w->setRegion(outs[i]); m_windows << w; }
    });
    connect(closeOut, &QPushButton::clicked, this, [this] { for (auto& w : m_windows) if (w) w->close(); m_windows.clear(); });
    connect(m_outputs, &QTableWidget::cellChanged, this, [this](int r, int col) {
        auto& outs = c->engine->videoOutputs().outputs; if (r >= outs.size() || !m_outputs->item(r, col)) return;
        const QString t = m_outputs->item(r, col)->text(); OutputRegion& o = outs[r];
        if (col == 0) o.name = t; else if (col == 1) o.screen = t.toInt(); else if (col == 2) { const auto p = t.split(','); if (p.size() == 4) o.source = QRectF(p[0].toDouble(), p[1].toDouble(), p[2].toDouble(), p[3].toDouble()); }
        else if (col == 3) o.rotation = (t.toInt() / 90 % 4) * 90; else if (col == 4) o.customSize = QSize(t.section('x', 0, 0).toInt(), t.section('x', 1, 1).toInt());
        else if (col == 5) o.testPattern = t.startsWith("y", Qt::CaseInsensitive) ? 0 : -1; else if (col == 6) o.blackout = t.startsWith("y", Qt::CaseInsensitive);
        for (auto& w : m_windows) if (w) { const int idx = int(m_windows.indexOf(w)); if (idx >= 0 && idx < outs.size()) w->setRegion(outs[idx]); }
    });
    connect(recBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, "Record to", QDir::home().filePath("lumaflow_recording.mp4"), "MP4 video (*.mp4)"); if (f.isEmpty()) return;
        QString err; if (!c->engine->startRecording(f, &err)) QMessageBox::warning(this, "Recording", err);
    });
    connect(stopBtn, &QPushButton::clicked, c->engine, &Engine::stopRecording);
    connect(c->engine, &Engine::projectLoaded, this, &OutputPanel::loadConfig);
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &OutputPanel::refreshStats); t->start(1000);
    loadConfig();
}

void OutputPanel::loadConfig() {
    const NetworkConfig nc = c->engine->network().config();
    m_anOut->setChecked(nc.artnetOut); m_anIn->setChecked(nc.artnetIn); m_saOut->setChecked(nc.sacnOut); m_saIn->setChecked(nc.sacnIn); m_oscIn->setChecked(nc.oscIn);
    m_anTarget->setText(nc.artnetTarget.toString()); m_offset->setValue(nc.artnetUniverseOffset);
    QStringList st; for (auto& h : nc.sacnUnicastTargets) st << h.toString(); m_saTargets->setText(st.join(", "));
    m_prio->setValue(nc.sacnPriority); m_outU->setText(setToString(nc.enabledOutputUniverses)); m_inU->setText(setToString(nc.inputUniverses));
    m_rate->setValue(nc.outputRateHz); m_oscPort->setValue(nc.oscListenPort);
    QStringList fb; for (auto& t : nc.oscFeedbackTargets) fb << QString("%1:%2").arg(t.first.toString()).arg(t.second); m_oscFb->setText(fb.join(", "));
    m_cw->setValue(c->engine->videoOutputs().canvasWidth); m_ch->setValue(c->engine->videoOutputs().canvasHeight);
    m_outputs->blockSignals(true);
    const auto& outs = c->engine->videoOutputs().outputs; m_outputs->setRowCount(outs.size());
    for (int i = 0; i < outs.size(); ++i) {
        const auto& o = outs[i];
        const QStringList cols{o.name, QString::number(o.screen), QString("%1,%2,%3,%4").arg(o.source.x()).arg(o.source.y()).arg(o.source.width()).arg(o.source.height()), QString::number(o.rotation),
                               o.customSize.isEmpty() ? "native" : QString("%1x%2").arg(o.customSize.width()).arg(o.customSize.height()), o.testPattern >= 0 ? "yes" : "no", o.blackout ? "yes" : "no"};
        for (int k = 0; k < cols.size(); ++k) m_outputs->setItem(i, k, new QTableWidgetItem(cols[k]));
    }
    m_outputs->blockSignals(false);
}

void OutputPanel::applyNetwork() {
    NetworkConfig nc = c->engine->network().config();
    nc.artnetOut = m_anOut->isChecked(); nc.artnetIn = m_anIn->isChecked(); nc.sacnOut = m_saOut->isChecked(); nc.sacnIn = m_saIn->isChecked(); nc.oscIn = m_oscIn->isChecked();
    nc.artnetTarget = QHostAddress(m_anTarget->text().trimmed()); nc.artnetUniverseOffset = m_offset->value();
    nc.sacnUnicastTargets.clear(); for (auto& h : m_saTargets->text().split(',', Qt::SkipEmptyParts)) nc.sacnUnicastTargets << QHostAddress(h.trimmed());
    nc.sacnPriority = m_prio->value(); nc.enabledOutputUniverses = parseSet(m_outU->text()); nc.inputUniverses = parseSet(m_inU->text());
    nc.outputRateHz = m_rate->value(); nc.oscListenPort = m_oscPort->value();
    nc.oscFeedbackTargets.clear();
    for (auto& t : m_oscFb->text().split(',', Qt::SkipEmptyParts)) { const QString s = t.trimmed(); const int i = s.lastIndexOf(':'); if (i > 0) nc.oscFeedbackTargets << qMakePair(QHostAddress(s.left(i)), quint16(s.mid(i + 1).toUInt())); }
    const QSet<int> used = c->engine->pixelMap().universesUsed(c->engine->patch());
    const QSet<int> missing = used - nc.enabledOutputUniverses;
    if (!missing.isEmpty() && (nc.artnetOut || nc.sacnOut)) emit c->status(QString("Note: pixel map uses universes %1 that are not enabled for output").arg(setToString(missing)));
    c->engine->applyNetworkConfig(nc);
}

void OutputPanel::refreshStats() {
    if (!isVisible()) return;
    const NetworkStats s = c->engine->network().stats();
    m_diag->setRowCount(s.universes.size());
    int r = 0;
    for (auto it = s.universes.begin(); it != s.universes.end(); ++it, ++r) {
        const QStringList cols{QString::number(it.key()), QString::number(it->packetsOut), QString::number(it->rateOut, 'f', 1), QString::number(it->packetsIn), QString::number(it->rateIn, 'f', 1), it->lastSource};
        for (int k = 0; k < cols.size(); ++k) m_diag->setItem(r, k, new QTableWidgetItem(cols[k]));
    }
    m_rec->setText(c->engine->recording() ? "RECORDING" : QString("Art-Net out %1  sACN out %2  OSC in %3  errors %4").arg(s.artnetOut).arg(s.sacnOut).arg(s.oscIn).arg(s.errors));
}

// ================================================================ Media
MediaPanel::MediaPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* h = new QHBoxLayout;
    auto* imp = new QPushButton("Import media..."); auto* use = new QPushButton("Use on selected layer"); auto* logo = new QPushButton("Use as logo"); auto* rem = new QPushButton("Remove"); auto* relink = new QPushButton("Relink missing...");
    for (QWidget* w : std::initializer_list<QWidget*>{imp, use, logo, rem, relink}) h->addWidget(w);
    h->addStretch(); v->addLayout(h);
    m_list = new QListWidget; m_list->setViewMode(QListView::IconMode); m_list->setIconSize(QSize(160, 90)); m_list->setGridSize(QSize(176, 130)); m_list->setResizeMode(QListView::Adjust); m_list->setWordWrap(true);
    v->addWidget(m_list, 1);
    m_info = new QLabel; v->addWidget(m_info);
    connect(imp, &QPushButton::clicked, this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(this, "Import media", {}, "Media (*.mp4 *.mov *.mkv *.avi *.webm *.png *.jpg *.jpeg *.gif *.bmp *.webp *.tif)");
        QStringList errs; for (auto& f : files) { QString e; if (!c->engine->mediaLibrary().add(f, &e)) errs << QFileInfo(f).fileName() + ": " + e; }
        if (!errs.isEmpty()) QMessageBox::warning(this, "Import", errs.join("\n"));
        refresh();
    });
    connect(use, &QPushButton::clicked, this, [this] { auto* it = m_list->currentItem(); if (it) undoable(c->engine, &c->undo, "Assign media", [&] { c->engine->setLayerMedia(c->layer(), it->data(Qt::UserRole).toString(), c->editPreview()); }); });
    connect(m_list, &QListWidget::itemDoubleClicked, use, &QPushButton::click);
    connect(logo, &QPushButton::clicked, this, [this] { auto* it = m_list->currentItem(); if (!it) return; c->engine->setLayerEffect(c->layer(), "logo"); LayerSource s = c->engine->program().layers[c->layer()]; s.mediaPath = it->data(Qt::UserRole).toString(); c->engine->setLayerSource(c->layer(), s); });
    connect(rem, &QPushButton::clicked, this, [this] { auto* it = m_list->currentItem(); if (it) { c->engine->mediaLibrary().remove(it->data(Qt::UserRole).toString()); refresh(); } });
    connect(relink, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, "Search folder for missing media"); if (d.isEmpty()) return;
        const int n = c->engine->mediaLibrary().relink(d);
        QJsonObject doc = c->engine->toProjectJson(); const int m = ProjectIO::relinkMedia(doc, d); if (m > 0) c->engine->fromProjectJson(doc);
        QMessageBox::information(this, "Relink", QString("Relinked %1 library item(s) and %2 project reference(s).").arg(n).arg(m)); refresh();
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it) { if (it) m_info->setText(it->toolTip()); });
    refresh();
}

void MediaPanel::refresh() {
    m_list->clear();
    for (const auto& i : c->engine->mediaLibrary().items()) {
        auto* it = new QListWidgetItem((i.valid ? "" : "⚠ MISSING: ") + QFileInfo(i.path).fileName());
        it->setData(Qt::UserRole, i.path);
        it->setToolTip(QString("%1\n%2x%3  %4%5  codec %6%7").arg(i.path).arg(i.width).arg(i.height).arg(i.isVideo ? QString("%1 s @ %2 fps").arg(i.duration, 0, 'f', 1).arg(i.fps, 0, 'f', 2) : "image")
                       .arg(i.hasAlpha ? "  alpha" : "").arg(i.codec).arg(i.valid ? "" : "\nFile missing - use Relink"));
        const QImage th = c->engine->mediaLibrary().thumbnail(i.path);
        if (!th.isNull()) it->setIcon(QPixmap::fromImage(th));
        m_list->addItem(it);
    }
    m_info->setText(QString("%1 item(s). Video decoding runs on background threads with a bounded frame queue.").arg(m_list->count()));
}

// ================================================================ Audio
AudioPanel::AudioPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* f = new QFormLayout;
    m_dev = new QComboBox; m_dev->setEditable(true); m_dev->addItems(AudioInput::devices()); m_dev->setToolTip("test:<bpm> synthetic signal, file:/path.wav, alsa:<device>");
    auto* browse = new QPushButton("WAV file..."); auto* devRow = new QHBoxLayout; devRow->addWidget(m_dev, 1); devRow->addWidget(browse);
    m_en = new QCheckBox("Enable audio input");
    auto dbl = [](double mn, double mx, double val, double st) { auto* d = new QDoubleSpinBox; d->setRange(mn, mx); d->setValue(val); d->setSingleStep(st); d->setDecimals(3); return d; };
    const AudioSettings s = c->engine->audioSettings();
    m_gain = dbl(0, 20, s.gain, 0.1); m_gate = dbl(0, 0.5, s.gate, 0.005); m_smooth = dbl(0, 0.99, s.smoothing, 0.05); m_sens = dbl(1.0, 4.0, s.sensitivity, 0.1);
    m_dev->setCurrentText(s.device); m_en->setChecked(s.enabled);
    f->addRow("Input", devRow); f->addRow(m_en); f->addRow("Gain", m_gain); f->addRow("Noise gate", m_gate); f->addRow("Smoothing", m_smooth); f->addRow("Beat sensitivity", m_sens);
    v->addLayout(f);
    v->addWidget(new ParamSlider(c->engine, "audio.reactivity", "Global reactivity", &c->undo));
    m_bpm = new QLabel; m_bpm->setObjectName("title"); v->addWidget(m_bpm);
    m_bands = new LevelMeter; m_bands->setMinimumHeight(90); v->addWidget(m_bands);
    m_spec = new LevelMeter; m_spec->setMinimumHeight(90); v->addWidget(m_spec);
    auto* syncBpm = new QPushButton("Use detected BPM as show tempo"); v->addWidget(syncBpm);
    auto* note = new QLabel("Audio drives layers through each layer's 'Audio reactivity' parameter (bass + beat modulate scale and brightness) and the audio nodes in the Node Editor. "
                            "LTC timecode can be decoded from the same input (Timeline → TC: ltc)."); note->setWordWrap(true); note->setStyleSheet("color:#8088b0;"); v->addWidget(note);
    v->addStretch();
    auto apply = [this] { AudioSettings a; a.device = m_dev->currentText(); a.enabled = m_en->isChecked(); a.gain = m_gain->value(); a.gate = m_gate->value(); a.smoothing = m_smooth->value(); a.sensitivity = m_sens->value(); c->engine->applyAudioSettings(a); };
    connect(m_en, &QCheckBox::toggled, this, apply);
    connect(m_dev, &QComboBox::currentTextChanged, this, [this, apply] { if (m_en->isChecked()) apply(); });
    for (auto* d : {m_gain, m_gate, m_smooth, m_sens}) connect(d, &QDoubleSpinBox::editingFinished, this, apply);
    connect(browse, &QPushButton::clicked, this, [this] { const QString f = QFileDialog::getOpenFileName(this, "Audio file", {}, "WAV (*.wav)"); if (!f.isEmpty()) m_dev->setCurrentText("file:" + f); });
    connect(syncBpm, &QPushButton::clicked, this, [this] { const double b = c->engine->audio().features().bpm; if (b > 0) c->engine->params().set("show.bpm", b, "audio"); });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, [this] {
        if (!isVisible()) return;
        const AudioFeatures a = c->engine->audio().features();
        m_bands->setLevels(QVector<float>(a.bands, a.bands + 6) << std::min(1.f, a.level * 4) << a.beatPulse, {"Sub", "Bass", "LoMid", "Mid", "HiMid", "Treble", "Level", "Beat"});
        m_spec->setLevels(a.spectrum);
        m_bpm->setText(a.bpm > 0 ? QString("Detected tempo: %1 BPM (confidence %2%)  beats: %3").arg(a.bpm, 0, 'f', 1).arg(int(a.bpmConfidence * 100)).arg(a.beatCount) : "Detected tempo: -");
    }); t->start(50);
}

// ================================================================ Calibration / safety
CalibrationPanel::CalibrationPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* warn = new QLabel("⚠ Photosensitivity warning: strobing and rapidly flashing content can trigger seizures in people with photosensitive epilepsy. "
                            "The safety limiter caps master-strobe and strobe-effect flash rates (default 3 Hz) and the global output brightness. Keep it enabled for public audiences.");
    warn->setWordWrap(true); warn->setObjectName("warning"); v->addWidget(warn);
    auto* safety = new QGroupBox("Safety limits"); auto* sf = new QFormLayout(safety);
    auto* lim = new QCheckBox("Limiter enabled"); lim->setChecked(c->engine->safety().limiterEnabled);
    auto* hz = new QDoubleSpinBox; hz->setRange(0.5, 12); hz->setValue(c->engine->safety().maxStrobeHz); hz->setSuffix(" Hz");
    auto* maxB = new QDoubleSpinBox; maxB->setRange(0, 1); maxB->setSingleStep(0.05); maxB->setValue(c->engine->safety().maxBrightness);
    sf->addRow(lim); sf->addRow("Max flash rate", hz); sf->addRow("Max output brightness", maxB);
    v->addWidget(safety);
    auto* cal = new QGroupBox("Video output calibration"); auto* cf = new QFormLayout(cal);
    auto dbl = [](double mn, double mx, double val, double st) { auto* d = new QDoubleSpinBox; d->setRange(mn, mx); d->setValue(val); d->setSingleStep(st); d->setDecimals(3); return d; };
    OutputCalibration& oc = c->engine->calibration();
    auto* gamma = dbl(0.3, 3.0, oc.gamma, 0.05); auto* r = dbl(0, 2, oc.gainR, 0.01); auto* g = dbl(0, 2, oc.gainG, 0.01); auto* b = dbl(0, 2, oc.gainB, 0.01);
    auto* br = dbl(0, 2, oc.brightness, 0.05); auto* wp = dbl(2000, 12000, oc.whitePointK, 100);
    cf->addRow("Gamma", gamma); cf->addRow("Red gain", r); cf->addRow("Green gain", g); cf->addRow("Blue gain", b); cf->addRow("Brightness", br); cf->addRow("White point (K)", wp);
    v->addWidget(cal);
    auto* mast = new QGroupBox("Master colour"); auto* mv = new QVBoxLayout(mast);
    for (const char* id : {"master.red", "master.green", "master.blue", "master.saturation"}) mv->addWidget(new ParamSlider(c->engine, id, c->engine->params().def(id).name, &c->undo));
    v->addWidget(mast);
    auto* tp = new QHBoxLayout; auto* test = new QComboBox; test->addItems({"Test pattern: off", "Colour bars + grid", "Grey ramp", "Full white", "RGB cycle"});
    auto* save = new QPushButton("Save calibration preset..."); auto* load = new QPushButton("Load preset...");
    tp->addWidget(test); tp->addWidget(save); tp->addWidget(load); tp->addStretch(); v->addLayout(tp);
    v->addStretch();
    auto apply = [this, lim, hz, maxB, gamma, r, g, b, br, wp] {
        QMutexLocker l(&c->engine->stateMutex());
        c->engine->safety().limiterEnabled = lim->isChecked(); c->engine->safety().maxStrobeHz = hz->value(); c->engine->safety().maxBrightness = maxB->value();
        OutputCalibration& o = c->engine->calibration(); o.gamma = gamma->value(); o.gainR = r->value(); o.gainG = g->value(); o.gainB = b->value(); o.brightness = br->value(); o.whitePointK = wp->value();
    };
    connect(lim, &QCheckBox::toggled, this, [this, lim, apply] {
        if (!lim->isChecked() && QMessageBox::warning(this, "Disable safety limiter", "Disabling the limiter allows flash rates up to 12 Hz. Only do this for controlled environments with appropriate warnings. Continue?", QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) { lim->setChecked(true); return; }
        apply();
    });
    for (auto* d : findChildren<QDoubleSpinBox*>()) connect(d, &QDoubleSpinBox::valueChanged, this, apply);
    connect(test, &QComboBox::currentIndexChanged, this, [this](int i) { c->engine->setTestPattern(i - 1); });
    connect(save, &QPushButton::clicked, this, [this] { const QString f = QFileDialog::getSaveFileName(this, "Calibration preset", "calibration.json", "JSON (*.json)"); if (!f.isEmpty()) ProjectIO::saveTyped(f, "lfcalibration", c->engine->calibration().toJson()); });
    connect(load, &QPushButton::clicked, this, [this, gamma, r, g, b, br, wp] {
        const QString f = QFileDialog::getOpenFileName(this, "Calibration preset", {}, "JSON (*.json)"); if (f.isEmpty()) return;
        const auto res = ProjectIO::loadTyped(f, "lfcalibration"); if (!res.ok()) { QMessageBox::warning(this, "Load", res.error); return; }
        const OutputCalibration o = OutputCalibration::fromJson(res.doc);
        gamma->setValue(o.gamma); r->setValue(o.gainR); g->setValue(o.gainG); b->setValue(o.gainB); br->setValue(o.brightness); wp->setValue(o.whitePointK);
    });
}

// ================================================================ Remote & sync
RemoteSyncPanel::RemoteSyncPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* rem = new QGroupBox("Remote touchscreen control (local network, no internet required)"); auto* rv = new QVBoxLayout(rem);
    auto* rh = new QHBoxLayout; auto* port = new QSpinBox; port->setRange(1024, 65535); port->setValue(8080);
    auto* start = new QPushButton("Start server"); auto* stop = new QPushButton("Stop"); m_url = new QLabel; m_url->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rh->addWidget(new QLabel("Port")); rh->addWidget(port); rh->addWidget(start); rh->addWidget(stop); rh->addWidget(m_url, 1); rv->addLayout(rh);
    m_users = new QTableWidget(0, 3); m_users->setHorizontalHeaderLabels({"User", "Role", "Access token"}); m_users->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_users->setMaximumHeight(120);
    rv->addWidget(m_users);
    m_clients = new QTableWidget(0, 4); m_clients->setHorizontalHeaderLabels({"Client", "User", "Last seen", "Requests"}); m_clients->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_clients->setMaximumHeight(120);
    rv->addWidget(m_clients);
    v->addWidget(rem);
    auto* sync = new QGroupBox("Multi-instance synchronisation"); auto* sv = new QVBoxLayout(sync);
    auto* sh = new QHBoxLayout; auto* role = new QComboBox; role->addItems({"Off", "Primary", "Secondary"}); auto* sport = new QSpinBox; sport->setRange(1024, 65535); sport->setValue(7700);
    auto* sstart = new QPushButton("Apply"); sh->addWidget(role); sh->addWidget(new QLabel("Port")); sh->addWidget(sport); sh->addWidget(sstart); sh->addStretch(); sv->addLayout(sh);
    m_sync = new QLabel; m_sync->setWordWrap(true); sv->addWidget(m_sync);
    auto* note = new QLabel("Sync shares parameters, scene recalls, blackout and the timeline clock over UDP multicast (239.255.77.77). LAN alignment is typically a few milliseconds; "
                            "it is not a genlocked, frame-accurate system. For frame-accurate multi-server playback use house sync/genlock hardware."); note->setWordWrap(true); note->setStyleSheet("color:#8088b0;"); sv->addWidget(note);
    v->addWidget(sync); v->addStretch();
    auto refreshUsers = [this, port] {
        const auto u = c->engine->remote().users();
        m_users->setRowCount(u.size()); int i = 0;
        for (auto it = u.begin(); it != u.end(); ++it, ++i) { m_users->setItem(i, 0, new QTableWidgetItem(it.key())); m_users->setItem(i, 1, new QTableWidgetItem(remoteRoleName(it->second))); m_users->setItem(i, 2, new QTableWidgetItem(it->first)); }
        QString ip = "127.0.0.1"; for (auto& a : QNetworkInterface::allAddresses()) if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()) { ip = a.toString(); break; }
        m_url->setText(c->engine->remote().running() ? QString("Open http://%1:%2/ on a tablet and enter a token").arg(ip).arg(c->engine->remote().port()) : "Server stopped");
    };
    connect(start, &QPushButton::clicked, this, [this, port, refreshUsers] { if (!c->engine->startRemote(quint16(port->value()))) QMessageBox::warning(this, "Remote", "Could not start server (port in use or safe mode)."); refreshUsers(); });
    connect(stop, &QPushButton::clicked, this, [this, refreshUsers] { c->engine->remote().stop(); refreshUsers(); });
    connect(sstart, &QPushButton::clicked, this, [this, role, sport] { if (role->currentIndex() == 0) c->engine->sync().stop(); else c->engine->startSync(SyncRole(role->currentIndex()), quint16(sport->value())); });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, [this] {
        if (!isVisible()) return;
        const auto cl = c->engine->remote().clients(); m_clients->setRowCount(cl.size());
        for (int i = 0; i < cl.size(); ++i) { m_clients->setItem(i, 0, new QTableWidgetItem(cl[i].peer)); m_clients->setItem(i, 1, new QTableWidgetItem(cl[i].user)); m_clients->setItem(i, 2, new QTableWidgetItem(cl[i].lastSeen.toString("HH:mm:ss"))); m_clients->setItem(i, 3, new QTableWidgetItem(QString::number(cl[i].requests))); }
        const SyncStatus s = c->engine->sync().status();
        m_sync->setText(QString("Role: %1   %2   peer %3   RTT %4 ms   clock offset %5 ms   in %6 / out %7 packets   gaps %8   snapshots %9   %10")
            .arg(syncRoleName(s.role), s.connected ? "<b style='color:#6c6'>CONNECTED</b>" : "<b style='color:#c66'>no peer</b>", s.peer).arg(s.rttMs, 0, 'f', 2).arg(s.clockOffsetMs, 0, 'f', 1)
            .arg(s.packetsIn).arg(s.packetsOut).arg(s.gaps).arg(s.snapshots).arg(s.lastEvent));
    }); t->start(500);
    refreshUsers();
}

// ================================================================ Plugins
PluginPanel::PluginPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    m_table = new QTableWidget(0, 8); m_table->setHorizontalHeaderLabels({"Enabled", "Name", "ID", "Version", "Vendor", "Category", "Provides", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_table->verticalHeader()->hide();
    v->addWidget(m_table, 1);
    auto* h = new QHBoxLayout; auto* inst = new QPushButton("Install plugin..."); auto* rem = new QPushButton("Remove"); auto* rescan = new QPushButton("Rescan");
    h->addWidget(inst); h->addWidget(rem); h->addWidget(rescan); h->addStretch(); v->addLayout(h);
    auto* note = new QLabel(QString("Plugin folders: %1\nPlugins are validated in a separate probe process before loading. Native plugins run in-process once loaded: install only trusted plugins. SDK header: lumaflow_plugin.h (API v%2).")
        .arg(c->engine->plugins().pluginDirs().join("; ")).arg(LF_PLUGIN_API_VERSION));
    note->setWordWrap(true); note->setStyleSheet("color:#8088b0;"); v->addWidget(note);
    connect(inst, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Install plugin", {}, "Plugins (*.so *.dll *.dylib)"); if (f.isEmpty()) return;
        QString err; if (!c->engine->plugins().install(f, &err)) QMessageBox::warning(this, "Install failed", err);
        else { c->engine->plugins().scanAndLoad(&c->engine->effects(), c->engine->options().safeMode); refresh(); emit c->engine->showStateChanged(); }
    });
    connect(rem, &QPushButton::clicked, this, [this] { const int r = m_table->currentRow(); if (r < 0) return; QString err; c->engine->plugins().uninstall(c->engine->plugins().records()[r].id, &err); if (!err.isEmpty()) QMessageBox::information(this, "Remove", err); refresh(); });
    connect(rescan, &QPushButton::clicked, this, [this] { c->engine->plugins().scanAndLoad(&c->engine->effects(), c->engine->options().safeMode); refresh(); emit c->engine->showStateChanged(); });
    refresh();
}

void PluginPanel::refresh() {
    const auto recs = c->engine->plugins().records();
    m_table->setRowCount(recs.size());
    for (int i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        auto* en = new QCheckBox; en->setChecked(r.enabled); m_table->setCellWidget(i, 0, en);
        connect(en, &QCheckBox::toggled, this, [this, id = r.id](bool b) { c->engine->plugins().setEnabled(id, b); });
        const QStringList cols{"", r.name, r.id, r.version, r.vendor, PluginHost::categoryName(r.category), QString("%1 effects, %2 nodes, %3 outputs").arg(r.effects).arg(r.nodes).arg(r.outputs), r.status};
        for (int k = 1; k < cols.size(); ++k) m_table->setItem(i, k, new QTableWidgetItem(cols[k]));
    }
}

// ================================================================ AI assistant
AiPanel::AiPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* intro = new QLabel("Describe an effect. The built-in generator runs locally and offline. Generated shaders and node graphs are compiled in an isolated validation program before you can activate them.");
    intro->setWordWrap(true); v->addWidget(intro);
    m_prompt = new QLineEdit; m_prompt->setPlaceholderText("e.g. Create a blue particle vortex / Generate a red and white centre-out chase / Create an audio-reactive plasma effect");
    v->addWidget(m_prompt);
    auto* h = new QHBoxLayout;
    auto* gen = new QPushButton("Generate (local)"); auto* ext = new QPushButton("Generate with external model"); m_apply = new QPushButton("Validate && apply to preview"); auto* prog = new QPushButton("Send preview to program (TAKE)");
    h->addWidget(gen); h->addWidget(ext); h->addWidget(m_apply); h->addWidget(prog); h->addStretch(); v->addLayout(h);
    auto* examples = new QComboBox; examples->addItems({"Examples...", "Create a blue particle vortex", "Generate a red and white centre-out chase", "Create an audio-reactive plasma effect",
                                                        "Generate a symmetrical effect using fixture positions", "Create a slow colour wave across LED strips", "Combine fractal with particle rings and music"});
    v->addWidget(examples);
    m_out = new QPlainTextEdit; m_out->setReadOnly(true); v->addWidget(m_out, 1);
    auto* cfg = new QGroupBox("External AI (optional, off by default)"); auto* cf = new QFormLayout(cfg);
    auto* en = new QCheckBox("Enable external model requests"); auto* model = new QLineEdit(c->engine->assistant().config().model); auto* env = new QLineEdit(c->engine->assistant().config().apiKeyEnv);
    auto* endpoint = new QLineEdit(c->engine->assistant().config().endpoint);
    cf->addRow(en); cf->addRow("Endpoint", endpoint); cf->addRow("Model", model); cf->addRow("API key environment variable", env);
    auto* privacy = new QLabel("When enabled, your prompt text is sent to the configured endpoint. Keys are read from the environment and never stored in project files."); privacy->setWordWrap(true); privacy->setStyleSheet("color:#8088b0;");
    cf->addRow(privacy);
    v->addWidget(cfg);
    m_apply->setEnabled(false);
    auto applyCfg = [this, en, model, env, endpoint] { AiConfig a; a.externalEnabled = en->isChecked(); a.model = model->text(); a.apiKeyEnv = env->text(); a.endpoint = endpoint->text(); c->engine->assistant().setConfig(a); };
    connect(en, &QCheckBox::toggled, this, applyCfg); connect(model, &QLineEdit::editingFinished, this, applyCfg); connect(env, &QLineEdit::editingFinished, this, applyCfg); connect(endpoint, &QLineEdit::editingFinished, this, applyCfg);
    connect(examples, &QComboBox::activated, this, [this, examples](int i) { if (i > 0) m_prompt->setText(examples->itemText(i)); examples->setCurrentIndex(0); });
    connect(gen, &QPushButton::clicked, this, [this] { show(c->engine->assistant().generateLocal(m_prompt->text(), c->engine->effects().ids())); });
    connect(m_prompt, &QLineEdit::returnPressed, gen, &QPushButton::click);
    connect(ext, &QPushButton::clicked, this, [this] { m_out->setPlainText("Requesting external model..."); c->engine->assistant().requestExternal(m_prompt->text()); });
    connect(&c->engine->assistant(), &EffectAssistant::externalResult, this, [this](const AiSuggestion& s) { show(s); });
    connect(&c->engine->assistant(), &EffectAssistant::externalError, this, [this](const QString& e) { m_out->setPlainText("External AI unavailable: " + e + "\n\nThe local generator remains available."); });
    connect(prog, &QPushButton::clicked, c->engine, [this] { c->engine->take(); });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        Engine* e = c->engine; const int L = c->layer();
        if (m_last.kind == AiSuggestion::Spatial) {
            undoable(e, &c->undo, "AI spatial effect", [&] { QMutexLocker l(&e->stateMutex()); SpatialSlot s; s.fx = m_last.spatial; s.enabled = true; e->spatial() << s; });
            emit e->showStateChanged();
            m_out->appendPlainText("\nAdded as spatial effect (see Spatial Effects panel and 3D Stage).");
            return;
        }
        QString effectId = m_last.effectId;
        if (m_last.kind == AiSuggestion::NodeEffect || m_last.kind == AiSuggestion::ShaderEffect) {
            EffectDef d; d.id = "ai_" + QString::number(qHash(m_prompt->text()), 16); d.name = "AI: " + m_last.title.left(30); d.category = "AI generated"; d.source = "ai";
            QString err;
            if (m_last.kind == AiSuggestion::NodeEffect) { const auto r = m_last.graph.compile(); if (!r.ok) { m_out->appendPlainText("\nGraph errors: " + r.errors.join("; ")); return; } d.glsl = r.glsl; d.nodeGraph = m_last.graph.toJson(); }
            else d.glsl = m_last.glsl;
            for (int i = 0; i < 4; ++i) d.params[i] = {"-", 0.5};
            d.colorA = m_last.colorA; d.colorB = m_last.colorB;
            if (!e->addCustomEffect(d, &err)) {
                m_out->appendPlainText("\nValidation FAILED (not activated):\n" + err);
                if (m_last.kind == AiSuggestion::ShaderEffect && e->assistant().config().externalEnabled) { m_out->appendPlainText("Requesting an automatic repair from the external model..."); e->assistant().requestExternal(m_prompt->text(), err); }
                return;
            }
            effectId = d.id;
        }
        e->setLayerEffect(L, effectId, true);
        LayerSource s = e->preview().layers[L]; s.colorA = m_last.colorA; s.colorB = m_last.colorB; e->setLayerSource(L, s, true);
        for (auto it = m_last.layerValues.begin(); it != m_last.layerValues.end(); ++it) e->preview().params.set(Composition::pid(L, it.key().toLatin1().constData()), it.value().toDouble(), "ai");
        e->preview().params.set(Composition::pid(L, "opacity"), 1.0, "ai");
        m_out->appendPlainText(QString("\nValidated and applied to PREVIEW layer %1. Press TAKE to send to program.").arg(L + 1));
    });
}

void AiPanel::show(const AiSuggestion& s) {
    m_last = s;
    QString text = "Suggestion: " + s.title + "\n" + s.explanation + "\n";
    if (!s.mappings.isEmpty()) text += "Mappings: " + s.mappings.join("; ") + "\n";
    if (s.kind == AiSuggestion::NodeEffect) { const auto r = s.graph.compile(); text += QString("\nNode graph: %1 nodes (%2)\n").arg(s.graph.nodes.size()).arg(r.ok ? "compiles" : "errors: " + r.errors.join("; ")); text += r.glsl; }
    if (s.kind == AiSuggestion::ShaderEffect) text += "\n" + s.glsl;
    if (s.kind == AiSuggestion::Spatial) text += "\n" + QString::fromUtf8(QJsonDocument(s.spatial.toJson()).toJson());
    if (s.kind == AiSuggestion::LayerPreset) text += QString("\nBase effect: %1\nValues: %2").arg(s.effectId, QString::fromUtf8(QJsonDocument(s.layerValues).toJson(QJsonDocument::Compact)));
    m_out->setPlainText(text);
    m_apply->setEnabled(true);
}

// ================================================================ Performance
PerformancePanel::PerformancePanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* h = new QHBoxLayout;
    auto* quality = new QComboBox; quality->addItems({"Quality: Low (50%)", "Quality: Medium (75%)", "Quality: High (100%)", "Quality: Adaptive"}); quality->setCurrentIndex(3);
    auto* fps = new QSpinBox; fps->setRange(10, 144); fps->setValue(c->engine->renderThread() ? int(c->engine->renderThread()->targetFps()) : 60); fps->setPrefix("Target "); fps->setSuffix(" fps");
    h->addWidget(quality); h->addWidget(fps); h->addStretch(); v->addLayout(h);
    m_fps = new HistoryGraph("FPS", 120); m_fps->setReference(fps->value()); v->addWidget(m_fps);
    m_ms = new HistoryGraph("Frame time (ms)", 50); m_ms->setReference(1000.0 / fps->value()); v->addWidget(m_ms);
    m_info = new QLabel; m_info->setWordWrap(true); m_info->setStyleSheet("font-family: monospace;"); v->addWidget(m_info);
    v->addStretch();
    connect(quality, &QComboBox::currentIndexChanged, this, [this](int i) { if (c->engine->renderThread()) c->engine->renderThread()->setQualityMode(QualityMode(i)); });
    connect(fps, &QSpinBox::valueChanged, this, [this](int f) { if (c->engine->renderThread()) c->engine->renderThread()->setTargetFps(f); m_fps->setReference(f); m_ms->setReference(1000.0 / f); });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, [this] {
        const EngineStatus s = c->engine->status();
        m_fps->push(s.render.fps); m_ms->push(s.render.frameMs);
        if (!isVisible()) return;
        m_info->setText(QString("GPU: %1\nGL: %2\nframe %3 ms (submit %4 ms, GPU+readback %5 ms, readback %6 ms)\nrender scale %7%   frames %8   programs cached %9\n"
                                "texture memory (LUMAFLOW) %10 MB   GPU free memory %11\nnetwork: Art-Net out %12, sACN out %13, errors %14   audio level %15")
            .arg(s.render.glRenderer, s.render.glVersion).arg(s.render.frameMs, 0, 'f', 2).arg(s.render.cpuMs, 0, 'f', 2).arg(s.render.gpuMs, 0, 'f', 2).arg(s.render.readbackMs, 0, 'f', 2)
            .arg(int(s.render.qualityScale * 100)).arg(s.render.frames).arg(s.render.programsCached).arg(s.render.textureMemoryKB / 1024.0, 0, 'f', 1)
            .arg(s.render.gpuMemoryKB >= 0 ? QString("%1 MB").arg(s.render.gpuMemoryKB / 1024) : "n/a (driver does not report)")
            .arg(s.network.artnetOut).arg(s.network.sacnOut).arg(s.network.errors).arg(s.audio.level, 0, 'f', 3));
    }); t->start(250);
}

// ================================================================ Recovery
RecoveryPanel::RecoveryPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* split = new QSplitter(Qt::Vertical);
    auto* top = new QWidget; auto* tv = new QVBoxLayout(top); tv->setContentsMargins(0, 0, 0, 0);
    auto* tl = new QLabel(c->engine->options().safeMode ? "SAFE MODE ACTIVE: plugins, custom effects, DMX output and remote control are disabled." : "Faults and recommended actions"); tl->setObjectName(c->engine->options().safeMode ? "warning" : "title");
    tv->addWidget(tl);
    m_faults = new QTableWidget(0, 5); m_faults->setHorizontalHeaderLabels({"Time", "Severity", "Subsystem", "Message", "Recommended action"});
    m_faults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); m_faults->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch); m_faults->verticalHeader()->hide();
    tv->addWidget(m_faults, 1);
    auto* fh = new QHBoxLayout; auto* clr = new QPushButton("Clear resolved"); auto* ack = new QPushButton("Mark selected resolved"); fh->addWidget(clr); fh->addWidget(ack); fh->addStretch(); tv->addLayout(fh);
    split->addWidget(top);
    auto* mid = new QWidget; auto* mv = new QVBoxLayout(mid); mv->setContentsMargins(0, 0, 0, 0);
    mv->addWidget(new QLabel("Backups (versioned on every save) and autosave"));
    m_backups = new QListWidget; mv->addWidget(m_backups, 1);
    auto* bh = new QHBoxLayout; auto* restore = new QPushButton("Restore selected backup..."); auto* autosave = new QPushButton("Load autosave"); auto* lkg = new QPushButton("Load last-known-good"); auto* saveNow = new QPushButton("Autosave now");
    bh->addWidget(restore); bh->addWidget(autosave); bh->addWidget(lkg); bh->addWidget(saveNow); bh->addStretch(); mv->addLayout(bh);
    split->addWidget(mid);
    m_log = new QPlainTextEdit; m_log->setReadOnly(true); m_log->setMaximumBlockCount(2000); m_log->setStyleSheet("font-family: monospace; font-size: 9pt;");
    split->addWidget(m_log);
    v->addWidget(split);
    connect(clr, &QPushButton::clicked, this, [this] { FaultRegistry::instance().clearResolved(); refresh(); });
    connect(ack, &QPushButton::clicked, this, [this] { const int r = m_faults->currentRow(); if (r < 0) return; const auto f = FaultRegistry::instance().faults()[r]; FaultRegistry::instance().resolve(f.subsystem, f.message); refresh(); });
    auto loadFile = [this](const QString& path) {
        QStringList w; QString err;
        if (!c->engine->loadProject(path, &w, &err)) QMessageBox::warning(this, "Load", err);
        else QMessageBox::information(this, "Loaded", "Loaded " + path + (w.isEmpty() ? QString() : "\n\n" + w.join("\n")));
    };
    connect(restore, &QPushButton::clicked, this, [this, loadFile] {
        auto* it = m_backups->currentItem(); if (!it) return;
        const QString target = c->engine->projectPath();
        if (target.isEmpty()) { loadFile(it->data(Qt::UserRole).toString()); return; }
        if (QMessageBox::question(this, "Restore backup", "Replace " + target + " with this backup? The current file is kept as .before-restore.") != QMessageBox::Yes) return;
        QString err; if (!ProjectIO::restoreBackup(it->data(Qt::UserRole).toString(), target, &err)) QMessageBox::warning(this, "Restore", err); else loadFile(target);
    });
    connect(autosave, &QPushButton::clicked, this, [this, loadFile] { loadFile(c->engine->session().autosavePath()); });
    connect(lkg, &QPushButton::clicked, this, [this, loadFile] { loadFile(c->engine->session().lastKnownGoodPath()); });
    connect(saveNow, &QPushButton::clicked, this, [this] { c->engine->autosaveNow(); refresh(); });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &RecoveryPanel::refresh); t->start(1000);
    refresh();
}

void RecoveryPanel::refresh() {
    auto& fr = FaultRegistry::instance();
    if (fr.revision() != m_rev) {
        m_rev = fr.revision();
        const auto faults = fr.faults();
        static const char* sev[] = {"info", "warning", "ERROR", "CRITICAL"};
        m_faults->setRowCount(faults.size());
        for (int i = 0; i < faults.size(); ++i) {
            const auto& f = faults[i];
            const QStringList cols{f.time.toString("HH:mm:ss"), sev[int(f.severity)], f.subsystem, f.message + (f.count > 1 ? QString(" (x%1)").arg(f.count) : ""), f.recommendation};
            for (int k = 0; k < 5; ++k) { auto* it = new QTableWidgetItem(cols[k]); if (f.severity >= FaultSeverity::Error) it->setForeground(QColor(255, 120, 120)); m_faults->setItem(i, k, it); }
        }
    }
    if (!isVisible()) return;
    const QStringList bks = ProjectIO::listBackups(c->engine->session().backupDir());
    if (bks.size() != m_backups->count()) {
        m_backups->clear();
        for (auto& b : bks) { auto* it = new QListWidgetItem(QFileInfo(b).fileName() + "  (" + QFileInfo(b).lastModified().toString("yyyy-MM-dd HH:mm:ss") + ")"); it->setData(Qt::UserRole, b); m_backups->addItem(it); }
    }
    if (Log::instance().sequence() != m_logSeq) {
        m_logSeq = Log::instance().sequence();
        QStringList lines; for (auto& e : Log::instance().recent(300)) lines << QString("%1 %2 %3: %4").arg(e.time.toString("HH:mm:ss.zzz"), Log::levelName(e.level), e.category, e.message);
        m_log->setPlainText(lines.join('\n')); m_log->moveCursor(QTextCursor::End);
    }
}

} // namespace lf
