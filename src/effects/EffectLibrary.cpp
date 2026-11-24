#include "EffectLibrary.h"
#include "core/Log.h"
#include "core/Resources.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>

namespace lf {

static QString readRes(const QString& p) { QFile f(p); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString(); }

QString EffectLibrary::prelude() { initResources(); static const QString s = readRes(":/lumaflow/shaders/prelude.glsl"); return s; }
QString EffectLibrary::epilogue() { initResources(); static const QString s = readRes(":/lumaflow/shaders/main.glsl"); return s; }
QString EffectLibrary::vertexShader() {
    return "in vec2 aPos;\nout vec2 vUV;\nvoid main() { vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
}

QJsonObject EffectDef::toJson() const {
    QJsonArray ps; for (auto& p : params) ps << QJsonObject{{"name", p.name}, {"default", p.defaultValue}};
    QJsonObject o{{"id", id}, {"name", name}, {"category", category}, {"description", description}, {"source", source},
                  {"glsl", glsl}, {"params", ps}, {"colorA", QJsonArray{colorA[0], colorA[1], colorA[2]}},
                  {"colorB", QJsonArray{colorB[0], colorB[1], colorB[2]}}, {"photosensitive", photosensitive}, {"format", "lfeffect"}, {"version", 1}};
    if (!nodeGraph.isEmpty()) o["nodeGraph"] = nodeGraph;
    return o;
}

EffectDef EffectDef::fromJson(const QJsonObject& o) {
    EffectDef e;
    e.id = o["id"].toString(); e.name = o["name"].toString(e.id); e.category = o["category"].toString("Custom");
    e.description = o["description"].toString(); e.source = o["source"].toString("custom"); e.glsl = o["glsl"].toString();
    const QJsonArray ps = o["params"].toArray();
    for (int i = 0; i < 4; ++i) { e.params[i].name = i < ps.size() ? ps[i].toObject()["name"].toString() : "-"; e.params[i].defaultValue = i < ps.size() ? ps[i].toObject()["default"].toDouble() : 0; }
    auto rgb = [](const QJsonValue& v, std::array<double, 3> d) { auto a = v.toArray(); return a.size() == 3 ? std::array<double, 3>{a[0].toDouble(), a[1].toDouble(), a[2].toDouble()} : d; };
    e.colorA = rgb(o["colorA"], e.colorA); e.colorB = rgb(o["colorB"], e.colorB);
    e.photosensitive = o["photosensitive"].toBool(); e.nodeGraph = o["nodeGraph"].toObject();
    return e;
}

bool EffectLibrary::loadBuiltin() {
    initResources();
    QFile f(":/lumaflow/effects.json");
    if (!f.open(QIODevice::ReadOnly)) { logError("effects", "effects.json resource missing"); return false; }
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object()["effects"].toArray();
    int n = 0;
    for (auto v : arr) {
        QJsonObject o = v.toObject();
        EffectDef e = EffectDef::fromJson(o);
        e.source = "builtin";
        e.glsl = readRes(":/lumaflow/shaders/" + o["shader"].toString());
        if (e.glsl.isEmpty()) { logWarn("effects", "missing shader for " + e.id); continue; }
        add(e); ++n;
    }
    logInfo("effects", QString("Loaded %1 built-in effects").arg(n));
    return n > 0;
}

int EffectLibrary::loadDirectory(const QString& dir) {
    int n = 0;
    for (const auto& fi : QDir(dir).entryInfoList({"*.lfeffect"}, QDir::Files)) {
        EffectDef e; QString err;
        if (loadEffectFile(fi.absoluteFilePath(), e, &err)) { add(e); ++n; }
        else logWarn("effects", fi.fileName() + ": " + err);
    }
    return n;
}

bool EffectLibrary::add(const EffectDef& e) {
    if (e.id.isEmpty()) return false;
    QWriteLocker l(&m_lock);
    if (!m_effects.contains(e.id)) m_order << e.id;
    m_effects[e.id] = e; ++m_rev;
    return true;
}
bool EffectLibrary::remove(const QString& id) {
    QWriteLocker l(&m_lock);
    if (!m_effects.contains(id) || m_effects[id].source == "builtin") return false;
    m_effects.remove(id); m_order.removeAll(id); ++m_rev; return true;
}
EffectDef EffectLibrary::get(const QString& id) const { QReadLocker l(&m_lock); return m_effects.value(id); }
bool EffectLibrary::contains(const QString& id) const { QReadLocker l(&m_lock); return m_effects.contains(id); }
QVector<EffectDef> EffectLibrary::all() const { QReadLocker l(&m_lock); QVector<EffectDef> v; for (auto& id : m_order) v << m_effects[id]; return v; }
QStringList EffectLibrary::ids() const { QReadLocker l(&m_lock); return m_order; }
QStringList EffectLibrary::categories() const { QReadLocker l(&m_lock); QStringList c; for (auto& id : m_order) if (!c.contains(m_effects[id].category)) c << m_effects[id].category; return c; }
int EffectLibrary::indexOf(const QString& id) const { QReadLocker l(&m_lock); return int(m_order.indexOf(id)); }
QString EffectLibrary::idAt(int i) const { QReadLocker l(&m_lock); return i >= 0 && i < m_order.size() ? m_order[i] : QString(); }
quint64 EffectLibrary::revision() const { QReadLocker l(&m_lock); return m_rev; }
void EffectLibrary::setFavourite(const QString& id, bool fav) { QWriteLocker l(&m_lock); if (m_effects.contains(id)) { m_effects[id].favourite = fav; ++m_rev; } }

QVector<EffectDef> EffectLibrary::search(const QString& text, const QString& category) const {
    QVector<EffectDef> out;
    for (const auto& e : all()) {
        if (!category.isEmpty() && category != "All" && !(category == "Favourites" ? e.favourite : e.category == category)) continue;
        if (!text.isEmpty() && !e.name.contains(text, Qt::CaseInsensitive) && !e.category.contains(text, Qt::CaseInsensitive) && !e.description.contains(text, Qt::CaseInsensitive)) continue;
        out << e;
    }
    return out;
}

bool EffectLibrary::saveEffectFile(const QString& path, const EffectDef& e) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(e.toJson()).toJson());
    return f.commit();
}

bool EffectLibrary::loadEffectFile(const QString& path, EffectDef& e, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = "cannot open"; return false; }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (d.isNull()) { if (err) *err = pe.errorString(); return false; }
    e = EffectDef::fromJson(d.object());
    if (e.id.isEmpty() || (e.glsl.isEmpty() && e.nodeGraph.isEmpty())) { if (err) *err = "missing id or shader/node graph"; return false; }
    if (e.source == "builtin") e.source = "custom";
    return true;
}

} // namespace lf
