#include "StagePanels.h"
#include "Widgets.h"
#include "UndoCommands.h"
#include "app/Engine.h"
#include "fixtures/Mvr.h"
#include "fixtures/Gdtf.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGroupBox>

namespace lf {

// ================================================================ Fixtures
FixturePanel::FixturePanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* add = new QHBoxLayout;
    m_type = new QComboBox; m_mode = new QComboBox; m_count = new QSpinBox; m_count->setRange(1, 512); m_count->setValue(1);
    m_univ = new QSpinBox; m_univ->setRange(1, 63999); m_univ->setPrefix("U ");
    auto* addBtn = new QPushButton("Add fixtures"); auto* del = new QPushButton("Delete");
    add->addWidget(m_type, 2); add->addWidget(m_mode, 1); add->addWidget(new QLabel("x")); add->addWidget(m_count); add->addWidget(m_univ); add->addWidget(addBtn); add->addWidget(del);
    v->addLayout(add);
    auto* io = new QHBoxLayout;
    auto* impMvr = new QPushButton("Import MVR..."); auto* impGdtf = new QPushButton("Import GDTF..."); auto* expMvr = new QPushButton("Export MVR...");
    auto* group = new QPushButton("Group selected..."); auto* demo = new QPushButton("Load demo rig");
    io->addWidget(impMvr); io->addWidget(impGdtf); io->addWidget(expMvr); io->addWidget(group); io->addStretch(); io->addWidget(demo);
    v->addLayout(io);
    m_table = new QTableWidget(0, 11);
    m_table->setHorizontalHeaderLabels({"FID", "Name", "Type", "Mode", "Univ", "Addr", "X", "Y", "Z", "Groups", "Capabilities"});
    m_table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch); m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_table, 1);
    m_caps = new QLabel; m_caps->setWordWrap(true); v->addWidget(m_caps);
    m_issues = new QPlainTextEdit; m_issues->setReadOnly(true); m_issues->setMaximumHeight(80); m_issues->setPlaceholderText("Patch validation: no issues"); v->addWidget(m_issues);
    auto fillTypes = [this] {
        m_type->clear();
        for (auto& t : FixtureLibrary::builtin().all()) m_type->addItem(t.manufacturer + " " + t.name, t.id);
        for (auto& t : c->engine->patch().types.all()) m_type->addItem("[show] " + t.manufacturer + " " + t.name, t.id);
    };
    fillTypes();
    connect(m_type, &QComboBox::currentIndexChanged, this, [this] {
        m_mode->clear(); const QString id = m_type->currentData().toString();
        const FixtureType* t = c->engine->patch().types.find(id); if (!t) t = FixtureLibrary::builtin().find(id);
        if (t) for (auto& m : t->modes) m_mode->addItem(QString("%1 (%2 ch)").arg(m.name).arg(m.footprint()), m.name);
    });
    emit m_type->currentIndexChanged(0);
    connect(addBtn, &QPushButton::clicked, this, [this] {
        undoable(c->engine, &c->undo, "Add fixtures", [&] {
            QMutexLocker l(&c->engine->stateMutex());
            Patch& p = c->engine->patch();
            FixtureInstance tmp; tmp.typeId = m_type->currentData().toString(); tmp.modeName = m_mode->currentData().toString();
            const int fp = p.footprintOf(tmp);
            int fid = 1; for (auto& f : p.fixtures) fid = std::max(fid, f.fid + 1);
            for (int i = 0; i < m_count->value(); ++i) {
                FixtureInstance f = tmp; f.fid = fid++; f.name = QString("%1 %2").arg(m_type->currentText().section(' ', 1)).arg(i + 1);
                int u = m_univ->value(), a = p.nextFreeAddress(u, fp);
                while (a < 0 && u < 63999) { ++u; a = p.nextFreeAddress(u, fp); }
                f.universe = u; f.address = a; f.position = Vec3(-0.5 * (m_count->value() - 1) + i, 0, 4);
                p.fixtures << f;
            }
        });
        refresh();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        QSet<int> rows; for (auto* it : m_table->selectedItems()) rows.insert(it->row());
        undoable(c->engine, &c->undo, "Delete fixtures", [&] { QMutexLocker l(&c->engine->stateMutex()); QList<int> r = rows.values(); std::sort(r.rbegin(), r.rend()); for (int i : r) c->engine->patch().fixtures.removeAt(i); });
        refresh();
    });
    connect(impMvr, &QPushButton::clicked, this, [this, fillTypes] {
        const QString f = QFileDialog::getOpenFileName(this, "Import MVR", {}, "My Virtual Rig (*.mvr)"); if (f.isEmpty()) return;
        const auto r = mvr::importFile(f);
        if (!r.ok()) { QMessageBox::warning(this, "MVR import failed", r.error); return; }
        undoable(c->engine, &c->undo, "Import MVR", [&] { QMutexLocker l(&c->engine->stateMutex()); c->engine->patch() = r.patch; });
        fillTypes(); refresh();
        m_issues->setPlainText(QString("Imported %1 fixtures from %2\n").arg(r.patch.fixtures.size()).arg(f) + r.warnings.join("\n"));
    });
    connect(impGdtf, &QPushButton::clicked, this, [this, fillTypes] {
        const QString f = QFileDialog::getOpenFileName(this, "Import GDTF", {}, "GDTF (*.gdtf)"); if (f.isEmpty()) return;
        const auto r = gdtf::importFile(f);
        if (!r.ok()) { QMessageBox::warning(this, "GDTF import failed", r.error); return; }
        { QMutexLocker l(&c->engine->stateMutex()); c->engine->patch().types.add(r.type); }
        fillTypes(); m_type->setCurrentIndex(m_type->findData(r.type.id));
        QStringList caps; for (auto& m : r.type.modes) caps << QString("%1: %2 ch - %3").arg(m.name).arg(m.footprint()).arg(analyzeCapabilities(m).describe().join(", "));
        m_issues->setPlainText("Imported " + r.type.manufacturer + " " + r.type.name + "\n" + caps.join("\n") + (r.warnings.isEmpty() ? "" : "\nWarnings:\n" + r.warnings.join("\n")));
    });
    connect(expMvr, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, "Export MVR", "rig.mvr", "My Virtual Rig (*.mvr)"); if (f.isEmpty()) return;
        QString err; if (!mvr::exportFile(f, c->engine->patch(), &err)) QMessageBox::warning(this, "Export failed", err);
    });
    connect(group, &QPushButton::clicked, this, [this] {
        bool ok; const QString g = QInputDialog::getText(this, "Group", "Group name:", QLineEdit::Normal, "Group 1", &ok); if (!ok) return;
        QSet<int> rows; for (auto* it : m_table->selectedItems()) rows.insert(it->row());
        undoable(c->engine, &c->undo, "Group fixtures", [&] { for (int r : rows) { auto& gs = c->engine->patch().fixtures[r].groups; if (!gs.contains(g)) gs << g; } });
        refresh();
    });
    connect(demo, &QPushButton::clicked, this, [this, fillTypes] { undoable(c->engine, &c->undo, "Demo rig", [&] { c->engine->createDemoShow(); }); fillTypes(); refresh(); });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int r, int col) {
        if (m_updating) return;
        QMutexLocker l(&c->engine->stateMutex());
        auto& fx = c->engine->patch().fixtures; if (r >= fx.size()) return;
        FixtureInstance& f = fx[r]; const QString t = m_table->item(r, col)->text();
        switch (col) { case 0: f.fid = t.toInt(); break; case 1: f.name = t; break; case 4: f.universe = std::max(1, t.toInt()); break; case 5: f.address = std::clamp(t.toInt(), 1, 512); break;
        case 6: f.position.x = t.toDouble(); break; case 7: f.position.y = t.toDouble(); break; case 8: f.position.z = t.toDouble(); break;
        case 9: f.groups = t.split(',', Qt::SkipEmptyParts); for (auto& g : f.groups) g = g.trimmed(); break; default: break; }
        l.unlock(); QTimer::singleShot(0, this, &FixturePanel::refresh);
    });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int r) {
        if (r < 0 || r >= c->engine->patch().fixtures.size()) return;
        const auto& f = c->engine->patch().fixtures[r]; const FixtureMode* m = c->engine->patch().modeOf(f);
        m_caps->setText(m ? QString("<b>%1</b> - %2 channels: %3").arg(f.name).arg(m->footprint()).arg(analyzeCapabilities(*m).describe().join(", ")) : "unknown type");
    });
    connect(c->engine, &Engine::projectLoaded, this, [this, fillTypes] { fillTypes(); refresh(); });
    refresh();
}

