#include "EditorPanels.h"
#include "UndoCommands.h"
#include "Widgets.h"
#include "app/Engine.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QCheckBox>
#include <QMenu>
#include <QInputDialog>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QLineEdit>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPathItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>

namespace lf {

// ================================================================ Timeline
TimelineCanvas::TimelineCanvas(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) { setMinimumHeight(160); setFocusPolicy(Qt::StrongFocus); setMouseTracking(true); }

void TimelineCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(17, 18, 26));
    Timeline& tl = c->engine->timeline();
    const int rowH = 34, top = 24;
    // ruler + beat grid
    p.fillRect(QRect(0, 0, width(), top), QColor(24, 26, 40));
    const double beat = tl.beatDuration();
    for (double t = 0; t <= tl.duration; t += beat) {
        const double x = xAt(t); if (x < 140 || x > width()) continue;
        const bool bar = std::fmod(std::round(t / beat), tl.beatsPerBar) == 0;
        p.setPen(bar ? QColor(70, 74, 110) : QColor(38, 40, 60)); p.drawLine(QPointF(x, top), QPointF(x, height()));
    }
    p.setPen(QColor(150, 155, 190));
    const double step = m_pps > 40 ? 1 : m_pps > 10 ? 5 : 10;
    for (double t = 0; t <= tl.duration; t += step) { const double x = xAt(t); if (x >= 140 && x <= width()) p.drawText(QPointF(x + 2, 15), tc::Timecode::fromSeconds(t, tc::Rate::Fps25).toString().mid(3, 5)); }
    if (tl.loop && tl.loopOut > tl.loopIn) p.fillRect(QRectF(xAt(tl.loopIn), 0, (tl.loopOut - tl.loopIn) * m_pps, top), QColor(80, 60, 160, 120));
    for (auto& m : tl.markers) { const double x = xAt(m.time); p.setPen(QColor(255, 190, 60)); p.drawLine(QPointF(x, 0), QPointF(x, height())); p.drawText(QPointF(x + 3, top - 2), m.name); }
    // tracks
    for (int i = 0; i < tl.tracks.size(); ++i) {
        const Track& tr = tl.tracks[i];
        const QRectF row(0, top + i * rowH, width(), rowH - 2);
        p.fillRect(row, i == m_selTrack ? QColor(30, 34, 58) : QColor(22, 23, 34));
        p.fillRect(QRectF(0, row.top(), 138, row.height()), QColor(28, 30, 46));
        p.setPen(tr.enabled ? QColor(210, 214, 240) : QColor(100, 100, 120));
        p.drawText(QRectF(6, row.top(), 130, row.height()), Qt::AlignVCenter, tr.name + "  [" + trackTypeName(tr.type) + "]");
        p.save(); p.setClipRect(QRectF(140, row.top(), width() - 140, row.height()));
        if (!tr.keys.isEmpty()) {
            const ParameterDef d = c->engine->params().def(tr.target);
            QPainterPath path; bool first = true;
            for (double x = 140; x < width(); x += 2) {
                const double t = timeAt(x); const double n = d.normalize(tr.evaluate(t));
                const QPointF pt(x, row.bottom() - 3 - n * (row.height() - 6));
                if (first) { path.moveTo(pt); first = false; } else path.lineTo(pt);
            }
            p.setPen(QPen(QColor(90, 150, 255), 1.5)); p.drawPath(path);
            for (auto& k : tr.keys) { const QPointF pt(xAt(k.time), row.bottom() - 3 - d.normalize(k.value) * (row.height() - 6)); p.setBrush(QColor(170, 110, 255)); p.setPen(Qt::white); p.drawEllipse(pt, 4, 4); }
        }
        for (auto& e : tr.events) { const double x = xAt(e.time); p.setBrush(QColor(60, 190, 140)); p.setPen(Qt::NoPen); p.drawRoundedRect(QRectF(x - 3, row.top() + 4, 6, row.height() - 8), 2, 2); p.setPen(QColor(150, 230, 200)); p.drawText(QPointF(x + 5, row.center().y() + 4), e.label.isEmpty() ? e.action : e.label); }
        p.restore();
    }
    // playhead
    const double px = xAt(tl.position());
    p.setPen(QPen(QColor(255, 60, 80), 2)); p.drawLine(QPointF(px, 0), QPointF(px, height()));
}

