#include "ShowPanels.h"
#include "Widgets.h"
#include "UndoCommands.h"
#include "app/Engine.h"
#include "storage/ProjectIO.h"
#include <QSlider>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QGroupBox>
#include <QPainter>
#include <QFileInfo>

namespace lf {

// ---------------------------------------------------------------- Library
LibraryPanel::LibraryPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* h = new QHBoxLayout;
    m_search = new QLineEdit; m_search->setPlaceholderText("Search effects...");
    m_cat = new QComboBox;
    h->addWidget(m_search, 1); h->addWidget(m_cat);
    v->addLayout(h);
    m_list = new QListWidget; m_list->setViewMode(QListView::IconMode); m_list->setIconSize(QSize(128, 72)); m_list->setGridSize(QSize(140, 100));
    m_list->setResizeMode(QListView::Adjust); m_list->setMovement(QListView::Static); m_list->setWordWrap(true); m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setToolTip("Double-click: load on selected layer\nRight-click: favourite, load to preview, export");
    v->addWidget(m_list, 1);
    auto* b = new QHBoxLayout;
    auto* imp = new QPushButton("Import..."); auto* exp = new QPushButton("Export...");
    b->addWidget(imp); b->addWidget(exp); b->addStretch();
    v->addLayout(b);
    connect(m_search, &QLineEdit::textChanged, this, &LibraryPanel::refresh);
    connect(m_cat, &QComboBox::currentTextChanged, this, &LibraryPanel::refresh);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        const QString id = it->data(Qt::UserRole).toString();
        undoable(c->engine, &c->undo, "Load effect " + id, [&] { c->engine->setLayerEffect(c->layer(), id, c->editPreview()); });
        if (!c->editPreview() && c->engine->params().value(Composition::pid(c->layer(), "opacity")) < 0.01) c->engine->params().set(Composition::pid(c->layer(), "opacity"), 1.0, "touch");
        emit c->status(QString("Layer %1: %2").arg(c->layer() + 1).arg(it->text()));
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        auto* it = m_list->itemAt(p); if (!it) return;
        const QString id = it->data(Qt::UserRole).toString();
        const EffectDef e = c->engine->effects().get(id);
        QMenu m;
        m.addAction(e.favourite ? "Remove from favourites" : "Add to favourites", [&] { c->engine->effects().setFavourite(id, !e.favourite); refresh(); });
        m.addAction("Load to preview layer", [&] { c->engine->setLayerEffect(c->layer(), id, true); });
        m.addAction("Export .lfeffect...", [&] { const QString f = QFileDialog::getSaveFileName(this, "Export effect", id + ".lfeffect", "LUMAFLOW effect (*.lfeffect)"); if (!f.isEmpty()) EffectLibrary::saveEffectFile(f, e); });
        if (e.source != "builtin") m.addAction("Delete custom effect", [&] { c->engine->effects().remove(id); refresh(); });
        m.exec(m_list->mapToGlobal(p));
    });
    connect(imp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Import effect", {}, "LUMAFLOW effect (*.lfeffect)");
        if (f.isEmpty()) return;
        EffectDef e; QString err;
        if (!EffectLibrary::loadEffectFile(f, e, &err) || !c->engine->addCustomEffect(e, &err)) QMessageBox::warning(this, "Import failed", err);
        else refresh();
    });
    connect(exp, &QPushButton::clicked, this, [this] {
        auto* it = m_list->currentItem(); if (!it) return;
        const QString id = it->data(Qt::UserRole).toString();
        const QString f = QFileDialog::getSaveFileName(this, "Export effect", id + ".lfeffect", "LUMAFLOW effect (*.lfeffect)");
        if (!f.isEmpty()) EffectLibrary::saveEffectFile(f, c->engine->effects().get(id));
    });
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &LibraryPanel::animate); t->start(120);
    connect(c->engine, &Engine::showStateChanged, this, [this] { if (m_list->count() != c->engine->effects().ids().size()) refresh(); });
    refresh();
}

void LibraryPanel::setThumbnails(const QHash<QString, QVector<QImage>>& t) { m_thumbs = t; refresh(); }

