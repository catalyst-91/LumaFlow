#include "PluginHost.h"
#include "core/Log.h"
#include "recovery/Recovery.h"
#include <QLibrary>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QFile>

namespace lf {

QString PluginHost::categoryName(int c) {
    switch (c) { case LF_PLUGIN_EFFECT: return "Visual effects"; case LF_PLUGIN_SHADER_NODE: return "Shader nodes"; case LF_PLUGIN_PROTOCOL: return "Control protocol";
    case LF_PLUGIN_FIXTURE_IMPORT: return "Fixture importer"; case LF_PLUGIN_FIXTURE_EXPORT: return "Fixture exporter"; case LF_PLUGIN_OUTPUT: return "Output module";
    case LF_PLUGIN_MEDIA_DECODER: return "Media decoder"; case LF_PLUGIN_AUTOMATION: return "Automation"; }
    return "Unknown";
}

QJsonObject PluginRecord::toJson() const {
    return {{"path", path}, {"id", id}, {"name", name}, {"version", version}, {"vendor", vendor}, {"description", description},
            {"api", apiVersion}, {"category", category}, {"requires", QJsonArray::fromStringList(dependencies)}, {"effects", effects},
            {"nodes", nodes}, {"outputs", outputs}, {"enabled", enabled}, {"loaded", loaded}, {"status", status}};
}

PluginHost::~PluginHost() {
    for (auto* l : m_libs) {
        if (auto fn = reinterpret_cast<LfPluginShutdownFn>(l->resolve("lf_plugin_shutdown"))) fn();
        l->unload(); delete l;
    }
}

static const char* safeStr(const char* s) { return s ? s : ""; }

bool PluginHost::probeInProcess(const QString& path, QJsonObject& out, QString* err) {
    QLibrary lib(path);
    if (!lib.load()) { if (err) *err = lib.errorString(); return false; }
    auto q = reinterpret_cast<LfPluginQueryFn>(lib.resolve("lf_plugin_query"));
    if (!q) { if (err) *err = "lf_plugin_query not exported"; lib.unload(); return false; }
    const LfPluginInfo* info = q();
    if (!info) { if (err) *err = "lf_plugin_query returned NULL"; lib.unload(); return false; }
    QJsonArray fx;
    for (int i = 0; i < info->effect_count && info->effects; ++i) fx << QString(safeStr(info->effects[i].id));
    out = QJsonObject{{"api", info->api_version}, {"id", safeStr(info->id)}, {"name", safeStr(info->name)}, {"version", safeStr(info->version)},
                      {"vendor", safeStr(info->vendor)}, {"description", safeStr(info->description)}, {"category", info->category},
                      {"requires", safeStr(info->requires_csv)}, {"effects", fx}, {"nodes", info->node_count}, {"outputs", info->output_count}};
    lib.unload();
    return true;
}

static QStringList libraryFilters() {
#if defined(Q_OS_WIN)
    return {"*.dll"};
#elif defined(Q_OS_MACOS)
    return {"*.dylib", "*.so"};
#else
    return {"*.so"};
#endif
}

static double hostGetParam(const char*) { return 0.0; }
static int hostSetParam(const char*, double) { return 0; }
static void hostLog(int level, const char* msg) { Log::instance().write(level >= 3 ? LogLevel::Error : level == 2 ? LogLevel::Warning : LogLevel::Info, "plugin", QString::fromUtf8(msg ? msg : "")); }

QVector<PluginRecord> PluginHost::scanAndLoad(EffectLibrary* effects, bool safeMode) {
    m_records.clear();
    QMap<QString, QJsonObject> probed;
    for (const auto& dir : m_dirs) {
        for (const auto& fi : QDir(dir).entryInfoList(libraryFilters(), QDir::Files)) {
            PluginRecord r; r.path = fi.absoluteFilePath(); r.name = fi.fileName();
            QJsonObject info; QString err; bool ok = false;
            if (!m_probe.isEmpty() && QFileInfo::exists(m_probe)) {
                // out-of-process probe: a crash here cannot affect the host
                QProcess p; p.start(m_probe, {"plugin-probe", r.path});
                if (!p.waitForFinished(5000)) { p.kill(); err = "probe timed out"; }
                else if (p.exitStatus() != QProcess::NormalExit) err = "probe crashed while loading plugin";
                else if (p.exitCode() != 0) err = QString::fromUtf8(p.readAllStandardError()).trimmed();
                else { info = QJsonDocument::fromJson(p.readAllStandardOutput()).object(); ok = !info.isEmpty(); if (!ok) err = "probe returned no data"; }
            } else ok = probeInProcess(r.path, info, &err);
            if (!ok) { r.status = err; m_records << r; FaultRegistry::instance().report(FaultSeverity::Warning, "plugin", fi.fileName() + ": " + err, "Remove or update the plugin."); continue; }
            r.apiVersion = info["api"].toInt(); r.id = info["id"].toString(); r.name = info["name"].toString(); r.version = info["version"].toString();
            r.vendor = info["vendor"].toString(); r.description = info["description"].toString(); r.category = info["category"].toInt();
            r.dependencies = info["requires"].toString().split(',', Qt::SkipEmptyParts);
            r.effects = int(info["effects"].toArray().size()); r.nodes = info["nodes"].toInt(); r.outputs = info["outputs"].toInt();
            r.enabled = !m_disabled.contains(r.id);
            if (r.apiVersion != LF_PLUGIN_API_VERSION) r.status = QString("API version %1 not supported (host %2)").arg(r.apiVersion).arg(LF_PLUGIN_API_VERSION);
            else r.status = "ok";
            m_records << r;
        }
    }
    QSet<QString> available; for (auto& r : m_records) if (r.status == "ok") available.insert(r.id);
    for (auto& r : m_records) {
        if (r.status != "ok") continue;
        for (auto& dep : r.dependencies) if (!available.contains(dep.trimmed())) { r.status = "missing dependency " + dep; break; }
        if (r.status != "ok" || !r.enabled) { if (!r.enabled) r.status = "disabled"; continue; }
        if (safeMode) { r.status = "not loaded (safe mode)"; continue; }
        auto* lib = new QLibrary(r.path);
        if (!lib->load()) { r.status = lib->errorString(); delete lib; continue; }
        auto q = reinterpret_cast<LfPluginQueryFn>(lib->resolve("lf_plugin_query"));
        const LfPluginInfo* info = q ? q() : nullptr;
        if (!info) { r.status = "query failed"; lib->unload(); delete lib; continue; }
        if (auto init = reinterpret_cast<LfPluginInitFn>(lib->resolve("lf_plugin_init"))) {
            static LfHostApi api{LF_PLUGIN_API_VERSION, LUMAFLOW_VERSION, hostLog, hostGetParam, hostSetParam};
            if (init(&api) != 0) { r.status = "plugin init failed"; lib->unload(); delete lib; continue; }
        }
        for (int i = 0; i < info->effect_count && info->effects && effects; ++i) {
            const LfEffectDesc& d = info->effects[i];
            EffectDef e; e.id = safeStr(d.id); e.name = safeStr(d.name); e.category = safeStr(d.category); e.glsl = safeStr(d.glsl);
            e.source = "plugin:" + r.id; e.photosensitive = d.photosensitive;
            for (int k = 0; k < 4; ++k) { e.params[k].name = d.params[k].name ? d.params[k].name : "-"; e.params[k].defaultValue = d.params[k].default_value; }
            for (int k = 0; k < 3; ++k) { e.colorA[k] = d.color_a[k]; e.colorB[k] = d.color_b[k]; }
            if (!e.id.isEmpty() && !e.glsl.isEmpty()) effects->add(e);
        }
        for (int i = 0; i < info->output_count && info->outputs; ++i) m_outputs << info->outputs[i];
        r.loaded = true; m_libs << lib;
        logInfo("plugin", QString("Loaded plugin %1 %2 (%3 effects)").arg(r.id, r.version).arg(r.effects));
    }
    return m_records;
}

bool PluginHost::install(const QString& file, QString* err) {
    if (m_dirs.isEmpty()) { if (err) *err = "no plugin directory configured"; return false; }
    QJsonObject info; QString perr;
    if (!m_probe.isEmpty() && QFileInfo::exists(m_probe)) {
        QProcess p; p.start(m_probe, {"plugin-probe", file});
        if (!p.waitForFinished(5000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) { if (err) *err = "plugin failed validation probe"; return false; }
        info = QJsonDocument::fromJson(p.readAllStandardOutput()).object();
    } else if (!probeInProcess(file, info, &perr)) { if (err) *err = perr; return false; }
    if (info["api"].toInt() != LF_PLUGIN_API_VERSION) { if (err) *err = "incompatible plugin API version"; return false; }
    QDir().mkpath(m_dirs.first());
    const QString dst = QDir(m_dirs.first()).filePath(QFileInfo(file).fileName());
    QFile::remove(dst);
    if (!QFile::copy(file, dst)) { if (err) *err = "copy failed"; return false; }
    return true;
}

bool PluginHost::uninstall(const QString& id, QString* err) {
    for (auto& r : m_records) if (r.id == id) {
        if (r.loaded) { if (err) *err = "plugin is loaded; it will be removed after restart"; QFile::rename(r.path, r.path + ".remove"); return true; }
        return QFile::remove(r.path);
    }
    if (err) *err = "unknown plugin";
    return false;
}

void PluginHost::setEnabled(const QString& id, bool on) { if (on) m_disabled.remove(id); else m_disabled.insert(id); for (auto& r : m_records) if (r.id == id) r.enabled = on; }

} // namespace lf