void TimelineCanvas::mousePressEvent(QMouseEvent* e) {
    Timeline& tl = c->engine->timeline();
    const int rowH = 34, top = 24;
    if (e->position().y() < top || e->button() == Qt::MiddleButton) { m_scrub = true; tl.seek(std::max(0.0, timeAt(e->position().x()))); update(); return; }
    m_selTrack = std::clamp(int((e->position().y() - top) / rowH), 0, std::max(0, int(tl.tracks.size()) - 1));
    m_dragKey = -1;
    if (m_selTrack < tl.tracks.size()) {
        Track& tr = tl.tracks[m_selTrack];
        for (int k = 0; k < tr.keys.size(); ++k) if (std::abs(xAt(tr.keys[k].time) - e->position().x()) < 6) m_dragKey = k;
        if (e->button() == Qt::RightButton && m_dragKey >= 0) { tr.keys.removeAt(m_dragKey); m_dragKey = -1; }
    }
    update();
}

void TimelineCanvas::mouseMoveEvent(QMouseEvent* e) {
    Timeline& tl = c->engine->timeline();
    if (m_scrub && (e->buttons() & (Qt::LeftButton | Qt::MiddleButton))) { tl.seek(std::max(0.0, timeAt(e->position().x()))); tl.applyAt(tl.position(), [this](const QString& id, double v) { c->engine->params().set(id, v, "timeline"); }); update(); return; }
    if (m_dragKey >= 0 && (e->buttons() & Qt::LeftButton) && m_selTrack < tl.tracks.size()) {
        Track& tr = tl.tracks[m_selTrack];
        const int rowH = 34, top = 24; const QRectF row(0, top + m_selTrack * rowH, width(), rowH - 2);
        double t = std::max(0.0, timeAt(e->position().x())); if (snap) t = tl.snap(t, 0.25);
        const ParameterDef d = c->engine->params().def(tr.target);
        tr.keys[m_dragKey].time = t;
        tr.keys[m_dragKey].value = d.denormalize(std::clamp((row.bottom() - 3 - e->position().y()) / (row.height() - 6), 0.0, 1.0));
        std::sort(tr.keys.begin(), tr.keys.end(), [](auto& a, auto& b) { return a.time < b.time; });
        update();
    }
}
void TimelineCanvas::mouseReleaseEvent(QMouseEvent*) { m_scrub = false; m_dragKey = -1; }

void TimelineCanvas::mouseDoubleClickEvent(QMouseEvent* e) {
    Timeline& tl = c->engine->timeline();
    if (m_selTrack >= tl.tracks.size()) return;
    Track& tr = tl.tracks[m_selTrack];
    double t = std::max(0.0, timeAt(e->position().x())); if (snap) t = tl.snap(t, 0.25);
    if (!tr.target.isEmpty()) tr.addKey(t, c->engine->params().value(tr.target), Curve::Linear);
    else { bool ok; const QString a = QInputDialog::getText(this, "Add event", "Action (scene / cue.go / osc / blackout / effect):", QLineEdit::Normal, "cue.go", &ok); if (ok) { tr.events << TimelineEvent{t, a, {}, a}; std::sort(tr.events.begin(), tr.events.end(), [](auto& x, auto& y) { return x.time < y.time; }); } }
    update();
}

void TimelineCanvas::keyPressEvent(QKeyEvent* e) {
    Timeline& tl = c->engine->timeline();
    if (e->key() == Qt::Key_K && m_selTrack < tl.tracks.size() && !tl.tracks[m_selTrack].target.isEmpty()) {
        double t = tl.position(); if (snap) t = tl.snap(t, 0.25);
        tl.tracks[m_selTrack].addKey(t, c->engine->params().value(tl.tracks[m_selTrack].target)); update();
    } else if (e->key() == Qt::Key_M) { tl.markers << Marker{tl.position(), QString("M%1").arg(tl.markers.size() + 1)}; update(); }
    else QWidget::keyPressEvent(e);
}

void TimelineCanvas::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) m_pps = std::clamp(m_pps * (e->angleDelta().y() > 0 ? 1.2 : 1 / 1.2), 2.0, 400.0);
    else m_scroll = std::max(0.0, m_scroll - e->angleDelta().y());
    update();
}