void FixturePanel::refresh() {
    m_updating = true;
    const Patch& p = c->engine->patch();
    m_table->setRowCount(p.fixtures.size());
    for (int i = 0; i < p.fixtures.size(); ++i) {
        const auto& f = p.fixtures[i]; const FixtureType* t = p.typeOf(f); const FixtureMode* m = p.modeOf(f);
        const QStringList cols{QString::number(f.fid), f.name, t ? t->name : f.typeId, f.modeName.isEmpty() && m ? m->name : f.modeName, QString::number(f.universe), QString::number(f.address),
                               QString::number(f.position.x, 'f', 2), QString::number(f.position.y, 'f', 2), QString::number(f.position.z, 'f', 2), f.groups.join(", "), m ? analyzeCapabilities(*m).describe().join(", ") : "?"};
        for (int k = 0; k < cols.size(); ++k) { auto* it = new QTableWidgetItem(cols[k]); if (k == 2 || k == 3 || k == 10) it->setFlags(it->flags() & ~Qt::ItemIsEditable); m_table->setItem(i, k, it); }
    }
    QStringList issues; for (auto& is : p.validate()) issues << is.message;
    m_issues->setPlainText(issues.join("\n"));
    m_updating = false;
}

// ================================================================ Pixel mapper
PixelMapperPanel::PixelMapperPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout;
    auto* strip = new QPushButton("+ Strip"); auto* matrix = new QPushButton("+ Matrix"); auto* fromPatch = new QPushButton("+ From patch (front)"); auto* fromTop = new QPushButton("+ From patch (top)");
    auto* del = new QPushButton("Delete"); m_test = new QComboBox; m_test->addItems({"Test: off (live)", "Test: full white", "Test: identify", "Test: chase"});
    auto* gamma = new QDoubleSpinBox; gamma->setRange(1.0, 3.0); gamma->setSingleStep(0.1); gamma->setPrefix("gamma "); gamma->setValue(c->engine->pixelMap().globalGamma);
    auto* bright = new QDoubleSpinBox; bright->setRange(0, 1); bright->setSingleStep(0.05); bright->setPrefix("limit "); bright->setValue(c->engine->pixelMap().masterBrightness);
    for (QWidget* w : std::initializer_list<QWidget*>{strip, matrix, fromPatch, fromTop, del, m_test, gamma, bright}) bar->addWidget(w);
    bar->addStretch(); v->addLayout(bar);
    auto* split = new QSplitter(Qt::Vertical);
    m_view = new ImageView("PIXEL MAP (program canvas)");
    m_view->setOverlayPainter([this](QPainter& p, const QRectF& r) {
        QMutexLocker l(&c->engine->stateMutex());
        const auto& els = c->engine->pixelMap().elements;
        const int sel = m_table->currentRow();
        for (int i = 0; i < els.size(); ++i) {
            const auto& pts = els[i].kind == PixelMapElement::Fixture ? els[i].cellPoints : els[i].points;
            p.setPen(QPen(i == sel ? QColor(255, 220, 0) : QColor(0, 255, 200, 200), i == sel ? 2 : 1)); p.setBrush(Qt::NoBrush);
            QPointF prev;
            for (int k = 0; k < pts.size(); ++k) {
                const QPointF pt(r.left() + pts[k].x() * r.width(), r.top() + pts[k].y() * r.height());
                p.drawEllipse(pt, 3, 3); if (k > 0 && els[i].kind != PixelMapElement::Fixture) p.drawLine(prev, pt); prev = pt;
            }
            if (!pts.isEmpty()) { p.setPen(Qt::white); p.drawText(QPointF(r.left() + pts[0].x() * r.width() + 5, r.top() + pts[0].y() * r.height() - 5), els[i].name); }
        }
    });
    split->addWidget(m_view);
    auto* lower = new QWidget; auto* lv = new QVBoxLayout(lower); lv->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({"Name", "Kind", "Univ", "Addr", "Order", "Pixels", "Split", "On"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); m_table->verticalHeader()->hide(); m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    lv->addWidget(m_table, 1);
    m_stats = new QLabel; m_stats->setWordWrap(true); lv->addWidget(m_stats);
    split->addWidget(lower);
    v->addWidget(split, 1);
    auto mod = [this](const QString& what, const std::function<void(PixelMap&)>& fn) { undoable(c->engine, &c->undo, what, [&] { QMutexLocker l(&c->engine->stateMutex()); fn(c->engine->pixelMap()); }); refresh(); };
    connect(strip, &QPushButton::clicked, this, [this, mod] {
        bool ok; const int n = QInputDialog::getInt(this, "LED strip", "Pixels:", 60, 1, 4096, 1, &ok); if (!ok) return;
        mod("Add strip", [&](PixelMap& pm) { const double y = 0.1 + 0.08 * (pm.elements.size() % 10); pm.elements << PixelMap::makeStrip(QString("Strip %1").arg(pm.elements.size() + 1), {0.05, y}, {0.95, y}, n, 10 + int(pm.elements.size()), 1); });
    });
    connect(matrix, &QPushButton::clicked, this, [this, mod] {
        bool ok; const QString s = QInputDialog::getText(this, "LED matrix", "Columns x rows:", QLineEdit::Normal, "32x16", &ok); if (!ok) return;
        const int w = s.section('x', 0, 0).toInt(), h = s.section('x', 1, 1).toInt(); if (w < 1 || h < 1) return;
        mod("Add matrix", [&](PixelMap& pm) { pm.elements << PixelMap::makeMatrix(QString("Matrix %1x%2").arg(w).arg(h), QRectF(0.1, 0.1, 0.8, 0.8), w, h, 20 + int(pm.elements.size()), 1, PixelOrder::RGB, true); });
    });
    connect(fromPatch, &QPushButton::clicked, this, [this, mod] { mod("Map patch", [&](PixelMap& pm) { pm.elements << PixelMap::makeFromPatch(c->engine->patch(), PixelMap::Projection::Front); }); });
    connect(fromTop, &QPushButton::clicked, this, [this, mod] { mod("Map patch", [&](PixelMap& pm) { pm.elements << PixelMap::makeFromPatch(c->engine->patch(), PixelMap::Projection::Top); }); });
    connect(del, &QPushButton::clicked, this, [this, mod] { const int r = m_table->currentRow(); if (r >= 0) mod("Delete element", [&](PixelMap& pm) { pm.elements.removeAt(r); }); });
    connect(m_test, &QComboBox::currentIndexChanged, c->engine, &Engine::setPixelTestPattern);
    connect(gamma, &QDoubleSpinBox::valueChanged, this, [this](double g) { QMutexLocker l(&c->engine->stateMutex()); c->engine->pixelMap().globalGamma = g; });
    connect(bright, &QDoubleSpinBox::valueChanged, this, [this](double b) { QMutexLocker l(&c->engine->stateMutex()); c->engine->pixelMap().masterBrightness = b; });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int r, int col) {
        if (m_updating) return;
        QMutexLocker l(&c->engine->stateMutex());
        auto& els = c->engine->pixelMap().elements; if (r >= els.size()) return;
        const QString t = m_table->item(r, col)->text();
        if (col == 0) els[r].name = t; else if (col == 2) els[r].universe = std::max(1, t.toInt()); else if (col == 3) els[r].address = std::clamp(t.toInt(), 1, 512);
        else if (col == 4) els[r].order = pixelOrderFromName(t); else if (col == 6) els[r].split = t.startsWith("C", Qt::CaseInsensitive) ? UniverseSplit::Continuous : UniverseSplit::PixelAligned;
        l.unlock(); QTimer::singleShot(0, this, &PixelMapperPanel::refresh);
    });
    connect(m_table, &QTableWidget::currentCellChanged, m_view, [this] { m_view->update(); });
    connect(c->engine, &Engine::frameReady, this, [this] { if (isVisible()) m_view->setImage(c->engine->lastProgramFrame()); });
    connect(c->engine, &Engine::projectLoaded, this, &PixelMapperPanel::refresh);
    connect(c->engine, &Engine::showStateChanged, this, &PixelMapperPanel::refresh);
    refresh();
}