void LibraryPanel::refresh() {
    const QString cur = m_cat->currentText();
    m_cat->blockSignals(true); m_cat->clear(); m_cat->addItem("All"); m_cat->addItem("Favourites"); m_cat->addItems(c->engine->effects().categories());
    m_cat->setCurrentText(cur.isEmpty() ? "All" : cur); m_cat->blockSignals(false);
    m_list->clear();
    for (const auto& e : c->engine->effects().search(m_search->text(), m_cat->currentText())) {
        auto* it = new QListWidgetItem(e.name + (e.favourite ? " ★" : "") + (e.photosensitive ? " ⚠" : ""));
        it->setData(Qt::UserRole, e.id);
        it->setToolTip(QString("%1\n%2\nCategory: %3  Source: %4%5").arg(e.name, e.description, e.category, e.source, e.photosensitive ? "\nWARNING: photosensitive (flashing) content" : ""));
        if (m_thumbs.contains(e.id) && !m_thumbs[e.id].isEmpty()) it->setIcon(QPixmap::fromImage(m_thumbs[e.id].first()));
        else { QPixmap pm(128, 72); pm.fill(QColor(30, 32, 50)); QPainter p(&pm); p.setPen(QColor(140, 150, 200)); p.drawText(pm.rect(), Qt::AlignCenter, e.category); it->setIcon(pm); }
        m_list->addItem(it);
    }
}

void LibraryPanel::animate() {
    if (!isVisible() || m_thumbs.isEmpty()) return;
    ++m_frame;
    // animate only the hovered/selected item and visible items (cheap pixmap swaps)
    for (int i = 0; i < m_list->count(); ++i) {
        auto* it = m_list->item(i);
        const auto& fr = m_thumbs.value(it->data(Qt::UserRole).toString());
        if (fr.size() > 1 && m_list->visualItemRect(it).intersects(m_list->viewport()->rect())) it->setIcon(QPixmap::fromImage(fr[m_frame % fr.size()]));
    }
}

// ---------------------------------------------------------------- Layers
LayerPanel::LayerPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({"Layer", "On", "Solo", "Source", "Blend", "Opacity"});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->hide(); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    v->addWidget(m_table);
    auto* h = new QHBoxLayout;
    auto* edit = new QCheckBox("Edit preview"); edit->setToolTip("Edits go to the preview composition; press TAKE to send to program.");
    auto* copy = new QPushButton("Program → Preview");
    h->addWidget(edit); h->addStretch(); h->addWidget(copy);
    v->addLayout(h);
    connect(edit, &QCheckBox::toggled, c, &UiContext::setEditPreview);
    connect(copy, &QPushButton::clicked, c->engine, &Engine::copyProgramToPreview);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) { if (!m_updating && row >= 0) c->setLayer(row); });
    connect(c->engine, &Engine::showStateChanged, this, &LayerPanel::refresh);
    connect(c, &UiContext::layerChanged, this, &LayerPanel::refresh);
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, [this] {
        if (!isVisible()) return;
        for (int r = 0; r < m_table->rowCount(); ++r) if (auto* s = qobject_cast<QSlider*>(m_table->cellWidget(r, 5))) if (!s->isSliderDown()) { s->blockSignals(true); s->setValue(int(c->engine->params().value(Composition::pid(r, "opacity")) * 100)); s->blockSignals(false); }
    }); t->start(200);
    refresh();
}