TimelinePanel::TimelinePanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout;
    auto* play = new QPushButton("▶ Play"); auto* pause = new QPushButton("⏸ Pause"); auto* stop = new QPushButton("■ Stop");
    auto* loop = new QCheckBox("Loop"); auto* snap = new QCheckBox("Snap"); snap->setChecked(true);
    auto* setIn = new QPushButton("Loop in"); auto* setOut = new QPushButton("Loop out");
    m_param = new QComboBox; m_param->setMinimumWidth(160); m_param->setToolTip("Parameter for a new automation track");
    for (auto& d : c->engine->params().all()) m_param->addItem(d.group + " / " + d.name, d.id);
    auto* addTrack = new QPushButton("+ Automation"); auto* addEvents = new QPushButton("+ Event track"); auto* delTrack = new QPushButton("Delete track");
    auto* src = new QComboBox; src->addItems({"internal", "mtc", "ltc", "sync"}); src->setToolTip("Timecode source"); src->setCurrentText(c->engine->timecodeSource());
    m_pos = new QLabel; m_pos->setStyleSheet("font-family: monospace; color:#8fb4ff;");
    for (QWidget* w : std::initializer_list<QWidget*>{play, pause, stop, loop, snap, setIn, setOut, m_param, addTrack, addEvents, delTrack, new QLabel("TC:"), src}) bar->addWidget(w);
    bar->addStretch(); bar->addWidget(m_pos);
    v->addLayout(bar);
    m_canvas = new TimelineCanvas(c); v->addWidget(m_canvas, 1);
    auto* hint = new QLabel("Double-click: add keyframe/event   K: key at playhead   M: marker   Right-click key: delete   Ctrl+wheel: zoom   Drag ruler: scrub");
    hint->setStyleSheet("color:#666c90;"); v->addWidget(hint);
    Timeline& tl = c->engine->timeline();
    connect(play, &QPushButton::clicked, this, [this] { c->engine->timeline().play(); });
    connect(pause, &QPushButton::clicked, this, [this] { c->engine->timeline().pause(); });
    connect(stop, &QPushButton::clicked, this, [this] { c->engine->timeline().stop(); });
    connect(loop, &QCheckBox::toggled, this, [this](bool b) { c->engine->timeline().loop = b; });
    connect(snap, &QCheckBox::toggled, this, [this](bool b) { m_canvas->snap = b; });
    connect(setIn, &QPushButton::clicked, this, [this] { c->engine->timeline().loopIn = c->engine->timeline().position(); });
    connect(setOut, &QPushButton::clicked, this, [this] { c->engine->timeline().loopOut = c->engine->timeline().position(); });
    connect(src, &QComboBox::currentTextChanged, c->engine, &Engine::setTimecodeSource);
    connect(addTrack, &QPushButton::clicked, this, [this] { undoable(c->engine, &c->undo, "Add track", [&] { Track t; t.target = m_param->currentData().toString(); t.name = m_param->currentText().section(" / ", 1); t.type = TrackType::Parameter; c->engine->timeline().tracks << t; }); m_canvas->update(); });
    connect(addEvents, &QPushButton::clicked, this, [this] { undoable(c->engine, &c->undo, "Add track", [&] { Track t; t.name = "Events"; t.type = TrackType::Scene; c->engine->timeline().tracks << t; }); m_canvas->update(); });
    connect(delTrack, &QPushButton::clicked, this, [this] { const int i = m_canvas->selectedTrack(); if (i < c->engine->timeline().tracks.size()) undoable(c->engine, &c->undo, "Delete track", [&] { c->engine->timeline().tracks.removeAt(i); }); m_canvas->update(); });
    loop->setChecked(tl.loop);
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, [this] {
        if (!isVisible()) return;
        const Timeline& tl = c->engine->timeline();
        m_pos->setText(QString("%1  %2 BPM  %3").arg(tc::Timecode::fromSeconds(tl.position(), tc::Rate::Fps25).toString()).arg(tl.bpm, 0, 'f', 1).arg(tl.playing() ? "PLAY" : "STOP"));
        m_canvas->update();
    }); t->start(50);
}

// ================================================================ Node editor
static QColor portColor(PortType t) { return t == PortType::Float ? QColor(120, 200, 255) : t == PortType::Vec2 ? QColor(255, 200, 90) : QColor(220, 110, 255); }