void PixelMapperPanel::refresh() {
    m_updating = true;
    const PixelMap& pm = c->engine->pixelMap();
    m_table->setRowCount(pm.elements.size());
    static const char* kinds[] = {"Strip", "Matrix", "Fixture"};
    for (int i = 0; i < pm.elements.size(); ++i) {
        const auto& e = pm.elements[i];
        const FixtureInstance* f = e.kind == PixelMapElement::Fixture ? c->engine->patch().byFid(e.fid) : nullptr;
        const QStringList cols{e.name, kinds[e.kind], QString::number(f ? f->universe : e.universe), QString::number(f ? f->address : e.address), e.kind == PixelMapElement::Fixture ? "(fixture)" : pixelOrderName(e.order),
                               QString::number(e.kind == PixelMapElement::Fixture ? e.cellPoints.size() : e.points.size()), e.split == UniverseSplit::Continuous ? "Continuous" : "Pixel-aligned", e.enabled ? "yes" : "no"};
        for (int k = 0; k < cols.size(); ++k) { auto* it = new QTableWidgetItem(cols[k]); if (k == 1 || k == 5 || k == 7 || (f && (k == 2 || k == 3))) it->setFlags(it->flags() & ~Qt::ItemIsEditable); m_table->setItem(i, k, it); }
    }
    const auto st = pm.analyze(c->engine->patch());
    m_stats->setText(QString("%1 elements, %2 pixels, %3 DMX channels across %4 universe(s). %5")
        .arg(pm.elements.size()).arg(st.pixels).arg(st.channels).arg(st.universes)
        .arg(st.warnings.isEmpty() ? "<span style='color:#6c6'>No conflicts.</span>" : "<span style='color:#fb4'>" + st.warnings.mid(0, 6).join("<br>") + "</span>"));
    m_updating = false;
    m_view->update();
}

