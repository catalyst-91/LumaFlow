#include "Engine.h"
#include "core/Log.h"
#include "storage/ProjectIO.h"
#include "fixtures/Gdtf.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <cmath>

namespace lf {

// ---------------------------------------------------------------- scenes / presets / transitions
double Engine::transitionProgress() const {
    if (m_transStart < 0 || m_transDur <= 0) return 1.0;
    return std::clamp((showTime() - m_transStart) / m_transDur, 0.0, 1.0);
}

int Engine::storeScene(const QString& name, int index) {
    QMutexLocker l(&m_state);
    Scene s; s.id = QUuid::createUuid().toString(QUuid::WithoutBraces); s.name = name.isEmpty() ? QString("Scene %1").arg(m_scenes.size() + 1) : name;
    s.state = m_program.toJson();
    if (index >= 0 && index < m_scenes.size()) { s.id = m_scenes[index].id; m_scenes[index] = s; }
    else { m_scenes << s; index = int(m_scenes.size()) - 1; }
    ++m_showRevision;
    l.unlock();
    updateIndexRanges();
    emit showStateChanged();
    return index;
}

bool Engine::recallScene(int index, double tsec) {
    QMutexLocker l(&m_state);
    if (index < 0 || index >= m_scenes.size()) return false;
    const double dur = tsec >= 0 ? tsec : (m_scenes[index].transition >= 0 ? m_scenes[index].transition : m_program.params.value("show.transition"));
    m_outgoing.copyFrom(m_program);
    m_clockO = m_clockP; m_playersO = m_playersP;
    const double master = m_program.params.value("master.intensity");
    QJsonObject state = m_scenes[index].state;
    m_program.fromJson(state);
    m_program.params.set("master.intensity", master, "scene-keep");   // scenes do not override the master fader
    m_program.params.set("show.scene", index, "internal-sync");
    m_transStart = showTime(); m_transDur = dur;
    m_activeScene = index;
    l.unlock();
    m_sync.publishEvent({{"type", "scene"}, {"index", index}, {"transition", dur}});
    sendStatusOsc("/lumaflow/status/scene", index);
    emit showStateChanged();
    emit message(QString("Scene %1 '%2' (%3 s)").arg(index + 1).arg(m_scenes[index].name).arg(dur));
    return true;
}

void Engine::take(double tsec) {
    QMutexLocker l(&m_state);
    const double dur = tsec >= 0 ? tsec : m_program.params.value("show.transition");
    m_outgoing.copyFrom(m_program); m_clockO = m_clockP; m_playersO = m_playersP;
    const double master = m_program.params.value("master.intensity");
    m_program.copyFrom(m_preview);
    m_program.params.set("master.intensity", master, "scene-keep");
    m_transStart = showTime(); m_transDur = dur;
    l.unlock();
    m_sync.publishEvent({{"type", "take"}, {"state", m_program.toJson()}, {"transition", dur}});
    emit showStateChanged();
}

void Engine::copyProgramToPreview() { QMutexLocker l(&m_state); m_preview.copyFrom(m_program); l.unlock(); emit showStateChanged(); }

int Engine::storePreset(int layer, const QString& name) {
    QMutexLocker l(&m_state);
    Preset p; p.id = QUuid::createUuid().toString(QUuid::WithoutBraces); p.name = name.isEmpty() ? QString("Preset %1").arg(m_presets.size() + 1) : name;
    p.layer = m_program.layerToJson(layer);
    m_presets << p; ++m_showRevision;
    l.unlock(); emit showStateChanged();
    return int(m_presets.size()) - 1;
}

bool Engine::recallPreset(int idx, int layer, bool pv) {
    QMutexLocker l(&m_state);
    if (idx < 0 || idx >= m_presets.size()) return false;
    (pv ? m_preview : m_program).layerFromJson(layer, m_presets[idx].layer);
    ++m_showRevision;
    l.unlock(); emit showStateChanged();
    return true;
}

void Engine::cueGo() {
    const Cue* c; { QMutexLocker l(&m_state); c = m_cues.go(); if (c) m_cues.markFired(showTime()); }
    if (!c) return;
    bool ok = false; const int sc = c->sceneId.toInt(&ok);
    int idx = -1;
    if (ok) idx = sc - 1; else for (int i = 0; i < m_scenes.size(); ++i) if (m_scenes[i].id == c->sceneId || m_scenes[i].name == c->sceneId) idx = i;
    if (idx >= 0) recallScene(idx, c->fadeTime);
    if (c->actions.contains("timeline.play")) m_timeline.play();
    if (c->actions.contains("osc")) { const QStringList parts = c->actions["osc"].toString().split(' '); osc::Message m; m.address = parts.value(0); for (int i = 1; i < parts.size(); ++i) m.args << parts[i].toDouble(); m_net->sendOscFeedback(m); }
    sendStatusOsc("/lumaflow/status/cue", c->number);
    emit message(QString("Cue %1 %2").arg(c->number).arg(c->name));
    emit showStateChanged();
}

void Engine::cueBack() {
    const Cue* c; { QMutexLocker l(&m_state); c = m_cues.back(); }
    if (!c) return;
    bool ok = false; const int sc = c->sceneId.toInt(&ok);
    if (ok) recallScene(sc - 1, c->fadeTime);
    emit showStateChanged();
}

bool Engine::cueGoto(double n) {
    const Cue* c; { QMutexLocker l(&m_state); c = m_cues.gotoCue(n); if (c) m_cues.markFired(showTime()); }
    if (!c) return false;
    bool ok = false; const int sc = c->sceneId.toInt(&ok);
    if (ok) recallScene(sc - 1, c->fadeTime);
    emit showStateChanged();
    return true;
}

void Engine::tapTempo() {
    const double now = showTime();
    if (!m_taps.isEmpty() && now - m_taps.last() > 2.0) m_taps.clear();
    m_taps << now;
    if (m_taps.size() > 8) m_taps.removeFirst();
    if (m_taps.size() >= 3) {
        const double bpm = 60.0 * (m_taps.size() - 1) / (m_taps.last() - m_taps.first());
        m_program.params.set("show.bpm", bpm, "tap");
        m_timeline.bpm = bpm;
        emit message(QString("Tap tempo: %1 BPM").arg(bpm, 0, 'f', 1));
    }
}

void Engine::setBlackout(bool on) {
    m_blackout = on;
    m_net->blackout(on);
    sendStatusOsc("/lumaflow/status/output", on ? "blackout" : "live");
    m_sync.publishEvent({{"type", "blackout"}, {"on", on}});
    emit showStateChanged();
}

void Engine::executeAction(const QString& a, double v, ControlSource src) {
    if (a == "cue.go") cueGo();
    else if (a == "cue.back") cueBack();
    else if (a == "cue.goto") cueGoto(v);
    else if (a == "scene.recall") recallScene(int(v) - (v >= 1 ? 1 : 0));
    else if (a == "take") take();
    else if (a == "blackout") setBlackout(v > 0.5 ? !m_blackout : m_blackout);
    else if (a == "blackout.on") setBlackout(true);
    else if (a == "blackout.off") setBlackout(false);
    else if (a == "tap") tapTempo();
    else if (a == "timeline.play") m_timeline.play();
    else if (a == "timeline.stop") m_timeline.stop();
    else if (a.startsWith("preset.recall")) recallPreset(int(v), 0);
    else logWarn("engine", "unknown action " + a + " from " + controlSourceName(src));
}

// ---------------------------------------------------------------- parameter changes
void Engine::onParam(const QString& id, double v, const QString& source) {
    // index parameters that select content
    if (id.endsWith(".effect") && id.startsWith("layer") && source != "internal-sync" && source != "load" && source != "defaults") {
        const int layer = id.mid(5, id.indexOf('.') - 5).toInt() - 1;
        const QString eid = m_effects.idAt(int(v));
        if (!eid.isEmpty() && layer >= 0 && layer < m_program.layerCount() && m_program.layers[layer].effectId != eid)
            QMetaObject::invokeMethod(this, [this, layer, eid] { setLayerEffect(layer, eid); }, Qt::QueuedConnection);
    }
    if (id == "show.scene" && source != "internal-sync" && source != "load" && source != "scene-keep") {
        const int idx = int(v);
        if (idx != m_activeScene) QMetaObject::invokeMethod(this, [this, idx] { recallScene(idx); }, Qt::QueuedConnection);
    }
    if (id == "show.control" && source != "load") {
        const int c = int(v);
        if (c != m_lastControl) {
            m_lastControl = c;
            QMetaObject::invokeMethod(this, [this, c] {
                switch (c) { case 1: cueGo(); break; case 2: cueBack(); break; case 3: setBlackout(true); break; case 4: tapTempo(); break; case 6: take(); break; default: if (m_blackout) setBlackout(false); break; }
            }, Qt::QueuedConnection);
        }
    }
    if (id == "show.bpm") m_timeline.bpm = v;
    m_mapping->onParameterChanged(id, v, source);
    if (!m_applyingSync && source != "sync") m_sync.publishParam(id, v);
    if (!source.startsWith("map:") && source != "load") sendStatusOsc("/lumaflow/status/param/" + id, v);
    emit parameterChanged(id, v, source);
}

void Engine::sendStatusOsc(const QString& path, const QVariant& v) {
    if (!m_net || m_net->config().oscFeedbackTargets.isEmpty()) return;
    osc::Message m; m.address = path; m.args << v;
    m_net->sendOscFeedback(m);
}

void Engine::sendFeedback(const ControlMapping& m, double n) {
    if (m.source == ControlSource::Osc) {
        osc::Message msg; msg.address = m.address.contains('*') ? QString("/lumaflow/param/%1").arg(m.target) : m.address;
        msg.args << float(m.outMax != m.outMin ? (n - m.outMin) / (m.outMax - m.outMin) * (m.inMax - m.inMin) + m.inMin : n);
        for (auto& c : m_consoles) if (c.enabled && c.name == m.console && !c.oscHost.isEmpty() && c.oscPort > 0) m_net->sendOsc(msg, QHostAddress(c.oscHost), quint16(c.oscPort));
        m_net->sendOscFeedback(msg);
    } else if (m.source == ControlSource::Midi && m_midi && m_midi->available()) {
        const QStringList k = m.address.split(':');
        midi::Message mm; mm.channel = k.value(1).toInt(); mm.data1 = k.value(2).toInt();
        double out = m.invert ? 1.0 - n : n;
        mm.data2 = int(std::lround(std::clamp(out, 0.0, 1.0) * 127));
        mm.type = k.value(0) == "note" ? midi::Type::NoteOn : midi::Type::ControlChange;
        m_midi->send(mm);
    }
    // DMX feedback is deliberately not sent: transmitting DMX does not update a console's programmer or playbacks.
}

// ---------------------------------------------------------------- inputs
void Engine::handleOsc(const osc::Message& m, const QString& sender) {
    const QString a = m.address;
    // Native LUMAFLOW OSC API (documented in OSC_REFERENCE.md)
    if (a.startsWith("/lumaflow/")) {
        const QString r = a.mid(10);
        const double v = m.numberArg(0, 1.0);
        if (r.startsWith("param/")) {
            QString id = r.mid(6); bool raw = id.endsWith("/raw"); if (raw) id.chop(4);
            if (raw) { if (m_mapping->write(id, m_program.params.def(id).normalize(v), ControlSource::Osc)) {} }
            else m_mapping->write(id, v, ControlSource::Osc);
            return;
        }
        if (r == "master/intensity") { m_mapping->write("master.intensity", v, ControlSource::Osc); return; }
        if (r.startsWith("layer/")) {
            const QStringList p = r.split('/');
            const int layer = p.value(1).toInt() - 1;
            if (p.value(2) == "effect") {
                if (!m.args.isEmpty() && m.args[0].typeId() == QMetaType::QString) setLayerEffect(layer, m.args[0].toString());
                else { const QString id = m_effects.idAt(int(v)); if (!id.isEmpty()) setLayerEffect(layer, id); }
            } else if (p.size() >= 3) m_mapping->write(Composition::pid(layer, p[2].toLatin1().constData()), v, ControlSource::Osc);
            return;
        }
        if (r == "scene/recall") { recallScene(int(v) - 1); return; }
        if (r == "scene/store") { storeScene(m.args.size() > 1 ? m.args[1].toString() : QString(), int(v) - 1); return; }
        if (r == "preset/recall") { recallPreset(int(v) - 1, int(m.numberArg(1, 1)) - 1); return; }
        if (r == "cue/go") { cueGo(); return; }
        if (r == "cue/back") { cueBack(); return; }
        if (r == "cue/goto") { cueGoto(v); return; }
        if (r == "take") { take(); return; }
        if (r == "blackout") { setBlackout(v > 0.5); return; }
        if (r == "tap") { tapTempo(); return; }
        if (r == "bpm") { m_program.params.set("show.bpm", v, "osc"); return; }
        if (r == "timeline/play") { m_timeline.play(); return; }
        if (r == "timeline/stop") { m_timeline.stop(); return; }
        if (r == "timeline/seek") { m_timeline.seek(v); return; }
        if (r == "timecode" && !m.args.isEmpty()) { if (auto t = tc::Timecode::parse(m.args[0].toString())) { m_tcSeconds = t->toSeconds(); } return; }
        if (r == "status/request") {
            osc::Message s; s.address = "/lumaflow/status/scene"; s.args << m_activeScene + 1;
            const QStringList hp = sender.split(':');
            m_net->sendOsc(s, QHostAddress(hp.value(0)), quint16(m.numberArg(0, hp.value(1).toInt())));
            return;
        }
    }
    m_mapping->handleOsc(m);
}

void Engine::handleDmxInput(int universe) {
    DmxFrame f;
    if (!m_net->inputFrame(universe, f)) return;
    // console personality decoding
    for (const auto& c : m_consoles) {
        if (!c.enabled || c.universe != universe) continue;
        const PersonalityProfile p = m_profiles.hasActive(c.mode) ? m_profiles.active(c.mode) : generatePersonality(c.mode);
        const int base = c.address - 1;
        if (base < 0 || base + p.footprint() > kDmxSlots) continue;
        const QVector<double> vals = PersonalityGenerator::decodeDmx(p, f.data() + base, kDmxSlots - base);
        for (int i = 0; i < p.channels.size() && i < vals.size(); ++i) {
            const auto& ch = p.channels[i];
            if (vals[i] < 0) continue;
            if (!ch.indexNames.isEmpty()) {
                const double cur = m_program.params.value(ch.paramId);
                if (int(cur) != int(vals[i]) && m_mapping->policy(ch.paramId) != OwnershipPolicy::Locked) m_mapping->write(ch.paramId, m_program.params.def(ch.paramId).normalize(vals[i]), ControlSource::Dmx);
            } else {
                const double cur = m_program.params.def(ch.paramId).normalize(m_program.params.value(ch.paramId));
                if (std::abs(cur - vals[i]) > 0.5 / 255.0) m_mapping->write(ch.paramId, vals[i], ControlSource::Dmx);
            }
        }
    }
    m_mapping->handleDmx(universe, f.data(), kDmxSlots);
}

void Engine::pollMidi() {
    if (!m_midi || !m_midi->available()) return;
    QVector<midi::Message> msgs;
    m_midi->poll(msgs);
    for (const auto& m : msgs) {
        if (m_mtc.feed(m) && m_tcSource == "mtc") m_tcSeconds = m_mtc.current().toSeconds();
        m_mapping->handleMidi(m);
    }
}

void Engine::setTimecodeSource(const QString& s) { m_tcSource = s; logInfo("timecode", "timecode source: " + s); }

// ---------------------------------------------------------------- control tick (main thread, ~60 Hz)
void Engine::controlTick() {
    const double now = showTime();
    const double dt = m_lastTick > 0 ? now - m_lastTick : 0.016;
    m_lastTick = now;
    pollMidi();
    // timeline (internal clock or chase external timecode)
    auto writer = [this](const QString& id, double v) { m_program.params.set(id, v, "timeline"); };
    auto events = [this](const TimelineEvent& e) {
        if (e.action == "scene") recallScene(e.args.value("index").toInt(), e.args.value("transition", -1).toDouble());
        else if (e.action == "cue.go") cueGo();
        else if (e.action == "osc") { osc::Message m; m.address = e.args.value("address").toString(); m.args << e.args.value("value", 1.0); m_net->sendOscFeedback(m); }
        else if (e.action == "midi" && m_midi && m_midi->available()) { midi::Message mm; mm.type = midi::Type::ControlChange; mm.channel = e.args.value("channel").toInt(); mm.data1 = e.args.value("cc").toInt(); mm.data2 = e.args.value("value").toInt(); m_midi->send(mm); }
        else if (e.action == "dmx") { const int u = e.args.value("universe", 1).toInt(), ch = e.args.value("channel", 1).toInt(); m_mapping->handleDmx(u, nullptr, 0); Q_UNUSED(ch); }
        else if (e.action == "blackout") setBlackout(e.args.value("on", true).toBool());
        else if (e.action == "effect") setLayerEffect(e.args.value("layer").toInt(), e.args.value("effect").toString());
        else executeAction(e.action, e.args.value("value", 1.0).toDouble(), ControlSource::Timeline);
    };
    if (m_tcSource == "internal") m_timeline.advance(dt, writer, events);
    else if (m_tcSource == "sync") m_timeline.chase(m_sync.primaryTime(), writer, events);
    else m_timeline.chase(m_tcSeconds, writer, events);
    // cue auto-follow
    const Cue* next = nullptr;
    { QMutexLocker l(&m_state); next = m_cues.update(now); if (next) m_cues.markFired(now); }
    if (next) { bool ok; int sc = next->sceneId.toInt(&ok); if (ok) recallScene(sc - 1, next->fadeTime); emit showStateChanged(); }
    // transition finished -> free outgoing media players
    if (m_transStart >= 0 && transitionProgress() >= 1.0) { m_transStart = -1; QMutexLocker l(&m_state); m_playersO.clear(); }
    // autosave every 30 s when changed
    if (now - m_lastAutosave > 30.0) { m_lastAutosave = now; if (dirty()) autosaveNow(); }
    // last-known-good: 60 s of running after load without critical faults
    if (!m_lkgWritten && now - m_loadedAt > 60.0 && FaultRegistry::instance().activeCount(FaultSeverity::Critical) == 0) {
        ProjectIO::save(m_session->lastKnownGoodPath(), toProjectJson());
        m_lkgWritten = true;
    }
}

// ---------------------------------------------------------------- personality / console
PersonalityProfile Engine::generatePersonality(PersonalityMode mode, const QStringList& custom) const {
    QStringList effects; for (auto& e : m_effects.all()) effects << e.name;
    QStringList scenes; for (auto& s : m_scenes) scenes << s.name;
    if (scenes.isEmpty()) scenes << "Scene 1";
    return PersonalityGenerator::generate(m_program.params, mode, effects, scenes, custom);
}

ProfileDiff Engine::checkPersonalityUpdates(PersonalityMode mode) { return m_profiles.propose(generatePersonality(mode)); }

bool Engine::exportConsoleFiles(const QString& dir, QString* report) {
    QDir().mkpath(dir);
    QVector<PersonalityProfile> modes;
    for (auto m : {PersonalityMode::Basic16, PersonalityMode::Standard32, PersonalityMode::Extended64, PersonalityMode::Advanced128}) {
        if (!m_profiles.hasActive(m)) { m_profiles.propose(generatePersonality(m)); m_profiles.approve(m, true); }
        modes << m_profiles.active(m);
    }
    QString err;
    QStringList rep;
    const QString gdtfPath = QDir(dir).filePath("LUMAFLOW_MediaServer.gdtf");
    if (!PersonalityGenerator::exportGdtf(gdtfPath, modes, &err)) { if (report) *report = "GDTF export failed: " + err; return false; }
    rep << "GDTF (grandMA3 / MagicQ import): " + gdtfPath;
    // round-trip validation with our importer
    const auto back = gdtf::importFile(gdtfPath);
    if (!back.ok()) { if (report) *report = "exported GDTF failed re-import: " + back.error; return false; }
    for (int i = 0; i < modes.size(); ++i) if (back.type.modes.value(i).footprint() != modes[i].footprint()) { if (report) *report = "footprint mismatch after re-import"; return false; }
    rep << QString("Round-trip check: %1 modes re-imported, footprints match").arg(back.type.modes.size());
    QDir().mkpath(QDir(dir).filePath("grandma3")); QDir().mkpath(QDir(dir).filePath("magicq")); QDir().mkpath(QDir(dir).filePath("definitions"));
    for (const char* sub : {"grandma3/LUMAFLOW_MediaServer.gdtf", "magicq/LUMAFLOW_MediaServer.gdtf"}) {
        const QString dst = QDir(dir).filePath(sub);
        QFile::remove(dst);                                   // QFile::copy never overwrites
        if (!QFile::copy(gdtfPath, dst)) { if (report) *report = "cannot write " + dst; return false; }
    }
    for (const auto& p : modes) {
        const QString tag = personalityModeName(p.mode).section(' ', 0, 0);
        auto write = [&](const QString& path, const QString& text) { QFile f(path); if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(text.toUtf8()); rep << path; };
        write(QDir(dir).filePath("grandma3/LUMAFLOW_" + tag + "_setup.md"), PersonalityGenerator::grandma3Guide(p));
        write(QDir(dir).filePath("magicq/LUMAFLOW_" + tag + "_channels.csv"), PersonalityGenerator::magicqChannelSheet(p));
        write(QDir(dir).filePath("definitions/LUMAFLOW_" + tag + ".json"), QJsonDocument(p.toJson()).toJson());
        write(QDir(dir).filePath("definitions/LUMAFLOW_" + tag + "_DMX.md"), "# LUMAFLOW " + personalityModeName(p.mode) + "\n\n" + PersonalityGenerator::dmxReference(p));
    }
    if (report) *report = rep.join('\n');
    return true;
}

void Engine::applyNetworkConfig(const NetworkConfig& c) { NetworkConfig cc = c; if (m_opt.safeMode) cc.artnetOut = cc.sacnOut = false; m_net->applyConfig(cc); ++m_showRevision; }

bool Engine::startRecording(const QString& path, QString* err) {
    const QImage f = lastProgramFrame();
    const int w = f.isNull() ? m_videoOut.canvasWidth : f.width(), h = f.isNull() ? m_videoOut.canvasHeight : f.height();
    return m_recorder.start(path, w, h, m_render ? m_render->targetFps() : 30, {}, err);
}
void Engine::stopRecording() { m_recorder.finish(); }

void Engine::applyAudioSettings(const AudioSettings& a) {
    m_audioSettings = a;
    if (m_audioIn) { m_audioIn->stop(); m_audioIn.reset(); }
    m_audio.setGain(float(a.gain)); m_audio.setNoiseGate(float(a.gate)); m_audio.setSmoothing(float(a.smoothing)); m_audio.setSensitivity(float(a.sensitivity));
    if (!a.enabled) return;
    m_audioIn = AudioInput::create(a.device);
    if (!m_audioIn) { FaultRegistry::instance().report(FaultSeverity::Warning, "audio", "audio device unavailable: " + a.device, "Choose another input in Audio settings."); return; }
    m_ltc = std::make_unique<tc::LtcDecoder>(m_audioIn->sampleRate());
    m_audioIn->start([this](const float* s, int n) {
        m_audio.process(s, n);
        if (m_tcSource == "ltc" && m_ltc) { QVector<tc::Timecode> out; m_ltc->feed(s, n, out); if (!out.isEmpty()) m_tcSeconds = out.last().toSeconds(); }
    });
    logInfo("audio", "audio input: " + m_audioIn->name());
}

bool Engine::startRemote(quint16 port) {
    if (m_opt.safeMode) return false;
    auto users = m_remote.users();
    if (users.isEmpty()) {
        users["admin"] = qMakePair(RemoteServer::generateToken(), RemoteRole::Admin);
        users["operator"] = qMakePair(RemoteServer::generateToken(), RemoteRole::Operator);
        users["viewer"] = qMakePair(RemoteServer::generateToken(), RemoteRole::Viewer);
        m_remote.setUsers(users);
    }
    m_remote.setApiHandler([this](const HttpRequest& r) {
        HttpResponse resp;
        QMetaObject::invokeMethod(this, [&] { resp = handleRemote(r); }, Qt::BlockingQueuedConnection);
        return resp;
    });
    return m_remote.start(port);
}

bool Engine::startSync(SyncRole role, quint16 port) {
    SyncService::Callbacks cb;
    cb.snapshot = [this] { QJsonObject o; QMetaObject::invokeMethod(this, [&] { o = QJsonObject{{"program", m_program.toJson()}, {"scene", m_activeScene}, {"time", m_timeline.position()}}; }, Qt::BlockingQueuedConnection); return o; };
    cb.applySnapshot = [this](const QJsonObject& o) { QMetaObject::invokeMethod(this, [this, o] { QMutexLocker l(&m_state); m_applyingSync = true; m_program.fromJson(o["program"].toObject()); m_activeScene = o["scene"].toInt(); m_applyingSync = false; l.unlock(); emit showStateChanged(); }, Qt::QueuedConnection); };
    cb.applyParam = [this](const QString& id, double v) { QMetaObject::invokeMethod(this, [this, id, v] { m_applyingSync = true; m_program.params.set(id, v, "sync"); m_applyingSync = false; }, Qt::QueuedConnection); };
    cb.applyEvent = [this](const QJsonObject& e) { QMetaObject::invokeMethod(this, [this, e] {
        m_applyingSync = true;
        const QString t = e["type"].toString();
        if (t == "scene") recallScene(e["index"].toInt(), e["transition"].toDouble());
        else if (t == "blackout") setBlackout(e["on"].toBool());
        else if (t == "take") { QMutexLocker l(&m_state); m_preview.fromJson(e["state"].toObject()); l.unlock(); take(e["transition"].toDouble()); }
        m_applyingSync = false; }, Qt::QueuedConnection); };
    cb.showTime = [this] { return m_timeline.position(); };
    m_sync.setCallbacks(cb);
    return m_sync.start(role, port);
}

// ---------------------------------------------------------------- remote API
QJsonObject Engine::remoteState() const {
    QJsonArray layers;
    for (int i = 0; i < m_program.layerCount(); ++i) {
        const auto& l = m_program.layers[i];
        layers << QJsonObject{{"index", i + 1}, {"effect", l.effectId}, {"type", int(l.type)}, {"opacity", m_program.lp(i, "opacity")},
                              {"speed", m_program.lp(i, "speed")}, {"scale", m_program.lp(i, "scale")}, {"hue", m_program.lp(i, "hue")}, {"enabled", l.enabled}};
    }
    QJsonArray scenes; for (auto& s : m_scenes) scenes << s.name;
    QJsonArray cues; for (auto& c : m_cues.cues) cues << QJsonObject{{"number", c.number}, {"name", c.name}};
    return {{"layers", layers}, {"scenes", scenes}, {"activeScene", m_activeScene}, {"cues", cues}, {"activeCue", m_cues.current},
            {"master", m_program.params.value("master.intensity")}, {"blackout", m_blackout}, {"bpm", m_program.params.value("show.bpm")},
            {"fps", m_lastStats.fps}, {"faults", FaultRegistry::instance().activeCount()}};
}

HttpResponse Engine::handleRemote(const HttpRequest& r) {
    HttpResponse resp;
    const QJsonObject body = QJsonDocument::fromJson(r.body).object();
    auto ok = [&](const QJsonObject& o = {{"ok", true}}) { resp.body = QJsonDocument(o).toJson(QJsonDocument::Compact); };
    auto bad = [&](const QString& e) { resp.status = 400; resp.body = QJsonDocument(QJsonObject{{"error", e}}).toJson(QJsonDocument::Compact); };
    if (r.path == "/api/state") ok(remoteState());
    else if (r.path == "/api/effects") { QJsonArray a; for (auto& e : m_effects.all()) a << QJsonObject{{"id", e.id}, {"name", e.name}, {"category", e.category}}; ok({{"effects", a}}); }
    else if (r.path == "/api/presets") { QJsonArray a; for (auto& p : m_presets) a << p.name; ok({{"presets", a}}); }
    else if (r.path == "/api/param" && r.method == "POST") {
        const QString id = body["id"].toString();
        if (!m_program.params.has(id)) return bad("unknown parameter"), resp;
        const bool accepted = m_mapping->write(id, body["value"].toDouble(), ControlSource::Remote);
        ok({{"ok", accepted}, {"owner", controlSourceName(m_mapping->lastOwner(id))}});
    }
    else if (r.path == "/api/effect" && r.method == "POST") { const QString id = body["effect"].toString(); if (!m_effects.contains(id)) return bad("unknown effect"), resp; setLayerEffect(body["layer"].toInt(1) - 1, id); ok(); }
    else if (r.path == "/api/scene/recall" && r.method == "POST") ok({{"ok", recallScene(body["index"].toInt(1) - 1)}});
    else if (r.path == "/api/preset/recall" && r.method == "POST") ok({{"ok", recallPreset(body["index"].toInt(1) - 1, body["layer"].toInt(1) - 1)}});
    else if (r.path == "/api/cue/go" && r.method == "POST") { cueGo(); ok(); }
    else if (r.path == "/api/cue/back" && r.method == "POST") { cueBack(); ok(); }
    else if (r.path == "/api/take" && r.method == "POST") { take(); ok(); }
    else if (r.path == "/api/tap" && r.method == "POST") { tapTempo(); ok(); }
    else if (r.path == "/api/blackout" && r.method == "POST") { setBlackout(body["on"].toBool()); ok(); }
    else if (r.path == "/api/whoami") ok({{"user", r.user}, {"role", remoteRoleName(r.role)}});
    else { resp.status = 404; resp.body = R"({"error":"not found"})"; }
    return resp;
}

// ---------------------------------------------------------------- project I/O
QJsonObject Engine::toProjectJson() const {
    QMutexLocker l(&m_state);
    QJsonObject o;
    o["composition"] = m_program.toJson();
    o["preview"] = m_preview.toJson();
    QJsonArray sc; for (auto& s : m_scenes) sc << s.toJson(); o["scenes"] = sc;
    QJsonArray pr; for (auto& p : m_presets) pr << p.toJson(); o["presets"] = pr;
    o["cueLists"] = QJsonArray{m_cues.toJson()};
    o["timeline"] = m_timeline.toJson();
    o["patch"] = m_patch.toJson();
    o["pixelMap"] = m_pixelMap.toJson();
    QJsonArray sp; for (auto& s : m_spatial) { QJsonObject x = s.fx.toJson(); x["enabled"] = s.enabled; x["level"] = s.level; sp << x; } o["spatial"] = sp;
    o["mappings"] = m_mapping->toJson();
    o["profiles"] = m_profiles.toJson();
    QJsonArray cl; for (auto& c : m_consoles) cl << c.toJson(); o["consoles"] = cl;
    o["network"] = m_net->config().toJson();
    o["video"] = m_videoOut.toJson();
    o["calibration"] = m_calibration.toJson();
    o["safety"] = QJsonObject{{"maxStrobeHz", m_safety.maxStrobeHz}, {"maxBrightness", m_safety.maxBrightness}, {"limiter", m_safety.limiterEnabled}};
    o["audio"] = QJsonObject{{"device", m_audioSettings.device}, {"enabled", m_audioSettings.enabled}, {"gain", m_audioSettings.gain}, {"gate", m_audioSettings.gate}, {"smoothing", m_audioSettings.smoothing}, {"sensitivity", m_audioSettings.sensitivity}};
    o["timecodeSource"] = m_tcSource;
    QJsonArray custom; for (auto& e : m_effects.all()) if (e.source == "custom" || e.source == "ai") custom << e.toJson(); o["customEffects"] = custom;
    QJsonArray plugins; for (auto& p : m_plugins.records()) if (p.loaded) plugins << QJsonObject{{"id", p.id}, {"version", p.version}}; o["plugins"] = plugins;
    return o;
}

bool Engine::fromProjectJson(const QJsonObject& o, QStringList* warnings) {
    {
        QMutexLocker l(&m_state);
        for (auto v : o["customEffects"].toArray()) { EffectDef e = EffectDef::fromJson(v.toObject()); if (!m_effects.contains(e.id) && !m_opt.safeMode) m_effects.add(e); }
        m_scenes.clear(); for (auto v : o["scenes"].toArray()) m_scenes << Scene::fromJson(v.toObject());
        m_presets.clear(); for (auto v : o["presets"].toArray()) m_presets << Preset::fromJson(v.toObject());
        const QJsonArray cls = o["cueLists"].toArray(); m_cues = cls.isEmpty() ? CueList{} : CueList::fromJson(cls[0].toObject());
        m_timeline = Timeline::fromJson(o["timeline"].toObject());
        m_patch = Patch::fromJson(o["patch"].toObject());
        m_pixelMap = PixelMap::fromJson(o["pixelMap"].toObject());
        m_spatial.clear();
        for (auto v : o["spatial"].toArray()) { SpatialSlot s; s.fx = SpatialEffect::fromJson(v.toObject()); s.enabled = v.toObject()["enabled"].toBool(); s.level = v.toObject()["level"].toDouble(1); m_spatial << s; }
        if (m_spatial.isEmpty()) m_spatial.resize(1);
        m_mapping->fromJson(o["mappings"].toArray());
        m_profiles.fromJson(o["profiles"].toObject());
        if (o.contains("consoles")) { m_consoles.clear(); for (auto v : o["consoles"].toArray()) m_consoles << ConsoleLink::fromJson(v.toObject()); }
        if (o.contains("video")) m_videoOut = VideoOutputConfig::fromJson(o["video"].toObject());
        m_calibration = OutputCalibration::fromJson(o["calibration"].toObject());
        const QJsonObject sf = o["safety"].toObject();
        m_safety.maxStrobeHz = sf["maxStrobeHz"].toDouble(3.0); m_safety.maxBrightness = sf["maxBrightness"].toDouble(1.0); m_safety.limiterEnabled = sf["limiter"].toBool(true);
        m_tcSource = o["timecodeSource"].toString("internal");
    }
    updateIndexRanges();
    {
        QMutexLocker l(&m_state);
        m_program.fromJson(o["composition"].toObject());
        if (o.contains("preview")) m_preview.fromJson(o["preview"].toObject()); else m_preview.copyFrom(m_program);
        m_playersP.clear(); m_playersV.clear(); m_playersO.clear();
    }
    if (o.contains("network")) applyNetworkConfig(NetworkConfig::fromJson(o["network"].toObject()));
    const QJsonObject au = o["audio"].toObject();
    if (!au.isEmpty()) { AudioSettings a; a.device = au["device"].toString("test:120"); a.enabled = au["enabled"].toBool(); a.gain = au["gain"].toDouble(1); a.gate = au["gate"].toDouble(0.01); a.smoothing = au["smoothing"].toDouble(0.6); a.sensitivity = au["sensitivity"].toDouble(1.4); applyAudioSettings(a); }
    const DependencyReport dep = ProjectIO::checkDependencies(o, m_effects.ids(), [this] { QStringList l; for (auto& p : m_plugins.records()) if (p.loaded) l << p.id; return l; }(), QFileInfo(m_projectPath).absolutePath());
    for (const auto& line : dep.lines()) { if (warnings) *warnings << line; FaultRegistry::instance().report(FaultSeverity::Warning, "project", line, "Use Media Library > Relink or install the missing component."); }
    for (const auto& issue : m_patch.validate()) if (warnings) *warnings << issue.message;
    m_transStart = -1; m_activeScene = -1;
    m_loadedAt = showTime(); m_lkgWritten = false;
    emit showStateChanged();
    emit projectLoaded();
    return true;
}

bool Engine::saveProject(const QString& path, QString* err) {
    if (!ProjectIO::save(path, toProjectJson(), err, m_session->backupDir(), 20)) return false;
    m_projectPath = path;
    m_session->updateProject(path);
    markClean();
    emit message("Saved " + QFileInfo(path).fileName());
    return true;
}

bool Engine::loadProject(const QString& path, QStringList* warnings, QString* err) {
    const LoadResult r = ProjectIO::load(path);
    if (!r.ok()) { if (err) *err = r.error; return false; }
    if (warnings) *warnings << r.warnings;
    m_projectPath = path;
    fromProjectJson(r.doc, warnings);
    m_session->updateProject(path);
    markClean();
    return true;
}

bool Engine::newProject() {
    QMutexLocker l(&m_state);
    m_scenes.clear(); m_presets.clear(); m_cues = CueList{}; m_timeline = Timeline{}; m_patch = Patch{}; m_pixelMap = PixelMap{};
    m_spatial.clear(); m_spatial.resize(1); m_mapping->setMappings({}); m_projectPath.clear();
    m_program.params.resetToDefaults();
    l.unlock();
    setLayerEffect(0, "plasma");
    for (int i = 1; i < m_program.layerCount(); ++i) m_program.params.set(Composition::pid(i, "opacity"), 0);
    m_preview.copyFrom(m_program);
    updateIndexRanges();
    markClean();
    emit showStateChanged();
    return true;
}

bool Engine::dirty() const { return m_program.params.revision() + m_showRevision != m_cleanRevision; }
void Engine::markClean() { m_cleanRevision = m_program.params.revision() + m_showRevision; }
bool Engine::autosaveNow() { QString err; const bool ok = ProjectIO::save(m_session->autosavePath(), toProjectJson(), &err); if (!ok) logWarn("recovery", "autosave failed: " + err); return ok; }

EngineStatus Engine::status() const {
    EngineStatus s;
    { QMutexLocker l(&m_frameMutex); s.render = m_lastStats; }
    s.network = m_net->stats(); s.audio = m_audio.features(); s.sync = m_sync.status();
    s.transition = transitionProgress(); s.activeScene = m_activeScene; s.activeCue = m_cues.current;
    s.timecode = tc::Timecode::fromSeconds(m_tcSource == "internal" ? m_timeline.position() : m_tcSeconds, tc::Rate::Fps25).toString();
    s.blackout = m_blackout; s.faults = FaultRegistry::instance().activeCount(); s.recording = m_recorder.recording();
    return s;
}

// ---------------------------------------------------------------- demo show
void Engine::createDemoShow() {
    QMutexLocker l(&m_state);
    m_patch = Patch{};
    int fid = 101;
    // 8 RGBW pars on a front truss (virtual dimmer), 6 moving washes on a mid truss, 4 pixel tubes, 2 LED battens, 4 dimmers
    for (int i = 0; i < 8; ++i) { FixtureInstance f; f.fid = fid++; f.name = QString("Par %1").arg(i + 1); f.typeId = "Generic@RGBW Par"; f.modeName = "4ch"; f.universe = 1; f.address = 1 + i * 4; f.position = Vec3(-7 + i * 2.0, -2, 6); f.groups << "Front Pars"; m_patch.fixtures << f; }
    for (int i = 0; i < 6; ++i) { FixtureInstance f; f.fid = fid++; f.name = QString("Wash %1").arg(i + 1); f.typeId = "Generic@Moving Wash"; f.modeName = "14ch"; f.universe = 1; f.address = 41 + i * 14; f.position = Vec3(-6.25 + i * 2.5, 2, 7); f.rotation = Vec3(180, 0, 0); f.groups << "Washes"; m_patch.fixtures << f; }
    for (int i = 0; i < 4; ++i) { FixtureInstance f; f.fid = fid++; f.name = QString("Tube %1").arg(i + 1); f.typeId = "Generic@Pixel Tube 32"; f.modeName = "32px RGB"; f.universe = 2; f.address = 1 + i * 96; f.position = Vec3(-4.5 + i * 3.0, 4, 2.5); f.rotation = Vec3(0, 90, 0); f.groups << "Tubes"; m_patch.fixtures << f; }
    for (int i = 0; i < 2; ++i) { FixtureInstance f; f.fid = fid++; f.name = QString("Batten %1").arg(i + 1); f.typeId = "Generic@LED Batten 16 RGBW"; f.modeName = "16px RGBW+Dim"; f.universe = 3; f.address = 1 + i * 65; f.position = Vec3(-3 + i * 6.0, 5, 0.3); f.groups << "Battens"; m_patch.fixtures << f; }
    for (int i = 0; i < 4; ++i) { FixtureInstance f; f.fid = fid++; f.name = QString("Blinder %1").arg(i + 1); f.typeId = "Generic@Dimmer"; f.modeName = "1ch"; f.universe = 3; f.address = 201 + i; f.position = Vec3(-6 + i * 4.0, 6, 5); f.groups << "Blinders"; m_patch.fixtures << f; }
    // pixel map: tubes and battens spatially from front view + an LED matrix region
    m_pixelMap = PixelMap{};
    for (auto& e : PixelMap::makeFromPatch(m_patch, PixelMap::Projection::Front, 0.04)) {
        const FixtureInstance* f = m_patch.byFid(e.fid);
        if (f && (f->groups.contains("Tubes") || f->groups.contains("Battens") || f->groups.contains("Front Pars"))) m_pixelMap.elements << e;
    }
    m_pixelMap.elements << PixelMap::makeMatrix("LED Matrix 16x8", QRectF(0.3, 0.05, 0.4, 0.2), 16, 8, 4, 1, PixelOrder::GRB, true);
    // spatial effect on the washes
    m_spatial.clear();
    SpatialSlot s; s.fx.name = "Wash centre-out"; s.fx.order = SpatialOrder::CentreOut; s.fx.waveform = Waveform::Sine; s.fx.group = "Washes"; s.fx.colorA = {0.2, 0.4, 1}; s.fx.colorB = {0.8, 0, 1}; s.fx.speed = 0.4; s.enabled = true;
    m_spatial << s;
    SpatialSlot s2; s2.fx.name = "Blinder chase"; s2.fx.order = SpatialOrder::LeftToRight; s2.fx.waveform = Waveform::Chase; s2.fx.group = "Blinders"; s2.fx.speed = 1.0; s2.fx.width = 0.25; s2.enabled = false;
    m_spatial << s2;
    NetworkConfig nc = m_net->config(); nc.enabledOutputUniverses = {1, 2, 3, 4};
    l.unlock();
    applyNetworkConfig(nc);
    // scenes
    auto look = [&](const char* e1, const char* e2, double o2, double hue, const char* name) {
        setLayerEffect(0, e1); setLayerEffect(1, e2);
        m_program.params.set("layer1.opacity", 1); m_program.params.set("layer2.opacity", o2); m_program.params.set("layer1.hue", hue);
        m_program.params.set("layer3.opacity", 0); m_program.params.set("layer4.opacity", 0);
        storeScene(name);
    };
    m_scenes.clear();
    look("plasma", "particle_cloud", 0.6, 0.0, "Plasma Drift");
    look("particle_vortex", "energy_rings", 0.5, 0.0, "Vortex Rings");
    look("kaleidoscope", "neon_lines", 0.4, 0.3, "Kaleido Neon");
    look("fluid", "particle_trails", 0.7, 0.55, "Liquid Trails");
    look("geo_tunnel", "audio_particles", 0.8, 0.1, "Tunnel Pulse");
    look("smoke", "lightning", 0.6, 0.0, "Storm");
    m_cues = CueList{};
    for (int i = 0; i < 6; ++i) { Cue c; c.number = i + 1; c.name = m_scenes[i].name; c.sceneId = QString::number(i + 1); c.fadeTime = 2.0; m_cues.cues << c; }
    m_cues.cues[2].follow = Cue::AutoFollow;
    // timeline with automation
    m_timeline = Timeline{}; m_timeline.duration = 60; m_timeline.bpm = 128;
    Track t1; t1.name = "Layer 1 hue"; t1.type = TrackType::Parameter; t1.target = "layer1.hue"; t1.addKey(0, 0); t1.addKey(30, 0.5, Curve::EaseInOut); t1.addKey(60, 1.0);
    Track t2; t2.name = "Master"; t2.type = TrackType::Parameter; t2.target = "master.intensity"; t2.addKey(0, 0.2, Curve::Smooth); t2.addKey(4, 1.0); t2.addKey(56, 1.0, Curve::Smooth); t2.addKey(60, 0.0);
    Track t3; t3.name = "Scenes"; t3.type = TrackType::Scene;
    t3.events << TimelineEvent{8, "scene", {{"index", 1}, {"transition", 2.0}}, "Vortex"} << TimelineEvent{24, "scene", {{"index", 2}, {"transition", 3.0}}, "Kaleido"} << TimelineEvent{40, "scene", {{"index", 4}, {"transition", 2.0}}, "Tunnel"};
    Track t4; t4.name = "OSC cues"; t4.type = TrackType::Osc; t4.events << TimelineEvent{16, "osc", {{"address", "/lumaflow/demo/marker"}, {"value", 1}}, "OSC marker"};
    m_timeline.tracks << t1 << t2 << t3 << t4;
    m_timeline.markers << Marker{0, "Intro"} << Marker{24, "Drop"} << Marker{56, "Outro"};
    // example console mappings
    ControlMapping m1; m1.console = "grandMA3"; m1.source = ControlSource::Osc; m1.address = "/gma3/Page1/Fader201"; m1.target = "master.intensity"; m1.inMin = 0; m1.inMax = 100; m1.softTakeover = true; m_mapping->add(m1);
    ControlMapping m2; m2.console = "MagicQ"; m2.source = ControlSource::Osc; m2.address = "/pb/1"; m2.target = "layer1.opacity"; m2.inMin = 0; m2.inMax = 100; m_mapping->add(m2);
    ControlMapping m3; m3.console = "generic"; m3.source = ControlSource::Midi; m3.address = "cc:0:1"; m3.target = "layer1.speed"; m3.smoothing = 0.5; m_mapping->add(m3);
    ControlMapping m4; m4.console = "generic"; m4.source = ControlSource::Midi; m4.address = "note:0:36"; m4.target = "action:cue.go"; m4.edge = TriggerEdge::Rising; m_mapping->add(m4);
    recallScene(0, 0);
    ++m_showRevision;
    emit showStateChanged();
}

} // namespace lf