void LayerPanel::refresh() {
    m_updating = true;
    Engine* e = c->engine;
    const Composition& comp = c->editPreview() ? e->preview() : e->program();
    m_table->setRowCount(comp.layerCount());
    for (int i = 0; i < comp.layerCount(); ++i) {
        const LayerSource& s = comp.layers[i];
        m_table->setItem(i, 0, new QTableWidgetItem(QString("L%1").arg(i + 1)));
        auto* on = new QCheckBox; on->setChecked(s.enabled);
        connect(on, &QCheckBox::toggled, this, [this, i](bool v) { LayerSource src = (c->editPreview() ? c->engine->preview() : c->engine->program()).layers[i]; src.enabled = v; c->engine->setLayerSource(i, src, c->editPreview()); });
        m_table->setCellWidget(i, 1, on);
        auto* solo = new QCheckBox; solo->setChecked(s.solo);
        connect(solo, &QCheckBox::toggled, this, [this, i](bool v) { LayerSource src = (c->editPreview() ? c->engine->preview() : c->engine->program()).layers[i]; src.solo = v; c->engine->setLayerSource(i, src, c->editPreview()); });
        m_table->setCellWidget(i, 2, solo);
        const QString src = s.type == SourceType::Effect ? e->effects().get(s.effectId).name : QFileInfo(s.mediaPath).fileName() + (s.type == SourceType::Media ? " (video)" : " (image)");
        m_table->setItem(i, 3, new QTableWidgetItem(src));
        auto* blend = new QComboBox; blend->addItems(blendModeNames()); blend->setCurrentIndex(int(comp.lp(i, "blend")));
        connect(blend, &QComboBox::currentIndexChanged, this, [this, i](int b) { (c->editPreview() ? c->engine->preview().params : c->engine->params()).set(Composition::pid(i, "blend"), b, "touch"); });
        m_table->setCellWidget(i, 4, blend);
        auto* op = new QSlider(Qt::Horizontal); op->setRange(0, 100); op->setValue(int(comp.lp(i, "opacity") * 100)); op->setMinimumWidth(90);
        connect(op, &QSlider::valueChanged, this, [this, i](int v) {
            if (c->editPreview()) c->engine->preview().params.set(Composition::pid(i, "opacity"), v / 100.0, "touch");
            else c->engine->mapping().write(Composition::pid(i, "opacity"), v / 100.0, ControlSource::Touch);
        });
        m_table->setCellWidget(i, 5, op);
    }
    m_table->selectRow(c->layer());
    m_updating = false;
}

// ---------------------------------------------------------------- Parameters
ParameterPanel::ParameterPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(4, 4, 4, 4);
    m_title = new QLabel; m_title->setObjectName("title");
    outer->addWidget(m_title);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    m_inner = new QWidget; m_box = new QVBoxLayout(m_inner); m_box->setContentsMargins(0, 0, 4, 0);
    scroll->setWidget(m_inner); outer->addWidget(scroll, 1);
    connect(c, &UiContext::layerChanged, this, &ParameterPanel::rebuild);
    connect(c->engine, &Engine::showStateChanged, this, &ParameterPanel::rebuild);
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &ParameterPanel::refreshValues); t->start(100);
    rebuild();
}