// ================================================================ 3D stage
StageView::StageView(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) { setMinimumSize(320, 220); setFocusPolicy(Qt::ClickFocus); }

QPointF StageView::project(const Vec3& p, double* depth) const {
    const double w = width(), h = height(), s = std::min(w, h) / std::max(1.0, m_dist) * 1.2;
    if (m_cam == Top) { if (depth) *depth = -p.z; return {w / 2 + (p.x - m_target.x) * s, h / 2 - (p.y - m_target.y) * s}; }
    if (m_cam == Front) { if (depth) *depth = p.y; return {w / 2 + (p.x - m_target.x) * s, h / 2 - (p.z - m_target.z) * s}; }
    if (m_cam == Side) { if (depth) *depth = p.x; return {w / 2 + (p.y - m_target.y) * s, h / 2 - (p.z - m_target.z) * s}; }
    const double yaw = m_yaw * M_PI / 180, pitch = m_pitch * M_PI / 180;
    const Vec3 eye = m_target + Vec3(std::sin(yaw) * std::cos(pitch), -std::cos(yaw) * std::cos(pitch), std::sin(pitch)) * m_dist;
    const Vec3 fwd = (m_target - eye).normalized(), right = fwd.cross(Vec3(0, 0, 1)).normalized(), up = right.cross(fwd);
    const Vec3 d = p - eye;
    const double z = std::max(0.1, d.dot(fwd));
    if (depth) *depth = z;
    const double f = std::min(w, h) * 1.1;
    return {w / 2 + d.dot(right) / z * f, h / 2 - d.dot(up) / z * f};
}

static RGB fixtureColour(const FixtureInstance& f, const FixtureMode& m, const QMap<int, DmxFrame>& dmx, int cell) {
    const int base = absoluteSlot(f.universe, f.address);
    auto val = [&](int off) { const int abs = base + off; auto it = dmx.find(abs / kDmxSlots + 1); return it == dmx.end() ? 0.0 : it.value()[abs % kDmxSlots] / 255.0; };
    double r = 0, g = 0, b = 0, w = 0, dim = 1; bool hasDim = false, hasCol = false;
    for (auto& ch : m.channels) {
        if (ch.cell != cell && ch.attribute != Attribute::Dimmer) continue;
        switch (ch.attribute) { case Attribute::Red: r = val(ch.offset); hasCol = true; break; case Attribute::Green: g = val(ch.offset); break; case Attribute::Blue: b = val(ch.offset); break;
        case Attribute::White: w = val(ch.offset); break; case Attribute::Amber: r += val(ch.offset) * 0.8; g += val(ch.offset) * 0.4; break;
        case Attribute::Dimmer: dim = val(ch.offset); hasDim = true; break; default: break; }
    }
    if (!hasCol) return {dim, dim * 0.95, dim * 0.85};
    return {std::min(1.0, (r + w) * (hasDim ? dim : 1)), std::min(1.0, (g + w) * (hasDim ? dim : 1)), std::min(1.0, (b + w) * (hasDim ? dim : 1))};
}

