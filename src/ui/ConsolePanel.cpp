#include "ConsolePanel.h"
#include "UndoCommands.h"
#include "app/Engine.h"
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>

namespace lf {

ConsolePanel::ConsolePanel(UiContext* ctx, QWidget* parent) : QWidget(parent), c(ctx) {
    auto* v = new QVBoxLayout(this); v->setContentsMargins(4, 4, 4, 4);
    auto* tabs = new QTabWidget;
    tabs->addTab(buildLinks(), "Console links");
    tabs->addTab(buildMappings(), "Control mapping");
    tabs->addTab(buildPolicies(), "Ownership");
    tabs->addTab(buildPersonality(), "Fixture personality");
    v->addWidget(tabs);
    connect(c->engine, &Engine::projectLoaded, this, &ConsolePanel::refreshMappings);
    connect(c->engine, &Engine::showStateChanged, this, [this] { if (m_maps && m_maps->rowCount() != c->engine->mapping().mappings().size()) refreshMappings(); });
}

QWidget* ConsolePanel::buildLinks() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w);
    auto* info = new QLabel("Each console controls LUMAFLOW like a fixture: patch the generated LUMAFLOW personality on the console at the address below and send Art-Net or sACN to this machine "
                            "(enable DMX input in Output → Network). grandMA3 and MagicQ can be connected simultaneously on independent addresses and mapping sets.");
    info->setWordWrap(true); v->addWidget(info);
    m_links = new QTableWidget(0, 7);
    m_links->setHorizontalHeaderLabels({"Console", "Enabled", "Universe", "Address", "Mode", "OSC feedback host", "OSC port"});
    m_links->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_links->verticalHeader()->hide();
    v->addWidget(m_links);
    auto refresh = [this] {
        m_updating = true;
        auto& cl = c->engine->consoles();
        m_links->setRowCount(cl.size());
        for (int i = 0; i < cl.size(); ++i) {
            m_links->setItem(i, 0, new QTableWidgetItem(cl[i].name));
            auto* en = new QCheckBox; en->setChecked(cl[i].enabled); m_links->setCellWidget(i, 1, en);
            connect(en, &QCheckBox::toggled, this, [this, i](bool b) {
                c->engine->consoles()[i].enabled = b;
                NetworkConfig nc = c->engine->network().config();
                if (b) { nc.inputUniverses.insert(c->engine->consoles()[i].universe); if (!nc.artnetIn && !nc.sacnIn) nc.artnetIn = nc.sacnIn = true; c->engine->applyNetworkConfig(nc); }
            });
            auto* u = new QSpinBox; u->setRange(1, 63999); u->setValue(cl[i].universe); m_links->setCellWidget(i, 2, u);
            connect(u, &QSpinBox::valueChanged, this, [this, i](int x) { c->engine->consoles()[i].universe = x; });
            auto* a = new QSpinBox; a->setRange(1, 512); a->setValue(cl[i].address); m_links->setCellWidget(i, 3, a);
            connect(a, &QSpinBox::valueChanged, this, [this, i](int x) { c->engine->consoles()[i].address = x; });
            auto* m = new QComboBox; for (auto pm : {PersonalityMode::Basic16, PersonalityMode::Standard32, PersonalityMode::Extended64, PersonalityMode::Advanced128}) m->addItem(personalityModeName(pm), int(pm));
            m->setCurrentIndex(m->findData(int(cl[i].mode))); m_links->setCellWidget(i, 4, m);
            connect(m, &QComboBox::currentIndexChanged, this, [this, i, m] { c->engine->consoles()[i].mode = PersonalityMode(m->currentData().toInt()); });
            m_links->setItem(i, 5, new QTableWidgetItem(cl[i].oscHost)); m_links->setItem(i, 6, new QTableWidgetItem(cl[i].oscPort ? QString::number(cl[i].oscPort) : ""));
        }
        m_updating = false;
    };
    connect(m_links, &QTableWidget::cellChanged, this, [this](int r, int col) {
        if (m_updating) return; auto& cl = c->engine->consoles(); if (r >= cl.size()) return;
        if (col == 0) cl[r].name = m_links->item(r, 0)->text(); if (col == 5) cl[r].oscHost = m_links->item(r, 5)->text(); if (col == 6) cl[r].oscPort = m_links->item(r, 6)->text().toInt();
    });
    auto* h = new QHBoxLayout; auto* add = new QPushButton("Add console"); auto* guide = new QPushButton("Open console setup guides...");
    h->addWidget(add); h->addWidget(guide); h->addStretch(); v->addLayout(h);
    connect(add, &QPushButton::clicked, this, [this, refresh] { ConsoleLink l; l.name = "generic"; l.universe = 3; c->engine->consoles() << l; refresh(); });
    connect(guide, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Export console files to...");
        if (dir.isEmpty()) return;
        QString rep; if (c->engine->exportConsoleFiles(dir, &rep)) { QMessageBox::information(this, "Console files exported", rep); QDesktopServices::openUrl(QUrl::fromLocalFile(dir)); }
        else QMessageBox::warning(this, "Export failed", rep);
    });
    v->addStretch();
    connect(c->engine, &Engine::projectLoaded, this, refresh);
    refresh();
    return w;
}

