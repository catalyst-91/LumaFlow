#pragma once
// Project file I/O: portable JSON documents with schema versioning,
// migrations, atomic writes, versioned backups and dependency checking.
// File types: .lumaflow (project), .lfpreset, .lfscene, .lfeffect.
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace lf {

constexpr int kProjectSchemaVersion = 3;

struct LoadResult {
    QJsonObject doc;
    QStringList warnings;       // migrations applied, unknown keys, ...
    QString error;
    int originalVersion = 0;
    bool ok() const { return error.isEmpty(); }
};

struct DependencyReport {
    QStringList missingMedia;
    QStringList unresolvedFixtureTypes;
    QStringList missingPlugins;
    QStringList missingEffects;
    bool clean() const { return missingMedia.isEmpty() && unresolvedFixtureTypes.isEmpty() && missingPlugins.isEmpty() && missingEffects.isEmpty(); }
    QStringList lines() const;
};

class ProjectIO {
public:
    // Writes atomically (temp file + rename). If backupDir is set and the
    // target exists, the previous version is copied to a timestamped backup and
    // only the newest `keepBackups` are retained.
    static bool save(const QString& path, const QJsonObject& doc, QString* err = nullptr,
                     const QString& backupDir = {}, int keepBackups = 20);
    static LoadResult load(const QString& path);
    static LoadResult parse(const QByteArray& data, const QString& expectedFormat = "lumaflow-project");
    static QJsonObject migrate(QJsonObject doc, int fromVersion, QStringList& log);
    static QStringList listBackups(const QString& backupDir, const QString& baseName = {});
    static bool restoreBackup(const QString& backupPath, const QString& targetPath, QString* err = nullptr);
    static DependencyReport checkDependencies(const QJsonObject& doc, const QStringList& knownEffects, const QStringList& knownPlugins,
                                              const QString& projectDir);
    // Rewrites media paths that are missing by searching `searchDir` for files with the same name.
    static int relinkMedia(QJsonObject& doc, const QString& searchDir);
    static bool saveTyped(const QString& path, const QString& format, const QJsonObject& body);
    static LoadResult loadTyped(const QString& path, const QString& format);
};

} // namespace lf