class NodeItem;
class NodeScene : public QGraphicsScene {
public:
    NodeScene(NodeEditorPanel* ed) : m_ed(ed) { setBackgroundBrush(QColor(16, 17, 24)); setSceneRect(-2000, -2000, 4000, 4000); }
    NodeEditorPanel* m_ed;
    QString dragNode, dragPort; bool dragFromOutput = false; QGraphicsPathItem* rubber = nullptr;
    std::function<void(const QString&)> onError;
    void drawBackground(QPainter* p, const QRectF& r) override {
        QGraphicsScene::drawBackground(p, r);
        p->setPen(QColor(30, 32, 46));
        for (double x = std::floor(r.left() / 40) * 40; x < r.right(); x += 40) p->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        for (double y = std::floor(r.top() / 40) * 40; y < r.bottom(); y += 40) p->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
};

class NodeItem : public QGraphicsItem {
public:
    NodeItem(NodeScene* sc, const QString& id) : m_sc(sc), m_id(id) { setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges); }
    QString m_id; NodeScene* m_sc;
    const Node* node() const { return m_sc->m_ed->graph().node(m_id); }
    const NodeTypeDef* def() const { return node() ? NodeRegistry::instance().find(node()->type) : nullptr; }
    QRectF boundingRect() const override { const auto* d = def(); const int rows = d ? std::max(d->inputs.size(), d->outputs.size()) : 1; return QRectF(0, 0, 170, 30 + rows * 20 + 6); }
    QPointF inPos(int i) const { return pos() + QPointF(0, 40 + i * 20); }
    QPointF outPos(int i) const { return pos() + QPointF(170, 40 + i * 20); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        const auto* d = def(); const Node* n = node(); if (!d || !n) return;
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = boundingRect();
        p->setBrush(QColor(30, 32, 48)); p->setPen(QPen(isSelected() ? QColor(150, 110, 255) : QColor(60, 64, 96), isSelected() ? 2 : 1)); p->drawRoundedRect(r, 6, 6);
        QLinearGradient g(0, 0, 170, 0); g.setColorAt(0, QColor(40, 90, 200)); g.setColorAt(1, QColor(110, 60, 200));
        p->setBrush(g); p->setPen(Qt::NoPen); p->drawRoundedRect(QRectF(0, 0, 170, 24), 6, 6);
        p->setPen(Qt::white); p->drawText(QRectF(8, 0, 160, 24), Qt::AlignVCenter, n->label.isEmpty() ? d->name : n->label);
        for (int i = 0; i < d->inputs.size(); ++i) {
            p->setBrush(portColor(d->inputs[i].type)); p->setPen(Qt::black); p->drawEllipse(QPointF(0, 40 + i * 20), 5, 5);
            p->setPen(QColor(200, 204, 230)); QString label = d->inputs[i].name;
            if (n->params.contains(d->inputs[i].name)) label += QString("=%1").arg(n->params[d->inputs[i].name], 0, 'g', 3);
            p->drawText(QRectF(10, 30 + i * 20, 90, 20), Qt::AlignVCenter, label);
        }
        for (int i = 0; i < d->outputs.size(); ++i) {
            p->setBrush(portColor(d->outputs[i].type)); p->setPen(Qt::black); p->drawEllipse(QPointF(170, 40 + i * 20), 5, 5);
            p->setPen(QColor(200, 204, 230)); p->drawText(QRectF(70, 30 + i * 20, 90, 20), Qt::AlignVCenter | Qt::AlignRight, d->outputs[i].name);
        }
    }
    int hitPort(QPointF local, bool& output) const {
        const auto* d = def(); if (!d) return -1;
        for (int i = 0; i < d->inputs.size(); ++i) if (QLineF(local, QPointF(0, 40 + i * 20)).length() < 9) { output = false; return i; }
        for (int i = 0; i < d->outputs.size(); ++i) if (QLineF(local, QPointF(170, 40 + i * 20)).length() < 9) { output = true; return i; }
        return -1;
    }
    QVariant itemChange(GraphicsItemChange ch, const QVariant& v) override {
        if (ch == ItemPositionHasChanged) { if (Node* n = m_sc->m_ed->graph().node(m_id)) n->pos = pos(); if (scene()) scene()->update(); }
        return QGraphicsItem::itemChange(ch, v);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        bool out; const int port = hitPort(e->pos(), out);
        if (port >= 0) {
            m_sc->dragNode = m_id; m_sc->dragFromOutput = out; m_sc->dragPort = out ? def()->outputs[port].name : def()->inputs[port].name;
            if (!out) { m_sc->m_ed->graph().disconnect(m_id, m_sc->dragPort); m_sc->update(); }
            m_sc->rubber = m_sc->addPath(QPainterPath(), QPen(QColor(255, 255, 255, 180), 2, Qt::DashLine));
            e->accept(); return;
        }
        QGraphicsItem::mousePressEvent(e);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (m_sc->rubber) { QPainterPath p(e->buttonDownScenePos(Qt::LeftButton)); p.lineTo(e->scenePos()); m_sc->rubber->setPath(p); return; }
        QGraphicsItem::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override {
        if (m_sc->rubber) {
            delete m_sc->rubber; m_sc->rubber = nullptr;
            for (QGraphicsItem* it : m_sc->items(e->scenePos())) if (auto* other = dynamic_cast<NodeItem*>(it)) {
                bool out; const int port = other->hitPort(other->mapFromScene(e->scenePos()), out);
                if (port < 0 || out == m_sc->dragFromOutput) continue;
                QString err;
                if (m_sc->dragFromOutput) err = m_sc->m_ed->graph().connect(m_sc->dragNode, m_sc->dragPort, other->m_id, other->def()->inputs[port].name);
                else err = m_sc->m_ed->graph().connect(other->m_id, other->def()->outputs[port].name, m_sc->dragNode, m_sc->dragPort);
                if (!err.isEmpty() && m_sc->onError) m_sc->onError(err);
                break;
            }
            m_sc->dragNode.clear(); m_sc->update(); return;
        }
        QGraphicsItem::mouseReleaseEvent(e);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override {
        // parameter / default editor
        Node* n = m_sc->m_ed->graph().node(m_id); const auto* d = def(); if (!n || !d) return;
        QDialog dlg; dlg.setWindowTitle("Edit node: " + d->name); auto* f = new QFormLayout(&dlg);
        auto* label = new QLineEdit(n->label); f->addRow("Label", label);
        QMap<QString, QDoubleSpinBox*> boxes;
        for (auto& pd : d->params) { auto* b = new QDoubleSpinBox; b->setRange(pd.min, pd.max); b->setDecimals(3); b->setValue(n->params.value(pd.name, pd.defaultValue)); f->addRow(pd.name, b); boxes[pd.name] = b; }
        for (auto& in : d->inputs) if (in.type != PortType::Vec2) { auto* b = new QDoubleSpinBox; b->setRange(-1000, 1000); b->setDecimals(3); b->setValue(n->params.value(in.name, in.defaultValue)); f->addRow("default " + in.name, b); boxes["in:" + in.name] = b; }
        QPlainTextEdit* code = nullptr;
        if (n->type == "glsl") { code = new QPlainTextEdit(n->code); code->setMinimumHeight(100); f->addRow("GLSL expression (vec3)", code); }
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); f->addRow(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept); QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        n->label = label->text();
        for (auto it = boxes.begin(); it != boxes.end(); ++it) n->params[it.key().startsWith("in:") ? it.key().mid(3) : it.key()] = it.value()->value();
        if (code) n->code = code->toPlainText();
        update();
    }
};

class EdgeLayer : public QGraphicsItem {
public:
    EdgeLayer(NodeScene* sc) : m_sc(sc) { setZValue(-1); }
    NodeScene* m_sc;
    QRectF boundingRect() const override { return m_sc->sceneRect(); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        p->setRenderHint(QPainter::Antialiasing);
        QHash<QString, NodeItem*> items;
        for (auto* it : m_sc->items()) if (auto* n = dynamic_cast<NodeItem*>(it)) items[n->m_id] = n;
        for (const auto& c : m_sc->m_ed->graph().connections) {
            NodeItem* a = items.value(c.fromNode); NodeItem* b = items.value(c.toNode); if (!a || !b || !a->def() || !b->def()) continue;
            int oi = 0, ii = 0; for (int i = 0; i < a->def()->outputs.size(); ++i) if (a->def()->outputs[i].name == c.fromPort) oi = i;
            for (int i = 0; i < b->def()->inputs.size(); ++i) if (b->def()->inputs[i].name == c.toPort) ii = i;
            const QPointF s = a->outPos(oi), e = b->inPos(ii);
            QPainterPath path(s); const double dx = std::max(40.0, std::abs(e.x() - s.x()) * 0.5);
            path.cubicTo(s + QPointF(dx, 0), e - QPointF(dx, 0), e);
            p->setPen(QPen(portColor(a->def()->outputs[oi].type), 2)); p->drawPath(path);
        }
    }
};

NodeEditorPanel::NodeEditorPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    m_graph = NodeGraph::defaultGraph();
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout;
    m_name = new QLineEdit(m_graph.name); m_name->setMaximumWidth(180);
    auto* compile = new QPushButton("Compile → Preview"); auto* apply = new QPushButton("Apply to Program"); auto* save = new QPushButton("Save as effect");
    auto* load = new QPushButton("Open..."); auto* exp = new QPushButton("Export..."); auto* group = new QPushButton("Group → Subgraph"); auto* clear = new QPushButton("New");
    m_live = new QCheckBox("Live preview"); m_live->setChecked(true);
    for (QWidget* w : std::initializer_list<QWidget*>{new QLabel("Name"), m_name, compile, apply, save, load, exp, group, clear, m_live}) bar->addWidget(w);
    bar->addStretch();
    v->addLayout(bar);
    auto* split = new QSplitter;
    m_palette = new QTreeWidget; m_palette->setHeaderLabel("Nodes (double-click to add)"); m_palette->setMaximumWidth(230);
    for (const auto& cat : NodeRegistry::instance().categories()) {
        auto* ci = new QTreeWidgetItem(m_palette, {cat});
        for (const auto& t : NodeRegistry::instance().all()) if (t.category == cat && t.id != "subin") { auto* it = new QTreeWidgetItem(ci, {t.name}); it->setData(0, Qt::UserRole, t.id); it->setToolTip(0, t.description); }
    }
    m_palette->expandAll();
    m_scene = new NodeScene(this);
    m_scene->onError = [this](const QString& e) { m_log->appendPlainText("⚠ " + e); };
    m_view = new QGraphicsView(m_scene); m_view->setRenderHint(QPainter::Antialiasing); m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_log = new QPlainTextEdit; m_log->setReadOnly(true); m_log->setMaximumHeight(110);
    auto* right = new QWidget; auto* rv = new QVBoxLayout(right); rv->setContentsMargins(0, 0, 0, 0); rv->addWidget(m_view, 1); rv->addWidget(m_log);
    split->addWidget(m_palette); split->addWidget(right); split->setStretchFactor(1, 1);
    v->addWidget(split, 1);
    connect(m_palette, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it) {
        const QString type = it->data(0, Qt::UserRole).toString(); if (type.isEmpty()) return;
        const QPointF centre = m_view->mapToScene(m_view->viewport()->rect().center());
        m_graph.addNode(type, centre); rebuildScene();
    });
    connect(compile, &QPushButton::clicked, this, [this] { compileAndApply(false); });
    connect(apply, &QPushButton::clicked, this, [this] { compileAndApply(true); });
    connect(save, &QPushButton::clicked, this, [this] {
        m_graph.name = m_name->text();
        const QString id = "node_" + m_name->text().toLower().replace(QRegularExpression("[^a-z0-9]+"), "_");
        QString err; EffectDef def;
        if (c->engine->compileNodeEffect(m_graph, id, &err, &def)) m_log->appendPlainText("Saved effect '" + id + "' to the library (" + def.description + ")");
        else m_log->appendPlainText("Compile failed:\n" + err);
    });
    connect(exp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, "Export node graph", m_name->text() + ".lfeffect", "LUMAFLOW effect (*.lfeffect)");
        if (f.isEmpty()) return;
        m_graph.name = m_name->text();
        const auto r = m_graph.compile();
        EffectDef e; e.id = "node_" + m_name->text().toLower().replace(QRegularExpression("[^a-z0-9]+"), "_"); e.name = m_graph.name; e.category = "Custom"; e.glsl = r.glsl; e.nodeGraph = m_graph.toJson();
        for (int i = 0; i < 4; ++i) e.params[i].name = r.exposedParams.value(i, "-");
        EffectLibrary::saveEffectFile(f, e);
    });
    connect(load, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Open node graph", {}, "LUMAFLOW effect (*.lfeffect *.lfgraph *.json)");
        if (f.isEmpty()) return;
        QFile file(f); file.open(QIODevice::ReadOnly); const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
        m_graph = NodeGraph::fromJson(o.contains("nodeGraph") ? o["nodeGraph"].toObject() : o); m_name->setText(m_graph.name); rebuildScene();
    });
    connect(group, &QPushButton::clicked, this, [this] {
        QStringList ids; for (auto* it : m_scene->selectedItems()) if (auto* n = dynamic_cast<NodeItem*>(it)) ids << n->m_id;
        if (ids.size() < 2) { m_log->appendPlainText("Select two or more nodes to group."); return; }
        bool ok; const QString name = QInputDialog::getText(this, "Subgraph", "Name:", QLineEdit::Normal, "Group", &ok); if (!ok) return;
        if (m_graph.makeSubgraph(ids, name).isEmpty()) m_log->appendPlainText("Selection must have an output connected outside the group.");
        rebuildScene();
    });
    connect(clear, &QPushButton::clicked, this, [this] { m_graph = NodeGraph(); m_graph.addNode("uv", {0, 0}); m_graph.addNode("output", {500, 0}); rebuildScene(); });
    connect(m_view, &QGraphicsView::customContextMenuRequested, this, [this](const QPoint& p) {
        QMenu m;
        QStringList sel; for (auto* it : m_scene->selectedItems()) if (auto* n = dynamic_cast<NodeItem*>(it)) sel << n->m_id;
        m.addAction("Delete selected", [&] { for (auto& id : sel) m_graph.removeNode(id); rebuildScene(); })->setEnabled(!sel.isEmpty());
        m.addAction("Copy", [&] { m_clipboard = m_graph.copy(sel); })->setEnabled(!sel.isEmpty());
        m.addAction("Paste", [&] { m_graph.paste(m_clipboard, {40, 40}); rebuildScene(); })->setEnabled(!m_clipboard.isEmpty());
        m.exec(m_view->mapToGlobal(p));
    });
    auto* live = new QTimer(this); connect(live, &QTimer::timeout, this, [this] { if (m_live->isChecked() && isVisible() && m_graph.structureHash() != m_lastHash) compileAndApply(false); }); live->start(700);
    rebuildScene();
}