void ParameterPanel::rebuild() {
    Engine* e = c->engine; const int L = c->layer();
    const LayerSource src = e->program().layers.value(L);
    const EffectDef fx = e->effects().get(src.effectId);
    m_title->setText(QString("Layer %1 - %2%3").arg(L + 1).arg(src.type == SourceType::Effect ? fx.name : QFileInfo(src.mediaPath).fileName()).arg(c->editPreview() ? "  [editing PREVIEW: values shown are program]" : ""));
    // clear
    while (auto* item = m_box->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    m_sliders.clear();
    auto group = [&](const QString& title) { auto* g = new QGroupBox(title); auto* l = new QVBoxLayout(g); l->setSpacing(2); m_box->addWidget(g); return l; };
    auto* tr = group("Transform & motion");
    for (const char* k : {"speed", "scale", "rotation", "posx", "posy"}) { auto* s = new ParamSlider(e, Composition::pid(L, k), e->params().def(Composition::pid(L, k)).name, &c->undo); tr->addWidget(s); m_sliders << s; }
    auto* fxg = group(src.type == SourceType::Effect ? "Effect parameters: " + fx.name : "Media");
    if (src.type == SourceType::Effect) {
        for (int i = 0; i < 4; ++i) if (fx.params[i].name != "-") { auto* s = new ParamSlider(e, Composition::pid(L, QString("p%1").arg(i + 1).toLatin1().constData()), fx.params[i].name, &c->undo); fxg->addWidget(s); m_sliders << s; }
        if (fx.id == "text" || fx.id == "logo") {
            if (fx.id == "text") { m_text = new QLineEdit(src.text); fxg->addWidget(new QLabel("Text")); fxg->addWidget(m_text);
                connect(m_text, &QLineEdit::editingFinished, this, [this] { c->engine->setLayerText(c->layer(), m_text->text()); }); }
            else { auto* b = new QPushButton("Choose logo image..."); fxg->addWidget(b);
                connect(b, &QPushButton::clicked, this, [this] { const QString f = QFileDialog::getOpenFileName(this, "Logo", {}, "Images (*.png *.jpg *.svg *.webp)"); if (f.isEmpty()) return; LayerSource s = c->engine->program().layers[c->layer()]; s.mediaPath = f; c->engine->setLayerSource(c->layer(), s); }); }
        }
        if (fx.photosensitive) { auto* w = new QLabel("⚠ Photosensitive effect: flashing content is limited by Calibration > Safety limits."); w->setObjectName("warning"); w->setWordWrap(true); fxg->addWidget(w); }
    } else {
        auto* info = new QLabel(src.mediaPath); info->setWordWrap(true); fxg->addWidget(info);
        auto* loop = new QCheckBox("Loop"); loop->setChecked(src.mediaLoop); fxg->addWidget(loop);
        auto* rev = new QCheckBox("Reverse (clips <= 900 frames)"); rev->setChecked(src.mediaReverse); fxg->addWidget(rev);
        auto* inOut = new QHBoxLayout; auto* in = new QDoubleSpinBox; auto* out = new QDoubleSpinBox; in->setRange(0, 36000); out->setRange(0, 36000); in->setValue(src.mediaIn); out->setValue(src.mediaOut);
        in->setPrefix("In "); out->setPrefix("Out "); in->setSuffix(" s"); out->setSuffix(" s"); inOut->addWidget(in); inOut->addWidget(out); fxg->addLayout(inOut);
        auto apply = [this, loop, rev, in, out] { LayerSource s = c->engine->program().layers[c->layer()]; s.mediaLoop = loop->isChecked(); s.mediaReverse = rev->isChecked(); s.mediaIn = in->value(); s.mediaOut = out->value(); c->engine->setLayerSource(c->layer(), s); };
        connect(loop, &QCheckBox::toggled, this, apply); connect(rev, &QCheckBox::toggled, this, apply);
        connect(in, &QDoubleSpinBox::editingFinished, this, apply); connect(out, &QDoubleSpinBox::editingFinished, this, apply);
    }
    auto* col = group("Colour");
    auto* ch = new QHBoxLayout;
    m_colA = new ColorButton("Colour A"); m_colB = new ColorButton("Colour B");
    m_colA->setColor(src.colorA); m_colB->setColor(src.colorB);
    connect(m_colA, &ColorButton::colorChanged, this, [this](std::array<double, 3> a) { undoable(c->engine, &c->undo, "Layer colour", [&] { c->engine->setLayerColors(c->layer(), a, c->engine->layerColorB(c->layer())); }); });
    connect(m_colB, &ColorButton::colorChanged, this, [this](std::array<double, 3> b) { undoable(c->engine, &c->undo, "Layer colour", [&] { c->engine->setLayerColors(c->layer(), c->engine->layerColorA(c->layer()), b); }); });
    ch->addWidget(m_colA); ch->addWidget(m_colB); col->addLayout(ch);
    for (const char* k : {"hue", "hue2", "saturation", "brightness", "contrast", "audio"}) { auto* s = new ParamSlider(e, Composition::pid(L, k), e->params().def(Composition::pid(L, k)).name, &c->undo); col->addWidget(s); m_sliders << s; }
    auto* mk = group("Mask & compositing");
    auto* mh = new QHBoxLayout; m_mask = new QComboBox; m_mask->addItems(maskTypeNames()); m_mask->setCurrentIndex(int(src.mask));
    m_maskSize = new QDoubleSpinBox; m_maskSize->setRange(0, 2); m_maskSize->setSingleStep(0.05); m_maskSize->setValue(src.maskSize); m_maskSize->setPrefix("size ");
    auto* feather = new QDoubleSpinBox; feather->setRange(0, 1); feather->setSingleStep(0.02); feather->setValue(src.maskFeather); feather->setPrefix("feather ");
    mh->addWidget(m_mask, 1); mh->addWidget(m_maskSize); mh->addWidget(feather); mk->addLayout(mh);
    auto applyMask = [this, feather] { LayerSource s = c->engine->program().layers[c->layer()]; s.mask = MaskType(m_mask->currentIndex()); s.maskSize = m_maskSize->value(); s.maskFeather = feather->value(); c->engine->setLayerSource(c->layer(), s); };
    connect(m_mask, &QComboBox::currentIndexChanged, this, applyMask); connect(m_maskSize, &QDoubleSpinBox::valueChanged, this, applyMask); connect(feather, &QDoubleSpinBox::valueChanged, this, applyMask);
    auto* op = new ParamSlider(e, Composition::pid(L, "opacity"), "Opacity", &c->undo); mk->addWidget(op); m_sliders << op;
    auto* pr = group("Layer presets");
    auto* ph = new QHBoxLayout; m_presets = new QComboBox;
    for (auto& p : e->presets()) m_presets->addItem(p.name);
    auto* rec = new QPushButton("Recall"); auto* store = new QPushButton("Store..."); auto* save = new QPushButton("Export...");
    ph->addWidget(m_presets, 1); ph->addWidget(rec); ph->addWidget(store); ph->addWidget(save); pr->addLayout(ph);
    connect(rec, &QPushButton::clicked, this, [this] { const int i = m_presets->currentIndex(); undoable(c->engine, &c->undo, "Recall preset", [&] { c->engine->recallPreset(i, c->layer(), c->editPreview()); }); });
    connect(store, &QPushButton::clicked, this, [this] { bool ok; const QString n = QInputDialog::getText(this, "Store preset", "Name:", QLineEdit::Normal, QString("Preset %1").arg(c->engine->presets().size() + 1), &ok); if (ok) undoable(c->engine, &c->undo, "Store preset", [&] { c->engine->storePreset(c->layer(), n); }); });
    connect(save, &QPushButton::clicked, this, [this] {
        const int i = m_presets->currentIndex(); if (i < 0) return;
        const QString f = QFileDialog::getSaveFileName(this, "Export preset", c->engine->presets()[i].name + ".lfpreset", "LUMAFLOW preset (*.lfpreset)");
        if (!f.isEmpty()) ProjectIO::saveTyped(f, "lfpreset", c->engine->presets()[i].toJson());
    });
    m_box->addStretch();
}

void ParameterPanel::refreshValues() { if (isVisible()) for (auto* s : m_sliders) s->refresh(); }

// ---------------------------------------------------------------- Show control
ShowPanel::ShowPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    Engine* e = c->engine;
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* top = new QHBoxLayout;
    auto* go = new QPushButton("GO"); go->setObjectName("go"); go->setMinimumHeight(44); go->setShortcut(Qt::Key_Space); go->setToolTip("Next cue (Space)");
    auto* back = new QPushButton("BACK"); back->setMinimumHeight(44);
    auto* take = new QPushButton("TAKE"); take->setObjectName("take"); take->setMinimumHeight(44); take->setShortcut(Qt::Key_Return); take->setToolTip("Preview → Program (Enter)");
    m_blackout = new QPushButton("BLACKOUT"); m_blackout->setObjectName("blackout"); m_blackout->setCheckable(true); m_blackout->setMinimumHeight(44); m_blackout->setShortcut(Qt::Key_B);
    top->addWidget(go, 2); top->addWidget(back, 1); top->addWidget(take, 1); top->addWidget(m_blackout, 1);
    v->addLayout(top);
    m_master = new ParamSlider(e, "master.intensity", "Master", &c->undo); v->addWidget(m_master);
    m_transTime = new ParamSlider(e, "show.transition", "Transition (s)", &c->undo); v->addWidget(m_transTime);
    auto* tempo = new QHBoxLayout;
    auto* tap = new QPushButton("TAP"); tap->setShortcut(Qt::Key_T); m_bpm = new QLabel; m_tc = new QLabel; m_trans = new QLabel;
    m_tc->setStyleSheet("font-family: monospace; font-size: 13pt; color: #8fb4ff;");
    tempo->addWidget(tap); tempo->addWidget(m_bpm); tempo->addStretch(); tempo->addWidget(m_trans); tempo->addWidget(m_tc);
    v->addLayout(tempo);
    auto* sl = new QLabel("Scenes (double-click to recall)"); sl->setObjectName("title"); v->addWidget(sl);
    m_scenes = new QListWidget; m_scenes->setMaximumHeight(170); v->addWidget(m_scenes);
    auto* sb = new QHBoxLayout;
    auto* store = new QPushButton("Store"); auto* update = new QPushButton("Update"); auto* del = new QPushButton("Delete"); auto* exportScene = new QPushButton("Export .lfscene");
    sb->addWidget(store); sb->addWidget(update); sb->addWidget(del); sb->addWidget(exportScene); v->addLayout(sb);
    auto* cl = new QLabel("Cue list"); cl->setObjectName("title"); v->addWidget(cl);
    m_cues = new QTableWidget(0, 5); m_cues->setHorizontalHeaderLabels({"Cue", "Name", "Scene", "Fade", "Follow"});
    m_cues->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); m_cues->verticalHeader()->hide(); m_cues->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_cues, 1);
    auto* cb = new QHBoxLayout;
    auto* addCue = new QPushButton("Add cue"); auto* delCue = new QPushButton("Delete cue"); auto* gotoCue = new QPushButton("Go to selected");
    cb->addWidget(addCue); cb->addWidget(delCue); cb->addWidget(gotoCue); v->addLayout(cb);

    connect(go, &QPushButton::clicked, e, &Engine::cueGo);
    connect(back, &QPushButton::clicked, e, &Engine::cueBack);
    connect(take, &QPushButton::clicked, e, [e] { e->take(); });
    connect(m_blackout, &QPushButton::toggled, e, &Engine::setBlackout);
    connect(tap, &QPushButton::clicked, e, &Engine::tapTempo);
    connect(m_scenes, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { c->engine->recallScene(m_scenes->currentRow()); });
    connect(store, &QPushButton::clicked, this, [this] { bool ok; const QString n = QInputDialog::getText(this, "Store scene", "Name:", QLineEdit::Normal, QString("Scene %1").arg(c->engine->scenes().size() + 1), &ok); if (ok) undoable(c->engine, &c->undo, "Store scene", [&] { c->engine->storeScene(n); }); });
    connect(update, &QPushButton::clicked, this, [this] { const int r = m_scenes->currentRow(); if (r >= 0) undoable(c->engine, &c->undo, "Update scene", [&] { c->engine->storeScene(c->engine->scenes()[r].name, r); }); });
    connect(del, &QPushButton::clicked, this, [this] { const int r = m_scenes->currentRow(); if (r >= 0) undoable(c->engine, &c->undo, "Delete scene", [&] { c->engine->scenes().removeAt(r); emit c->engine->showStateChanged(); }); });
    connect(exportScene, &QPushButton::clicked, this, [this] {
        const int r = m_scenes->currentRow(); if (r < 0) return;
        const QString f = QFileDialog::getSaveFileName(this, "Export scene", c->engine->scenes()[r].name + ".lfscene", "LUMAFLOW scene (*.lfscene)");
        if (!f.isEmpty()) ProjectIO::saveTyped(f, "lfscene", c->engine->scenes()[r].toJson());
    });
    connect(addCue, &QPushButton::clicked, this, [this] {
        undoable(c->engine, &c->undo, "Add cue", [&] {
            Cue q; q.number = c->engine->cueList().nextCueNumber(); q.name = QString("Cue %1").arg(q.number);
            q.sceneId = QString::number(std::max(1, m_scenes->currentRow() + 1)); q.fadeTime = c->engine->params().value("show.transition");
            c->engine->cueList().cues << q; emit c->engine->showStateChanged();
        });
    });
    connect(delCue, &QPushButton::clicked, this, [this] { const int r = m_cues->currentRow(); if (r >= 0) undoable(c->engine, &c->undo, "Delete cue", [&] { c->engine->cueList().cues.removeAt(r); emit c->engine->showStateChanged(); }); });
    connect(gotoCue, &QPushButton::clicked, this, [this] { const int r = m_cues->currentRow(); if (r >= 0) c->engine->cueGoto(c->engine->cueList().cues[r].number); });
    connect(m_cues, &QTableWidget::cellChanged, this, [this](int r, int col) {
        auto& cues = c->engine->cueList().cues; if (r >= cues.size()) return;
        const QString t = m_cues->item(r, col)->text();
        if (col == 0) cues[r].number = t.toDouble(); else if (col == 1) cues[r].name = t; else if (col == 2) cues[r].sceneId = t; else if (col == 3) cues[r].fadeTime = t.toDouble();
        else if (col == 4) cues[r].follow = t.startsWith("A", Qt::CaseInsensitive) ? Cue::AutoFollow : t.startsWith("W", Qt::CaseInsensitive) ? Cue::Wait : Cue::Manual;
    });
    connect(e, &Engine::showStateChanged, this, &ShowPanel::refresh);
    auto* t = new QTimer(this); connect(t, &QTimer::timeout, this, &ShowPanel::refreshStatus); t->start(100);
    refresh();
}