void StageView::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient bg(0, 0, 0, height()); bg.setColorAt(0, QColor(10, 11, 20)); bg.setColorAt(1, QColor(22, 22, 36)); p.fillRect(rect(), bg);
    // stage floor grid 20 x 12 m
    p.setPen(QColor(45, 48, 75));
    for (int x = -10; x <= 10; ++x) p.drawLine(project(Vec3(x, -6, 0)), project(Vec3(x, 6, 0)));
    for (int y = -6; y <= 6; ++y) p.drawLine(project(Vec3(-10, y, 0)), project(Vec3(10, y, 0)));
    p.setPen(QPen(QColor(90, 100, 160), 2)); p.drawLine(project(Vec3(-10, -6, 0)), project(Vec3(10, -6, 0)));   // stage front edge
    p.setPen(QColor(120, 130, 200)); p.drawText(project(Vec3(0, -6.8, 0)), "FRONT / AUDIENCE");
    const Patch& patch = c->engine->patch();
    const auto dmx = c->engine->lastDmxOutput();
    // trusses: horizontal line through each group of fixtures at equal height & depth
    QMap<QPair<int, int>, QPair<double, double>> truss;
    for (auto& f : patch.fixtures) { auto key = qMakePair(int(std::round(f.position.y * 2)), int(std::round(f.position.z * 2))); auto& t = truss[key]; if (t.first == 0 && t.second == 0) t = {f.position.x, f.position.x}; t.first = std::min(t.first, f.position.x); t.second = std::max(t.second, f.position.x); }
    p.setPen(QPen(QColor(110, 110, 130), 3));
    for (auto it = truss.begin(); it != truss.end(); ++it) if (it.key().second > 2) { const double y = it.key().first / 2.0, z = it.key().second / 2.0 + 0.25; p.drawLine(project(Vec3(it->first - 0.7, y, z)), project(Vec3(it->second + 0.7, y, z))); }
    // LED screen placement (program frame on a virtual screen upstage)
    const QImage frame = c->engine->lastProgramFrame();
    if (!frame.isNull() && m_cam != Top) {
        const QPointF tl = project(Vec3(-4, 6, 6)), tr = project(Vec3(4, 6, 6)), bl = project(Vec3(-4, 6, 1.5));
        QTransform t; QPolygonF src(QRectF(0, 0, frame.width(), frame.height())); QPolygonF dst; dst << tl << tr << (tr + bl - tl) << bl;
        if (QTransform::quadToQuad(src, dst, t)) { p.save(); p.setTransform(t); p.setOpacity(0.9); p.drawImage(0, 0, frame); p.restore(); }
    }
    // fixtures sorted far-to-near
    struct Draw { double depth; QPointF pt; const FixtureInstance* f; RGB col; int cell; };
    QVector<Draw> draws;
    for (auto& f : patch.fixtures) {
        const FixtureMode* m = patch.modeOf(f); if (!m) continue;
        const Mat4 rot = Mat4::rotationXYZDeg(f.rotation);
        const int cells = std::max(1, m->cellCount);
        for (int i = 0; i < cells; ++i) {
            const Vec3 pos = f.position + rot.transformPoint(i < m->cellOffsets.size() ? m->cellOffsets[i] : Vec3());
            double d; const QPointF pt = project(pos, &d);
            draws << Draw{d, pt, &f, fixtureColour(f, *m, dmx, i), i};
        }
    }
    std::sort(draws.begin(), draws.end(), [](auto& a, auto& b) { return a.depth > b.depth; });
    for (auto& d : draws) {
        const QColor col = QColor::fromRgbF(float(d.col.r), float(d.col.g), float(d.col.b));
        const double rad = d.f->groups.contains("Tubes") || d.f->groups.contains("Battens") || patch.modeOf(*d.f)->cellCount > 4 ? 3.5 : 7;
        if (d.col.r + d.col.g + d.col.b > 0.05) { QRadialGradient g(d.pt, rad * 3.5); QColor c0 = col; c0.setAlpha(150); g.setColorAt(0, c0); g.setColorAt(1, QColor(0, 0, 0, 0)); p.setBrush(g); p.setPen(Qt::NoPen); p.drawEllipse(d.pt, rad * 3.5, rad * 3.5); }
        p.setBrush(col); p.setPen(QPen(m_sel.contains(d.f->fid) ? QColor(255, 220, 0) : QColor(80, 80, 110), m_sel.contains(d.f->fid) ? 2 : 1));
        p.drawEllipse(d.pt, rad, rad);
    }
    if (showLabels) {
        p.setPen(QColor(200, 205, 235)); QFont f = font(); f.setPointSizeF(7.5); p.setFont(f);
        for (auto& fx : patch.fixtures) { const QPointF pt = project(fx.position); p.drawText(pt + QPointF(8, -8), QString::number(fx.fid)); }
    }
    static const char* names[] = {"Perspective", "Top", "Front", "Side"};
    p.setPen(QColor(140, 180, 255)); p.setFont(font());
    p.drawText(QRect(8, 4, 400, 20), Qt::AlignLeft, QString("%1  |  %2 fixtures  |  drag: orbit/pan  wheel: zoom  click: select (Ctrl: add)").arg(names[m_cam]).arg(patch.fixtures.size()));
}