QWidget* ConsolePanel::buildMappings() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w);
    m_maps = new QTableWidget(0, 14);
    m_maps->setHorizontalHeaderLabels({"Console", "Source", "Address", "Target", "In min", "In max", "Out min", "Out max", "Invert", "Deadband", "Smooth", "Edge", "Soft takeover", "Feedback"});
    m_maps->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); m_maps->verticalHeader()->hide(); m_maps->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_maps, 1);
    auto* h = new QHBoxLayout;
    auto* target = new QComboBox; target->setEditable(true); target->setMinimumWidth(200);
    for (auto& d : c->engine->params().all()) target->addItem(d.id);
    for (const char* a : {"action:cue.go", "action:cue.back", "action:scene.recall", "action:take", "action:blackout", "action:tap", "action:timeline.play", "action:timeline.stop"}) target->addItem(a);
    auto* console = new QComboBox; console->addItems({"generic", "grandMA3", "MagicQ"});
    auto* learn = new QPushButton("MIDI / OSC / DMX Learn"); learn->setCheckable(true);
    auto* addOsc = new QPushButton("+ OSC"); auto* addMidi = new QPushButton("+ MIDI"); auto* addDmx = new QPushButton("+ DMX"); auto* del = new QPushButton("Delete");
    m_learn = new QLabel;
    for (QWidget* x : std::initializer_list<QWidget*>{console, target, learn, addOsc, addMidi, addDmx, del}) h->addWidget(x);
    h->addStretch(); v->addLayout(h); v->addWidget(m_learn);
    connect(learn, &QPushButton::toggled, this, [this, target, console, learn](bool on) {
        if (on) { c->engine->mapping().startLearn(target->currentText(), console->currentText()); m_learn->setText("Learning: move a fader / send an OSC message / change a DMX channel now..."); }
        else c->engine->mapping().cancelLearn();
        auto* t = new QTimer(this); t->setSingleShot(false);
        connect(t, &QTimer::timeout, this, [this, t, learn] { if (!c->engine->mapping().learning()) { t->stop(); t->deleteLater(); learn->setChecked(false); m_learn->setText("Learned: " + c->engine->mapping().lastLearned()); refreshMappings(); } });
        if (on) t->start(100); else t->deleteLater();
    });
    auto addMap = [this, target, console](ControlSource s, const QString& addr) {
        bool ok; const QString a = QInputDialog::getText(this, "Mapping address", s == ControlSource::Osc ? "OSC address pattern:" : s == ControlSource::Midi ? "MIDI key (cc:ch:num / note:ch:num):" : "DMX universe/channel[/fine]:", QLineEdit::Normal, addr, &ok);
        if (!ok) return;
        undoable(c->engine, &c->undo, "Add mapping", [&] { ControlMapping m; m.source = s; m.address = a; m.target = target->currentText(); m.console = console->currentText();
            if (m.target.startsWith("action:")) m.edge = TriggerEdge::Rising; c->engine->mapping().add(m); });
        refreshMappings();
    };
    connect(addOsc, &QPushButton::clicked, this, [addMap] { addMap(ControlSource::Osc, "/fader/1"); });
    connect(addMidi, &QPushButton::clicked, this, [addMap] { addMap(ControlSource::Midi, "cc:0:1"); });
    connect(addDmx, &QPushButton::clicked, this, [addMap] { addMap(ControlSource::Dmx, "1/1"); });
    connect(del, &QPushButton::clicked, this, [this] { const int r = m_maps->currentRow(); if (r < 0) return; const QString id = c->engine->mapping().mappings()[r].id; undoable(c->engine, &c->undo, "Delete mapping", [&] { c->engine->mapping().remove(id); }); refreshMappings(); });
    connect(m_maps, &QTableWidget::cellChanged, this, [this](int r, int col) {
        if (m_updating) return;
        auto maps = c->engine->mapping().mappings(); if (r >= maps.size()) return;
        ControlMapping& m = maps[r]; const QString t = m_maps->item(r, col)->text();
        switch (col) { case 0: m.console = t; break; case 2: m.address = t; break; case 3: m.target = t; break; case 4: m.inMin = t.toDouble(); break; case 5: m.inMax = t.toDouble(); break;
        case 6: m.outMin = t.toDouble(); break; case 7: m.outMax = t.toDouble(); break; case 8: m.invert = t.startsWith("y", Qt::CaseInsensitive); break; case 9: m.deadband = t.toDouble(); break;
        case 10: m.smoothing = std::clamp(t.toDouble(), 0.0, 0.99); break; case 11: m.edge = t.startsWith("r", Qt::CaseInsensitive) ? TriggerEdge::Rising : t.startsWith("f", Qt::CaseInsensitive) ? TriggerEdge::Falling : t.startsWith("b", Qt::CaseInsensitive) ? TriggerEdge::Both : TriggerEdge::None; break;
        case 12: m.softTakeover = t.startsWith("y", Qt::CaseInsensitive); break; case 13: m.feedback = t.startsWith("y", Qt::CaseInsensitive); break; default: break; }
        c->engine->mapping().setMappings(maps);
    });
    refreshMappings();
    return w;
}

