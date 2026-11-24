#pragma once
// Plugin host: discovery, out-of-process probing, version/dependency checks,
// enable/disable, install/remove, error isolation.
#include "lumaflow_plugin.h"
#include "effects/EffectLibrary.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QSet>

class QLibrary;

namespace lf {

struct PluginRecord {
    QString path;
    QString id, name, version, vendor, description;
    int apiVersion = 0;
    int category = 0;
    QStringList dependencies;
    int effects = 0, nodes = 0, outputs = 0;
    bool enabled = true;
    bool loaded = false;
    QString status;         // "ok", "probe crashed", "api mismatch", "missing dependency x", ...
    QJsonObject toJson() const;
};

class PluginHost {
public:
    ~PluginHost();
    void setPluginDirs(const QStringList& dirs) { m_dirs = dirs; }
    QStringList pluginDirs() const { return m_dirs; }
    void setProbeExecutable(const QString& exe) { m_probe = exe; }
    void setDisabled(const QSet<QString>& ids) { m_disabled = ids; }
    QSet<QString> disabled() const { return m_disabled; }
    // Scans plugin directories, probes each library out-of-process, then loads accepted ones.
    QVector<PluginRecord> scanAndLoad(EffectLibrary* effects, bool safeMode);
    QVector<PluginRecord> records() const { return m_records; }
    bool install(const QString& file, QString* err = nullptr);
    bool uninstall(const QString& id, QString* err = nullptr);
    void setEnabled(const QString& id, bool on);
    // In-process query used by the probe process.
    static bool probeInProcess(const QString& path, QJsonObject& out, QString* err);
    static QString categoryName(int c);
    const QVector<LfOutputDesc>& outputModules() const { return m_outputs; }
private:
    QStringList m_dirs;
    QString m_probe;
    QSet<QString> m_disabled;
    QVector<PluginRecord> m_records;
    QVector<QLibrary*> m_libs;
    QVector<LfOutputDesc> m_outputs;
};

} // namespace lf