void StageView::mousePressEvent(QMouseEvent* e) {
    m_last = e->pos();
    if (e->button() != Qt::LeftButton) return;
    int best = -1; double bd = 12;
    for (auto& f : c->engine->patch().fixtures) { const double d = QLineF(project(f.position), e->position()).length(); if (d < bd) { bd = d; best = f.fid; } }
    if (!(e->modifiers() & Qt::ControlModifier)) m_sel.clear();
    if (best >= 0) { if (m_sel.contains(best)) m_sel.remove(best); else m_sel.insert(best); }
    update(); emit selectionChanged();
}

void StageView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint d = e->pos() - m_last; m_last = e->pos();
    if (e->buttons() & Qt::RightButton || (e->buttons() & Qt::LeftButton && e->modifiers() & Qt::ShiftModifier)) {
        const double s = m_dist / std::max(1, std::min(width(), height())) * 1.5;
        m_target = m_target + Vec3(-d.x() * s, 0, d.y() * s);
    } else if (e->buttons() & Qt::MiddleButton || (e->buttons() & Qt::LeftButton && e->modifiers() & Qt::AltModifier)) { m_yaw += d.x() * 0.4; m_pitch = std::clamp(m_pitch + d.y() * 0.4, -5.0, 89.0); }
    update();
}
void StageView::wheelEvent(QWheelEvent* e) { m_dist = std::clamp(m_dist * (e->angleDelta().y() > 0 ? 0.88 : 1.14), 3.0, 120.0); update(); }

StagePanel::StagePanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout;
    auto* cam = new QComboBox; cam->addItems({"Perspective", "Top", "Front", "Side"});
    auto* labels = new QCheckBox("Labels"); labels->setChecked(true);
    auto* orbit = new QPushButton("Orbit ↻"); orbit->setCheckable(true); orbit->setToolTip("Middle-drag (or Alt+drag) to orbit; right-drag (or Shift+drag) to pan");
    auto* mkGroup = new QPushButton("Group from selection...");
    auto* spatialGroup = new QComboBox; spatialGroup->addItems({"Spatial group by...", "Left / right halves", "Front / back rows", "Height levels"});
    m_selLabel = new QLabel;
    bar->addWidget(cam); bar->addWidget(labels); bar->addWidget(orbit); bar->addWidget(mkGroup); bar->addWidget(spatialGroup); bar->addStretch(); bar->addWidget(m_selLabel);
    v->addLayout(bar);
    m_view = new StageView(c); v->addWidget(m_view, 1);
    auto* edit = new QHBoxLayout;
    const char* axes[] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) { m_pos[i] = new QDoubleSpinBox; m_pos[i]->setRange(-200, 200); m_pos[i]->setDecimals(2); m_pos[i]->setSingleStep(0.1); m_pos[i]->setPrefix(QString("%1 ").arg(axes[i])); m_pos[i]->setSuffix(" m"); edit->addWidget(m_pos[i]); }
    for (int i = 0; i < 3; ++i) { m_rot[i] = new QDoubleSpinBox; m_rot[i]->setRange(-360, 360); m_rot[i]->setDecimals(1); m_rot[i]->setPrefix(QString("r%1 ").arg(axes[i])); m_rot[i]->setSuffix("°"); edit->addWidget(m_rot[i]); }
    v->addLayout(edit);
    connect(cam, &QComboBox::currentIndexChanged, this, [this](int i) { m_view->setCamera(StageView::Camera(i)); });
    connect(labels, &QCheckBox::toggled, this, [this](bool b) { m_view->showLabels = b; m_view->update(); });
    auto* timer = new QTimer(this); connect(timer, &QTimer::timeout, this, [this, orbit] { if (!isVisible()) return; if (orbit->isChecked()) m_view->orbit(0.5); else m_view->update(); }); timer->start(50);
    connect(m_view, &StageView::selectionChanged, this, &StagePanel::loadSelection);
    auto apply = [this] {
        if (m_updating) return;
        undoable(c->engine, &c->undo, "Move fixtures", [&] {
            QMutexLocker l(&c->engine->stateMutex());
            const bool one = m_view->selection().size() == 1;
            for (int fid : m_view->selection()) if (auto* f = c->engine->patch().byFid(fid)) {
                if (one) f->position = Vec3(m_pos[0]->value(), m_pos[1]->value(), m_pos[2]->value());
                else f->position.z = m_pos[2]->value();   // multi-selection: set common height
                f->rotation = Vec3(m_rot[0]->value(), m_rot[1]->value(), m_rot[2]->value());
            }
        });
    };
    for (int i = 0; i < 3; ++i) { connect(m_pos[i], &QDoubleSpinBox::editingFinished, this, apply); connect(m_rot[i], &QDoubleSpinBox::editingFinished, this, apply); }
    connect(mkGroup, &QPushButton::clicked, this, [this] {
        if (m_view->selection().isEmpty()) return;
        bool ok; const QString g = QInputDialog::getText(this, "Spatial group", "Group name:", QLineEdit::Normal, "Selection", &ok); if (!ok) return;
        undoable(c->engine, &c->undo, "Create group", [&] { for (int fid : m_view->selection()) if (auto* f = c->engine->patch().byFid(fid)) if (!f->groups.contains(g)) f->groups << g; });
    });
    connect(spatialGroup, &QComboBox::activated, this, [this, spatialGroup](int mode) {
        if (mode == 0) return;
        undoable(c->engine, &c->undo, "Spatial groups", [&] {
            QMutexLocker l(&c->engine->stateMutex());
            for (auto& f : c->engine->patch().fixtures) {
                if (mode == 1) f.groups << (f.position.x < 0 ? "Stage Left" : "Stage Right");
                if (mode == 2) f.groups << (f.position.y < 0 ? "Front Row" : "Back Row");
                if (mode == 3) f.groups << QString("Level %1m").arg(int(std::round(f.position.z)));
                f.groups.removeDuplicates();
            }
        });
        spatialGroup->setCurrentIndex(0);
    });
}

