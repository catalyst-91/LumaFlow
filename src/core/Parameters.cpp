#include "Parameters.h"
#include <algorithm>

namespace lf {

static QString typeName(ParamType t) {
    switch (t) { case ParamType::Continuous: return "continuous"; case ParamType::Index: return "index";
    case ParamType::Trigger: return "trigger"; case ParamType::Toggle: return "toggle"; }
    return "continuous";
}
static ParamType typeFromName(const QString& s) {
    if (s == "index") return ParamType::Index; if (s == "trigger") return ParamType::Trigger;
    if (s == "toggle") return ParamType::Toggle; return ParamType::Continuous;
}

QJsonObject ParameterDef::toJson() const {
    return QJsonObject{{"id", id}, {"name", name}, {"group", group}, {"type", typeName(type)},
                       {"min", minimum}, {"max", maximum}, {"default", defaultValue},
                       {"resolution", resolution}, {"indexCount", indexCount}};
}

ParameterDef ParameterDef::fromJson(const QJsonObject& o) {
    ParameterDef d;
    d.id = o["id"].toString(); d.name = o["name"].toString(); d.group = o["group"].toString();
    d.type = typeFromName(o["type"].toString()); d.minimum = o["min"].toDouble(0);
    d.maximum = o["max"].toDouble(1); d.defaultValue = o["default"].toDouble(0);
    d.resolution = o["resolution"].toInt(8); d.indexCount = o["indexCount"].toInt(0);
    return d;
}

void ParameterRegistry::define(const ParameterDef& def) {
    QWriteLocker l(&m_lock);
    auto it = m_index.find(def.id);
    if (it != m_index.end()) { m_defs[*it] = def; }
    else { m_index.insert(def.id, m_defs.size()); m_defs.push_back(def); m_values.insert(def.id, def.defaultValue); }
    ++m_structRevision;
}

bool ParameterRegistry::has(const QString& id) const { QReadLocker l(&m_lock); return m_index.contains(id); }

ParameterDef ParameterRegistry::def(const QString& id) const {
    QReadLocker l(&m_lock);
    auto it = m_index.find(id);
    return it == m_index.end() ? ParameterDef{} : m_defs[*it];
}

QVector<ParameterDef> ParameterRegistry::all() const { QReadLocker l(&m_lock); return m_defs; }

QStringList ParameterRegistry::ids() const {
    QReadLocker l(&m_lock); QStringList out; for (auto& d : m_defs) out << d.id; return out;
}

double ParameterRegistry::value(const QString& id) const { QReadLocker l(&m_lock); return m_values.value(id, 0.0); }

bool ParameterRegistry::set(const QString& id, double v, const QString& source) {
    QHash<int, Listener> listeners;
    {
        QWriteLocker l(&m_lock);
        auto it = m_index.find(id);
        if (it == m_index.end()) return false;
        const ParameterDef& d = m_defs[*it];
        v = std::clamp(v, std::min(d.minimum, d.maximum), std::max(d.minimum, d.maximum));
        if (d.type == ParamType::Index) v = std::round(v);
        double& cur = m_values[id];
        if (cur == v && d.type != ParamType::Trigger) return true;
        cur = v;
        ++m_revision;
        listeners = m_listeners;
    }
    for (auto& fn : listeners) fn(id, v, source);
    return true;
}

bool ParameterRegistry::setNormalized(const QString& id, double n, const QString& source) {
    ParameterDef d = def(id);
    if (d.id.isEmpty()) return false;
    return set(id, d.denormalize(n), source);
}

void ParameterRegistry::resetToDefaults() {
    const auto defs = all();
    for (const auto& d : defs) set(d.id, d.defaultValue, "reset");
}

int ParameterRegistry::addListener(Listener fn) { QWriteLocker l(&m_lock); int h = m_nextListener++; m_listeners.insert(h, std::move(fn)); return h; }
void ParameterRegistry::removeListener(int h) { QWriteLocker l(&m_lock); m_listeners.remove(h); }
quint64 ParameterRegistry::revision() const { QReadLocker l(&m_lock); return m_revision; }
quint64 ParameterRegistry::structureRevision() const { QReadLocker l(&m_lock); return m_structRevision; }

QJsonObject ParameterRegistry::valuesToJson() const {
    QReadLocker l(&m_lock); QJsonObject o;
    for (auto& d : m_defs) if (d.type != ParamType::Trigger) o.insert(d.id, m_values.value(d.id));
    return o;
}

void ParameterRegistry::valuesFromJson(const QJsonObject& o) {
    for (auto it = o.begin(); it != o.end(); ++it) set(it.key(), it.value().toDouble(), "load");
}

} // namespace lf