void NodeEditorPanel::rebuildScene() {
    m_scene->clear();
    m_scene->addItem(new EdgeLayer(m_scene));
    for (const auto& n : m_graph.nodes) { auto* it = new NodeItem(m_scene, n.id); m_scene->addItem(it); it->setPos(n.pos); }
}

void NodeEditorPanel::compileAndApply(bool toProgram) {
    m_lastHash = m_graph.structureHash();
    m_graph.name = m_name->text();
    const CompileResult r = m_graph.compile();
    if (!r.ok) { m_log->setPlainText("Compile errors:\n" + r.errors.join("\n")); return; }
    QString log;
    if (!c->engine->validateShader(r.glsl, &log)) { m_log->setPlainText("GPU validation failed:\n" + log); return; }
    EffectDef e; e.id = "__nodegraph_live"; e.name = "Node editor (live)"; e.category = "Custom"; e.source = "custom"; e.glsl = r.glsl;
    for (int i = 0; i < 4; ++i) { e.params[i].name = r.exposedParams.value(i, "-"); e.params[i].defaultValue = 0.5; }
    e.colorA = {0.1, 0.6, 1.0}; e.colorB = {1.0, 0.2, 0.6};
    c->engine->effects().add(e);
    c->engine->setLayerEffect(c->layer(), e.id, !toProgram);
    if (!toProgram) c->engine->preview().params.set(Composition::pid(c->layer(), "opacity"), 1.0);
    m_log->setPlainText(QString("OK: %1 nodes compiled, validated on GPU, applied to %2 layer %3%4").arg(r.nodeCount).arg(toProgram ? "PROGRAM" : "PREVIEW").arg(c->layer() + 1)
                        .arg(r.warnings.isEmpty() ? QString() : "\n" + r.warnings.join("\n")));
}

} // namespace lf