void StagePanel::loadSelection() {
    m_updating = true;
    const auto sel = m_view->selection();
    m_selLabel->setText(sel.isEmpty() ? "No selection" : QString("%1 selected").arg(sel.size()));
    if (!sel.isEmpty()) if (const auto* f = c->engine->patch().byFid(*sel.begin())) {
        m_pos[0]->setValue(f->position.x); m_pos[1]->setValue(f->position.y); m_pos[2]->setValue(f->position.z);
        m_rot[0]->setValue(f->rotation.x); m_rot[1]->setValue(f->rotation.y); m_rot[2]->setValue(f->rotation.z);
    }
    m_updating = false;
}

// ================================================================ Spatial effects
SpatialPanel::SpatialPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* h = new QHBoxLayout(this); h->setContentsMargins(4, 4, 4, 4);
    auto* left = new QVBoxLayout;
    m_list = new QListWidget; m_list->setMaximumWidth(200); left->addWidget(m_list, 1);
    auto* add = new QPushButton("+ Spatial effect"); auto* del = new QPushButton("Delete"); left->addWidget(add); left->addWidget(del);
    h->addLayout(left);
    m_form = new QWidget; auto* f = new QFormLayout(m_form); f->setLabelAlignment(Qt::AlignRight);
    m_enabled = new QCheckBox("Enabled (output to fixtures)");
    m_order = new QComboBox; m_order->addItems(spatialOrderNames());
    m_wave = new QComboBox; m_wave->addItems({"Sine", "Square", "Sawtooth", "Ramp", "Triangle", "Pulse", "Chase"});
    m_group = new QComboBox; m_group->setEditable(true);
    auto dbl = [](double mn, double mx, double st) { auto* d = new QDoubleSpinBox; d->setRange(mn, mx); d->setSingleStep(st); d->setDecimals(3); return d; };
    auto in = [](int mn, int mx) { auto* s = new QSpinBox; s->setRange(mn, mx); return s; };
    m_speed = dbl(0, 20, 0.05); m_spread = dbl(0, 8, 0.1); m_width = dbl(0.01, 1, 0.05); m_offset = dbl(0, 1, 0.05); m_delay = dbl(0, 5, 0.05);
    m_low = dbl(0, 1, 0.05); m_high = dbl(0, 1, 0.05); m_level = dbl(0, 1, 0.05);
    m_wings = in(1, 16); m_blocks = in(1, 64); m_groups = in(1, 64); m_seed = in(0, 1000000);
    m_reverse = new QCheckBox("Reverse direction"); m_bounce = new QCheckBox("Bounce"); m_sweep = new QCheckBox("Hue sweep across fixtures");
    m_ca = new ColorButton("High colour"); m_cb = new ColorButton("Low colour");
    f->addRow(m_enabled); f->addRow("Fixture group", m_group); f->addRow("Order", m_order); f->addRow("Waveform", m_wave);
    f->addRow("Speed (cycles/s)", m_speed); f->addRow("Phase spread", m_spread); f->addRow("Width", m_width); f->addRow("Offset", m_offset); f->addRow("Delay per step (s)", m_delay);
    f->addRow("Wings", m_wings); f->addRow("Blocks", m_blocks); f->addRow("Groups", m_groups); f->addRow("Random seed", m_seed);
    f->addRow("Intensity low", m_low); f->addRow("Intensity high", m_high); f->addRow("Output level", m_level);
    auto* cols = new QHBoxLayout; cols->addWidget(m_ca); cols->addWidget(m_cb); f->addRow("Colours", cols);
    f->addRow(m_reverse); f->addRow(m_bounce); f->addRow(m_sweep);
    auto* note = new QLabel("Fixtures with colour but no dimmer use the virtual dimmer; intensity-only fixtures receive the level; multi-cell fixtures are evaluated per cell. Preview in the 3D Stage view.");
    note->setWordWrap(true); note->setStyleSheet("color:#8088b0;"); f->addRow(note);
    h->addWidget(m_form, 1);
    connect(m_list, &QListWidget::currentRowChanged, this, &SpatialPanel::load);
    connect(add, &QPushButton::clicked, this, [this] { undoable(c->engine, &c->undo, "Add spatial effect", [&] { SpatialSlot s; s.fx.name = QString("Spatial %1").arg(c->engine->spatial().size() + 1); QMutexLocker l(&c->engine->stateMutex()); c->engine->spatial() << s; }); refresh(); m_list->setCurrentRow(m_list->count() - 1); });
    connect(del, &QPushButton::clicked, this, [this] { const int r = m_list->currentRow(); if (r >= 0) undoable(c->engine, &c->undo, "Delete spatial effect", [&] { QMutexLocker l(&c->engine->stateMutex()); c->engine->spatial().removeAt(r); }); refresh(); });
    for (auto* w : m_form->findChildren<QDoubleSpinBox*>()) connect(w, &QDoubleSpinBox::valueChanged, this, &SpatialPanel::store);
    for (auto* w : m_form->findChildren<QSpinBox*>()) connect(w, &QSpinBox::valueChanged, this, &SpatialPanel::store);
    for (auto* w : m_form->findChildren<QCheckBox*>()) connect(w, &QCheckBox::toggled, this, &SpatialPanel::store);
    for (auto* w : {m_order, m_wave}) connect(w, &QComboBox::currentIndexChanged, this, &SpatialPanel::store);
    connect(m_group, &QComboBox::currentTextChanged, this, &SpatialPanel::store);
    connect(m_ca, &ColorButton::colorChanged, this, &SpatialPanel::store); connect(m_cb, &ColorButton::colorChanged, this, &SpatialPanel::store);
    connect(c->engine, &Engine::projectLoaded, this, &SpatialPanel::refresh);
    connect(c->engine, &Engine::showStateChanged, this, [this] { if (m_list->count() != c->engine->spatial().size()) refresh(); });
    refresh();
}

