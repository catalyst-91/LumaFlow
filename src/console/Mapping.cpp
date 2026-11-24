#include "Mapping.h"
#include <QJsonArray>
#include <QUuid>
#include <cmath>
#include "core/Math.h"

namespace lf {

static const char* kSrc[] = {"dmx", "osc", "midi", "touch", "remote", "timeline", "sync", "audio", "internal"};
QString controlSourceName(ControlSource s) { return kSrc[int(s)]; }
ControlSource controlSourceFromName(const QString& s) { for (int i = 0; i < 9; ++i) if (s == kSrc[i]) return ControlSource(i); return ControlSource::Internal; }
static const char* kPol[] = {"last-change-wins", "console-priority", "touch-priority", "locked"};
QString ownershipPolicyName(OwnershipPolicy p) { return kPol[int(p)]; }
OwnershipPolicy ownershipPolicyFromName(const QString& s) { for (int i = 0; i < 4; ++i) if (s == kPol[i]) return OwnershipPolicy(i); return OwnershipPolicy::LastChangeWins; }

QJsonObject ControlMapping::toJson() const {
    return {{"id", id}, {"console", console}, {"source", controlSourceName(source)}, {"address", address}, {"oscArg", oscArgIndex},
            {"target", target}, {"inMin", inMin}, {"inMax", inMax}, {"outMin", outMin}, {"outMax", outMax}, {"invert", invert},
            {"deadband", deadband}, {"smoothing", smoothing}, {"threshold", threshold}, {"edge", int(edge)},
            {"softTakeover", softTakeover}, {"feedback", feedback}, {"enabled", enabled}};
}
ControlMapping ControlMapping::fromJson(const QJsonObject& o) {
    ControlMapping m; m.id = o["id"].toString(); m.console = o["console"].toString("generic"); m.source = controlSourceFromName(o["source"].toString());
    m.address = o["address"].toString(); m.oscArgIndex = o["oscArg"].toInt(); m.target = o["target"].toString();
    m.inMin = o["inMin"].toDouble(0); m.inMax = o["inMax"].toDouble(1); m.outMin = o["outMin"].toDouble(0); m.outMax = o["outMax"].toDouble(1);
    m.invert = o["invert"].toBool(); m.deadband = o["deadband"].toDouble(); m.smoothing = o["smoothing"].toDouble();
    m.threshold = o["threshold"].toDouble(0.5); m.edge = TriggerEdge(o["edge"].toInt()); m.softTakeover = o["softTakeover"].toBool();
    m.feedback = o["feedback"].toBool(true); m.enabled = o["enabled"].toBool(true);
    if (m.id.isEmpty()) m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return m;
}

MappingEngine::MappingEngine(ParameterRegistry* reg) : m_reg(reg) { m_clock.start(); }

void MappingEngine::setMappings(const QVector<ControlMapping>& m) { m_mappings = m; m_state.clear(); }
void MappingEngine::add(const ControlMapping& m) { ControlMapping c = m; if (c.id.isEmpty()) c.id = QUuid::createUuid().toString(QUuid::WithoutBraces); m_mappings << c; }
void MappingEngine::remove(const QString& id) { m_mappings.erase(std::remove_if(m_mappings.begin(), m_mappings.end(), [&](auto& m) { return m.id == id; }), m_mappings.end()); m_state.remove(id); }

void MappingEngine::setPolicy(const QString& id, OwnershipPolicy p, ControlSource owner) { m_policies[id] = qMakePair(p, owner); }
OwnershipPolicy MappingEngine::policy(const QString& id) const { return m_policies.contains(id) ? m_policies[id].first : m_defaultPolicy; }

static bool isConsole(ControlSource s) { return s == ControlSource::Dmx || s == ControlSource::Osc || s == ControlSource::Midi; }
static bool isTouch(ControlSource s) { return s == ControlSource::Touch || s == ControlSource::Remote; }

bool MappingEngine::allowed(const QString& id, ControlSource src) {
    const auto pol = m_policies.contains(id) ? m_policies[id] : qMakePair(m_defaultPolicy, ControlSource::Touch);
    const qint64 now = m_clock.elapsed();
    const Owner o = m_owner.value(id);
    const bool recent = now - o.atMs < m_holdMs;
    bool ok = true;
    switch (pol.first) {
    case OwnershipPolicy::LastChangeWins: ok = true; break;
    case OwnershipPolicy::ConsolePriority: ok = isConsole(src) || !(recent && isConsole(o.source)); break;
    case OwnershipPolicy::TouchPriority: ok = isTouch(src) || !(recent && isTouch(o.source)); break;
    case OwnershipPolicy::Locked: ok = src == pol.second; break;
    }
    if (ok) m_owner[id] = Owner{src, now};
    return ok;
}

bool MappingEngine::write(const QString& id, double n, ControlSource src) {
    if (!allowed(id, src)) return false;
    return m_reg->setNormalized(id, n, sourceTag(src));
}

int MappingEngine::apply(ControlMapping& m, double raw) {
    MappingState& st = m_state[m.id];
    double n = m.inMax != m.inMin ? (raw - m.inMin) / (m.inMax - m.inMin) : raw;
    n = clamp01(n);
    if (m.invert) n = 1.0 - n;
    if (m.edge != TriggerEdge::None) {
        const bool high = n >= m.threshold;
        const bool fire = (high && !st.triggerHigh && (m.edge == TriggerEdge::Rising || m.edge == TriggerEdge::Both)) ||
                          (!high && st.triggerHigh && (m.edge == TriggerEdge::Falling || m.edge == TriggerEdge::Both));
        st.triggerHigh = high;
        if (!fire) return 0;
        if (m.target.startsWith("action:")) { if (m_action) m_action(m.target.mid(7), 1.0, m.source); return 1; }
        return write(m.target, 1.0, m.source) ? 1 : 0;
    }
    if (st.smoothed < 0) st.smoothed = n;
    st.smoothed = m.smoothing > 0 ? st.smoothed * m.smoothing + n * (1 - m.smoothing) : n;
    double v = m.outMin + (m.outMax - m.outMin) * st.smoothed;
    if (st.last >= 0 && std::abs(v - st.last) < m.deadband) return 0;
    if (m.target.startsWith("action:")) { st.last = v; if (m_action) m_action(m.target.mid(7), v, m.source); return 1; }
    if (m.softTakeover && !st.pickedUp) {
        const ParameterDef d = m_reg->def(m.target);
        const double cur = d.normalize(m_reg->value(m.target));
        // pick up when the physical control crosses or comes within 2% of the current value
        if (st.last >= 0 && ((st.last - cur) * (v - cur) <= 0 || std::abs(v - cur) < 0.02)) st.pickedUp = true;
        else if (st.last < 0 && std::abs(v - cur) < 0.02) st.pickedUp = true;
        st.last = v;
        if (!st.pickedUp) return 0;
    }
    st.last = v;
    m_lastFeedback[m.target + "|" + m.id] = v;     // remember so the echo is not sent back
    return write(m.target, v, m.source) ? 1 : 0;
}

int MappingEngine::handleOsc(const osc::Message& msg) {
    if (learning()) { learnFrom(ControlSource::Osc, msg.address); return 1; }
    int n = 0;
    for (auto& m : m_mappings) {
        if (!m.enabled || m.source != ControlSource::Osc) continue;
        if (!osc::matchPattern(m.address, msg.address)) continue;
        n += apply(m, msg.args.isEmpty() ? 1.0 : msg.numberArg(m.oscArgIndex));
    }
    return n;
}

int MappingEngine::handleMidi(const midi::Message& msg) {
    if (msg.type == midi::Type::Clock || msg.type == midi::Type::QuarterFrame) return 0;
    if (learning()) { learnFrom(ControlSource::Midi, msg.key()); return 1; }
    int n = 0;
    const QString key = msg.key();
    for (auto& m : m_mappings) {
        if (!m.enabled || m.source != ControlSource::Midi || m.address != key) continue;
        n += apply(m, msg.normalized());
    }
    return n;
}

int MappingEngine::handleDmx(int universe, const quint8* dmxSlots, int count) {
    int n = 0;
    for (auto& m : m_mappings) {
        if (!m.enabled || m.source != ControlSource::Dmx) continue;
        const QStringList p = m.address.split('/');
        if (p.size() < 2 || p[0].toInt() != universe) continue;
        const int ch = p[1].toInt(), fine = p.size() > 2 ? p[2].toInt() : 0;
        if (ch < 1 || ch > count) continue;
        double v = dmxSlots[ch - 1] / 255.0;
        if (fine >= 1 && fine <= count) v = (dmxSlots[ch - 1] * 256 + dmxSlots[fine - 1]) / 65535.0;
        // only act on change (DMX is a continuous stream)
        const QString key = m.id;
        const int raw = fine ? dmxSlots[ch - 1] * 256 + dmxSlots[fine - 1] : dmxSlots[ch - 1];
        auto it = m_lastDmx[0].find(key);
        if (it != m_lastDmx[0].end() && *it == quint8(raw & 0xFF) && m_state.contains(key) && m_state[key].last >= 0 && fine == 0) continue;
        m_lastDmx[0][key] = quint8(raw & 0xFF);
        n += apply(m, v);
    }
    if (learning()) {
        // DMX learn: choose the first channel that changed by more than 10%
        static QHash<int, QVector<quint8>> prev;
        auto& pv = prev[universe];
        if (pv.size() == count) for (int i = 0; i < count; ++i) if (std::abs(int(dmxSlots[i]) - int(pv[i])) > 25) { learnFrom(ControlSource::Dmx, QString("%1/%2").arg(universe).arg(i + 1)); break; }
        pv = QVector<quint8>(dmxSlots, dmxSlots + count);
    }
    return n;
}

void MappingEngine::startLearn(const QString& target, const QString& console) { m_learnTarget = target; m_learnConsole = console; }

void MappingEngine::learnFrom(ControlSource src, const QString& address) {
    ControlMapping m; m.source = src; m.address = address; m.target = m_learnTarget; m.console = m_learnConsole;
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (src == ControlSource::Midi && address.startsWith("note:")) { m.edge = m_learnTarget.startsWith("action:") ? TriggerEdge::Rising : TriggerEdge::None; }
    if (m_learnTarget.startsWith("action:")) m.edge = TriggerEdge::Rising;
    m_mappings << m;
    m_lastLearned = QString("%1 %2 -> %3").arg(controlSourceName(src), address, m_learnTarget);
    m_learnTarget.clear();
}

void MappingEngine::onParameterChanged(const QString& id, double value, const QString& tag) {
    if (!m_feedback) return;
    const ParameterDef d = m_reg->def(id);
    const double n = d.normalize(value);
    for (auto& m : m_mappings) {
        if (!m.feedback || !m.enabled || m.target != id || m.edge != TriggerEdge::None) continue;
        // feedback loop prevention: do not echo a value back to the mapping that produced it
        const QString key = id + "|" + m.id;
        if (tag == sourceTag(m.source) && std::abs(m_lastFeedback.value(key, -1) - n) < 1e-4) continue;
        if (std::abs(m_lastFeedback.value(key, -1) - n) < 1e-4) continue;
        m_lastFeedback[key] = n;
        // reset soft takeover: physical control must be picked up again after an external change
        if (tag != sourceTag(m.source)) m_state[m.id].pickedUp = false;
        m_feedback(m, n);
    }
}

QJsonArray MappingEngine::toJson() const {
    QJsonArray a; for (auto& m : m_mappings) a << m.toJson();
    QJsonObject pol; for (auto it = m_policies.begin(); it != m_policies.end(); ++it) pol[it.key()] = ownershipPolicyName(it->first) + "|" + controlSourceName(it->second);
    a << QJsonObject{{"__policies", pol}, {"__default", ownershipPolicyName(m_defaultPolicy)}};
    return a;
}

void MappingEngine::fromJson(const QJsonArray& a) {
    m_mappings.clear(); m_state.clear(); m_policies.clear();
    for (auto v : a) {
        const QJsonObject o = v.toObject();
        if (o.contains("__policies")) {
            const QJsonObject pol = o["__policies"].toObject();
            for (auto it = pol.begin(); it != pol.end(); ++it) { const auto s = it.value().toString().split('|'); m_policies[it.key()] = qMakePair(ownershipPolicyFromName(s.value(0)), controlSourceFromName(s.value(1))); }
            m_defaultPolicy = ownershipPolicyFromName(o["__default"].toString());
            continue;
        }
        m_mappings << ControlMapping::fromJson(o);
    }
}

} // namespace lf