void ConsolePanel::refreshMappings() {
    if (!m_maps) return;
    m_updating = true;
    const auto maps = c->engine->mapping().mappings();
    m_maps->setRowCount(maps.size());
    static const char* edges[] = {"none", "rising", "falling", "both"};
    for (int i = 0; i < maps.size(); ++i) {
        const auto& m = maps[i];
        const QStringList cols{m.console, controlSourceName(m.source), m.address, m.target, QString::number(m.inMin), QString::number(m.inMax), QString::number(m.outMin), QString::number(m.outMax),
                               m.invert ? "yes" : "no", QString::number(m.deadband), QString::number(m.smoothing), edges[int(m.edge)], m.softTakeover ? "yes" : "no", m.feedback ? "yes" : "no"};
        for (int k = 0; k < cols.size(); ++k) { auto* it = new QTableWidgetItem(cols[k]); if (k == 1) it->setFlags(it->flags() & ~Qt::ItemIsEditable); m_maps->setItem(i, k, it); }
    }
    m_updating = false;
}

QWidget* ConsolePanel::buildPolicies() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w);
    auto* info = new QLabel("Control ownership decides which source may change a parameter when several are active. Console priority: a console that touched a parameter holds it for 2 s against touch/remote changes. "
                            "Touch priority: the reverse. Locked: only the selected source may change it. Default for parameters not listed:");
    info->setWordWrap(true); v->addWidget(info);
    auto* def = new QComboBox; def->addItems({"last-change-wins", "console-priority", "touch-priority", "locked"}); def->setCurrentIndex(int(c->engine->mapping().defaultPolicy()));
    v->addWidget(def);
    connect(def, &QComboBox::currentIndexChanged, this, [this](int i) { c->engine->mapping().setDefaultPolicy(OwnershipPolicy(i)); });
    m_policies = new QTableWidget(0, 3); m_policies->setHorizontalHeaderLabels({"Parameter", "Policy", "Locked owner"}); m_policies->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    v->addWidget(m_policies, 1);
    const QStringList ids{"master.intensity", "show.scene", "layer1.effect", "layer1.opacity", "layer1.speed", "layer2.opacity", "audio.reactivity"};
    m_policies->setRowCount(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
        m_policies->setItem(i, 0, new QTableWidgetItem(ids[i]));
        auto* pol = new QComboBox; pol->addItems({"last-change-wins", "console-priority", "touch-priority", "locked"}); pol->setCurrentIndex(int(c->engine->mapping().policy(ids[i])));
        auto* own = new QComboBox; own->addItems({"dmx", "osc", "midi", "touch", "remote", "timeline", "sync"}); own->setCurrentIndex(3);
        m_policies->setCellWidget(i, 1, pol); m_policies->setCellWidget(i, 2, own);
        auto apply = [this, id = ids[i], pol, own] { c->engine->mapping().setPolicy(id, OwnershipPolicy(pol->currentIndex()), controlSourceFromName(own->currentText())); };
        connect(pol, &QComboBox::currentIndexChanged, this, apply); connect(own, &QComboBox::currentIndexChanged, this, apply);
    }
    return w;
}