void SpatialPanel::refresh() {
    const int cur = m_list->currentRow();
    m_list->blockSignals(true); m_list->clear();
    for (auto& s : c->engine->spatial()) m_list->addItem((s.enabled ? "● " : "○ ") + s.fx.name);
    m_list->blockSignals(false);
    m_list->setCurrentRow(std::clamp(cur, 0, int(m_list->count()) - 1));
    load();
}

void SpatialPanel::load() {
    const int r = m_list->currentRow();
    m_form->setEnabled(r >= 0 && r < c->engine->spatial().size());
    if (!m_form->isEnabled()) return;
    m_updating = true;
    const SpatialSlot& s = c->engine->spatial()[r];
    QStringList groups{""}; for (auto& f : c->engine->patch().fixtures) for (auto& g : f.groups) if (!groups.contains(g)) groups << g;
    m_group->clear(); m_group->addItems(groups); m_group->setCurrentText(s.fx.group);
    m_enabled->setChecked(s.enabled); m_order->setCurrentIndex(int(s.fx.order)); m_wave->setCurrentIndex(int(s.fx.waveform));
    m_speed->setValue(s.fx.speed); m_spread->setValue(s.fx.phaseSpread); m_width->setValue(s.fx.width); m_offset->setValue(s.fx.offset); m_delay->setValue(s.fx.delay);
    m_wings->setValue(s.fx.wings); m_blocks->setValue(s.fx.blocks); m_groups->setValue(s.fx.groups); m_seed->setValue(int(s.fx.seed));
    m_low->setValue(s.fx.low); m_high->setValue(s.fx.high); m_level->setValue(s.level);
    m_reverse->setChecked(s.fx.reverse); m_bounce->setChecked(s.fx.bounce); m_sweep->setChecked(s.fx.colorSweep);
    m_ca->setColor({s.fx.colorA.r, s.fx.colorA.g, s.fx.colorA.b}); m_cb->setColor({s.fx.colorB.r, s.fx.colorB.g, s.fx.colorB.b});
    m_updating = false;
}

void SpatialPanel::store() {
    if (m_updating) return;
    const int r = m_list->currentRow(); if (r < 0 || r >= c->engine->spatial().size()) return;
    QMutexLocker l(&c->engine->stateMutex());
    SpatialSlot& s = c->engine->spatial()[r];
    s.enabled = m_enabled->isChecked(); s.fx.group = m_group->currentText(); s.fx.order = SpatialOrder(m_order->currentIndex()); s.fx.waveform = Waveform(m_wave->currentIndex());
    s.fx.speed = m_speed->value(); s.fx.phaseSpread = m_spread->value(); s.fx.width = m_width->value(); s.fx.offset = m_offset->value(); s.fx.delay = m_delay->value();
    s.fx.wings = m_wings->value(); s.fx.blocks = m_blocks->value(); s.fx.groups = m_groups->value(); s.fx.seed = quint32(m_seed->value());
    s.fx.low = m_low->value(); s.fx.high = m_high->value(); s.level = m_level->value();
    s.fx.reverse = m_reverse->isChecked(); s.fx.bounce = m_bounce->isChecked(); s.fx.colorSweep = m_sweep->isChecked();
    const auto a = m_ca->color(), b = m_cb->color(); s.fx.colorA = {a[0], a[1], a[2]}; s.fx.colorB = {b[0], b[1], b[2]};
    l.unlock();
    if (auto* it = m_list->item(r)) it->setText((s.enabled ? "● " : "○ ") + s.fx.name);
}

} // namespace lf