void ShowPanel::refresh() {
    Engine* e = c->engine;
    m_scenes->clear();
    for (int i = 0; i < e->scenes().size(); ++i) { auto* it = new QListWidgetItem(QString("%1  %2").arg(i + 1).arg(e->scenes()[i].name)); if (i == e->activeScene()) { it->setForeground(QColor(170, 120, 255)); QFont f = it->font(); f.setBold(true); it->setFont(f); } m_scenes->addItem(it); }
    m_cues->blockSignals(true);
    m_cues->setRowCount(e->cueList().cues.size());
    for (int i = 0; i < e->cueList().cues.size(); ++i) {
        const Cue& q = e->cueList().cues[i];
        const QStringList cols{QString::number(q.number), q.name, q.sceneId, QString::number(q.fadeTime), q.follow == Cue::AutoFollow ? "Auto" : q.follow == Cue::Wait ? "Wait" : "Manual"};
        for (int k = 0; k < 5; ++k) { auto* it = new QTableWidgetItem(cols[k]); if (i == e->cueList().current) it->setBackground(QColor(40, 70, 150)); m_cues->setItem(i, k, it); }
    }
    m_cues->blockSignals(false);
    m_blackout->blockSignals(true); m_blackout->setChecked(e->blackout()); m_blackout->blockSignals(false);
}