QWidget* ConsolePanel::buildPersonality() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w);
    auto* h = new QHBoxLayout;
    m_mode = new QComboBox; for (auto pm : {PersonalityMode::Basic16, PersonalityMode::Standard32, PersonalityMode::Extended64, PersonalityMode::Advanced128}) m_mode->addItem(personalityModeName(pm), int(pm));
    m_mode->setCurrentIndex(1);
    auto* check = new QPushButton("Check for updates"); auto* approve = new QPushButton("Approve pending"); auto* reject = new QPushButton("Reject"); auto* rollback = new QPushButton("Rollback");
    auto* exp = new QPushButton("Export console files...");
    for (QWidget* x : std::initializer_list<QWidget*>{m_mode, check, approve, reject, rollback, exp}) h->addWidget(x);
    h->addStretch(); v->addLayout(h);
    m_profile = new QTableWidget(0, 5); m_profile->setHorizontalHeaderLabels({"Ch", "Fine", "Parameter", "Name", "GDTF attribute"});
    m_profile->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); m_profile->verticalHeader()->hide();
    v->addWidget(m_profile, 2);
    m_diff = new QPlainTextEdit; m_diff->setReadOnly(true); m_diff->setMaximumHeight(140); v->addWidget(m_diff);
    auto show = [this] {
        const PersonalityMode m = PersonalityMode(m_mode->currentData().toInt());
        ProfileManager& pm = c->engine->profiles();
        const PersonalityProfile p = pm.hasPending(m) ? pm.pending(m) : pm.hasActive(m) ? pm.active(m) : c->engine->generatePersonality(m);
        m_profile->setRowCount(p.channels.size());
        for (int i = 0; i < p.channels.size(); ++i) {
            const auto& ch = p.channels[i];
            const QStringList cols{QString::number(ch.offset), ch.fineOffset ? QString::number(ch.fineOffset) : "", ch.paramId, ch.name, ch.gdtfAttribute};
            for (int k = 0; k < 5; ++k) m_profile->setItem(i, k, new QTableWidgetItem(cols[k]));
        }
        QString state = pm.hasActive(m) ? QString("Active: v%1 (%2), footprint %3").arg(pm.active(m).version).arg(pm.active(m).fingerprint()).arg(pm.active(m).footprint()) : "No active profile yet (export or approve to activate).";
        if (pm.hasPending(m)) state += QString("\nPENDING v%1 - review the changes and approve. The active console profile is unchanged until you approve.").arg(pm.pending(m).version);
        state += QString("\nHistory: %1 previous version(s).").arg(pm.history(m).size());
        m_diff->setPlainText(state);
    };
    connect(m_mode, &QComboBox::currentIndexChanged, this, show);
    connect(check, &QPushButton::clicked, this, [this, show] {
        const PersonalityMode m = PersonalityMode(m_mode->currentData().toInt());
        const ProfileDiff d = c->engine->checkPersonalityUpdates(m);
        show();
        m_diff->appendPlainText(d.empty() ? "\nUp to date: no parameter changes affect this personality." : QString("\nChanges detected%1:\n%2").arg(d.breaking() ? " (BREAKING - DMX channel meaning changes)" : "", d.summary()));
    });
    connect(approve, &QPushButton::clicked, this, [this, show] {
        const PersonalityMode m = PersonalityMode(m_mode->currentData().toInt());
        QString why;
        if (!c->engine->profiles().approve(m, false, &why)) {
            if (why.contains("breaking") && QMessageBox::warning(this, "Breaking change", why + "\n\nApproving will require re-patching or updating the fixture type on every connected console. Continue?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
                c->engine->profiles().approve(m, true);
            else if (!why.contains("breaking")) m_diff->appendPlainText(why);
        }
        show();
    });
    connect(reject, &QPushButton::clicked, this, [this, show] { c->engine->profiles().reject(PersonalityMode(m_mode->currentData().toInt())); show(); });
    connect(rollback, &QPushButton::clicked, this, [this, show] { if (!c->engine->profiles().rollback(PersonalityMode(m_mode->currentData().toInt()))) m_diff->appendPlainText("Nothing to roll back."); else show(); });
    connect(exp, &QPushButton::clicked, this, [this, show] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Export console files to..."); if (dir.isEmpty()) return;
        QString rep; const bool ok = c->engine->exportConsoleFiles(dir, &rep); show(); m_diff->appendPlainText((ok ? "\nExported:\n" : "\nFAILED: ") + rep);
    });
    show();
    return w;
}

} // namespace lf