void ShowPanel::refreshStatus() {
    if (!isVisible()) return;
    const EngineStatus s = c->engine->status();
    m_bpm->setText(QString("BPM %1%2").arg(c->engine->params().value("show.bpm"), 0, 'f', 1).arg(s.audio.bpm > 0 ? QString("  (audio %1)").arg(s.audio.bpm, 0, 'f', 1) : QString()));
    m_tc->setText(s.timecode);
    m_trans->setText(s.transition < 1.0 ? QString("Transition %1%").arg(int(s.transition * 100)) : QString());
    m_master->refresh(); m_transTime->refresh();
}

// ---------------------------------------------------------------- Preview / Program
PreviewPanel::PreviewPanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* h = new QHBoxLayout;
    m_preview = new ImageView("PREVIEW"); m_preview->setBorderColor(QColor(60, 160, 90));
    m_program = new ImageView("PROGRAM"); m_program->setBorderColor(QColor(200, 50, 70));
    h->addWidget(m_preview, 1); h->addWidget(m_program, 1);
    v->addLayout(h, 1);
    auto* bar = new QHBoxLayout;
    auto* safe = new QCheckBox("Safe area"); auto* grid = new QCheckBox("Pixel grid"); m_overlayPix = new QCheckBox("Pixel map overlay");
    auto* take = new QPushButton("TAKE ▶"); take->setObjectName("take");
    m_stats = new QLabel; m_stats->setStyleSheet("color:#8890b8; font-family: monospace;");
    bar->addWidget(safe); bar->addWidget(grid); bar->addWidget(m_overlayPix); bar->addStretch(); bar->addWidget(m_stats); bar->addWidget(take);
    v->addLayout(bar);
    connect(safe, &QCheckBox::toggled, this, [this](bool b) { m_program->setShowSafeArea(b); m_preview->setShowSafeArea(b); });
    connect(grid, &QCheckBox::toggled, this, [this](bool b) { m_program->setShowGrid(b); });
    connect(take, &QPushButton::clicked, c->engine, [this] { c->engine->take(); });
    connect(m_overlayPix, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) { m_program->setOverlayPainter(nullptr); return; }
        m_program->setOverlayPainter([this](QPainter& p, const QRectF& r) {
            QMutexLocker l(&c->engine->stateMutex());
            for (const auto& el : c->engine->pixelMap().elements) {
                p.setPen(QPen(QColor(0, 255, 200, 200), 1));
                const auto& pts = el.kind == PixelMapElement::Fixture ? el.cellPoints : el.points;
                for (const auto& pt : pts) p.drawEllipse(QPointF(r.left() + pt.x() * r.width(), r.top() + pt.y() * r.height()), 2.5, 2.5);
            }
        });
    });
    connect(c->engine, &Engine::frameReady, this, &PreviewPanel::updateFrames);
}

void PreviewPanel::updateFrames() {
    if (!isVisible()) return;
    m_program->setImage(c->engine->lastProgramFrame());
    m_preview->setImage(c->engine->lastPreviewFrame());
    const EngineStatus s = c->engine->status();
    m_stats->setText(QString("%1 fps  %2 ms  q%3%").arg(s.render.fps, 0, 'f', 1).arg(s.render.frameMs, 0, 'f', 1).arg(int(s.render.qualityScale * 100)));
}

} // namespace lf
