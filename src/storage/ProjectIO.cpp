#include "ProjectIO.h"
#include "core/Log.h"
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDirIterator>

namespace lf {

QStringList DependencyReport::lines() const {
    QStringList l;
    for (auto& m : missingMedia) l << "Missing media: " + m;
    for (auto& f : unresolvedFixtureTypes) l << "Unresolved fixture type: " + f;
    for (auto& p : missingPlugins) l << "Missing plugin: " + p;
    for (auto& e : missingEffects) l << "Missing effect: " + e;
    return l;
}

bool ProjectIO::save(const QString& path, const QJsonObject& docIn, QString* err, const QString& backupDir, int keep) {
    QJsonObject doc = docIn;
    doc["format"] = "lumaflow-project";
    doc["schemaVersion"] = kProjectSchemaVersion;
    doc["savedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    doc["savedBy"] = QString("LUMAFLOW ") + LUMAFLOW_VERSION;
    if (!backupDir.isEmpty() && QFileInfo::exists(path)) {
        QDir().mkpath(backupDir);
        const QString base = QFileInfo(path).completeBaseName();
        const QString bk = QDir(backupDir).filePath(QString("%1_%2.lumaflow").arg(base, QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")));
        QFile::copy(path, bk);
        const QStringList all = listBackups(backupDir, base);
        for (int i = keep; i < all.size(); ++i) QFile::remove(all[i]);
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { if (err) *err = f.errorString(); return false; }
    const QByteArray bytes = QJsonDocument(doc).toJson(QJsonDocument::Indented);
    f.write(bytes);
    if (!f.commit()) { if (err) *err = f.errorString(); return false; }
    // verify round trip (guards against silent corruption on full disks etc.)
    QFile check(path);
    if (!check.open(QIODevice::ReadOnly) || check.readAll() != bytes) { if (err) *err = "verification after write failed"; return false; }
    return true;
}

LoadResult ProjectIO::parse(const QByteArray& data, const QString& expected) {
    LoadResult r;
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(data, &pe);
    if (d.isNull() || !d.isObject()) { r.error = "invalid JSON: " + pe.errorString() + QString(" at offset %1").arg(pe.offset); return r; }
    QJsonObject o = d.object();
    const QString fmt = o["format"].toString();
    if (!fmt.isEmpty() && fmt != expected) { r.error = QString("file format is '%1', expected '%2'").arg(fmt, expected); return r; }
    r.originalVersion = o["schemaVersion"].toInt(1);
    if (r.originalVersion > kProjectSchemaVersion && expected == "lumaflow-project") {
        r.error = QString("project was saved by a newer LUMAFLOW (schema %1 > %2); refusing to load to avoid data loss").arg(r.originalVersion).arg(kProjectSchemaVersion);
        return r;
    }
    if (expected == "lumaflow-project" && r.originalVersion < kProjectSchemaVersion) o = migrate(o, r.originalVersion, r.warnings);
    r.doc = o;
    return r;
}

LoadResult ProjectIO::load(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { LoadResult r; r.error = "cannot open " + path + ": " + f.errorString(); return r; }
    return parse(f.readAll());
}

QJsonObject ProjectIO::migrate(QJsonObject o, int from, QStringList& log) {
    if (from < 2) {
        // v1: layers / values at top level -> "composition" object; "cues" array -> cueLists
        if (o.contains("layers") && !o.contains("composition")) {
            o["composition"] = QJsonObject{{"layers", o["layers"]}, {"values", o["values"]}};
            o.remove("layers"); o.remove("values");
            log << "migrated v1 layers -> composition";
        }
        if (o.contains("cues") && !o.contains("cueLists")) {
            o["cueLists"] = QJsonArray{QJsonObject{{"name", "Main"}, {"cues", o["cues"]}}};
            o.remove("cues"); log << "migrated v1 cues -> cueLists";
        }
    }
    if (from < 3) {
        // v2 stored network config under "artnet"/"sacn" keys; v3 uses unified "network"
        if (!o.contains("network") && (o.contains("artnet") || o.contains("sacn"))) {
            QJsonObject n;
            const QJsonObject a = o["artnet"].toObject(), s = o["sacn"].toObject();
            n["artnetOut"] = a["enabled"].toBool(); n["artnetTarget"] = a["target"].toString("2.255.255.255");
            n["sacnOut"] = s["enabled"].toBool(); n["sacnPriority"] = s["priority"].toInt(100);
            QJsonArray u; for (auto v : a["universes"].toArray()) u << v; for (auto v : s["universes"].toArray()) if (!u.contains(v)) u << v;
            n["outputUniverses"] = u;
            o["network"] = n; o.remove("artnet"); o.remove("sacn");
            log << "migrated v2 artnet/sacn -> network";
        }
    }
    log << QString("project upgraded from schema %1 to %2 (original file left untouched until saved)").arg(from).arg(kProjectSchemaVersion);
    o["schemaVersion"] = kProjectSchemaVersion;
    return o;
}

QStringList ProjectIO::listBackups(const QString& dir, const QString& base) {
    QStringList out;
    const auto files = QDir(dir).entryInfoList({base.isEmpty() ? "*.lumaflow" : base + "_*.lumaflow"}, QDir::Files, QDir::Name | QDir::Reversed);
    for (auto& f : files) out << f.absoluteFilePath();
    return out;
}

bool ProjectIO::restoreBackup(const QString& bk, const QString& target, QString* err) {
    const LoadResult r = load(bk);
    if (!r.ok()) { if (err) *err = "backup is not loadable: " + r.error; return false; }
    if (QFileInfo::exists(target)) QFile::copy(target, target + ".before-restore");
    QFile::remove(target);
    if (!QFile::copy(bk, target)) { if (err) *err = "copy failed"; return false; }
    return true;
}

static void collectMedia(const QJsonValue& v, QStringList& out) {
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if ((it.key() == "media" || it.key() == "mediaPath" || it.key() == "image") && it.value().isString() && !it.value().toString().isEmpty()) out << it.value().toString();
            else collectMedia(it.value(), out);
        }
    } else if (v.isArray()) for (auto x : v.toArray()) collectMedia(x, out);
}

DependencyReport ProjectIO::checkDependencies(const QJsonObject& doc, const QStringList& effects, const QStringList& plugins, const QString& projectDir) {
    DependencyReport r;
    QStringList media; collectMedia(doc, media); media.removeDuplicates();
    for (auto& m : media) {
        QFileInfo fi(m);
        if (fi.isRelative()) fi = QFileInfo(QDir(projectDir).filePath(m));
        if (!fi.exists()) r.missingMedia << m;
    }
    for (auto v : doc["patch"].toObject()["fixtures"].toArray()) {
        const QString t = v.toObject()["type"].toString();
        if (t.startsWith("unresolved:") && !r.unresolvedFixtureTypes.contains(t)) r.unresolvedFixtureTypes << t.mid(11);
    }
    for (auto v : doc["plugins"].toArray()) { const QString p = v.toObject()["id"].toString(); if (!plugins.contains(p)) r.missingPlugins << p; }
    auto checkComp = [&](const QJsonObject& c) {
        for (auto l : c["layers"].toArray()) { const QJsonObject lo = l.toObject(); if (lo["type"].toInt() == 0) { const QString e = lo["effect"].toString(); if (!e.isEmpty() && !effects.contains(e) && !r.missingEffects.contains(e)) r.missingEffects << e; } }
    };
    checkComp(doc["composition"].toObject());
    for (auto s : doc["scenes"].toArray()) checkComp(s.toObject()["state"].toObject());
    return r;
}

static void relinkValue(QJsonValue& v, const QMap<QString, QString>& byName, int& n) {
    if (v.isObject()) {
        QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if ((it.key() == "media" || it.key() == "mediaPath") && it.value().isString()) {
                const QString p = it.value().toString();
                if (!p.isEmpty() && !QFileInfo::exists(p) && byName.contains(QFileInfo(p).fileName())) { it.value() = byName[QFileInfo(p).fileName()]; ++n; }
            } else { QJsonValue c = it.value(); relinkValue(c, byName, n); it.value() = c; }
        }
        v = o;
    } else if (v.isArray()) { QJsonArray a = v.toArray(); for (int i = 0; i < a.size(); ++i) { QJsonValue c = a[i]; relinkValue(c, byName, n); a[i] = c; } v = a; }
}

int ProjectIO::relinkMedia(QJsonObject& doc, const QString& dir) {
    QMap<QString, QString> byName;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); byName.insert(it.fileName(), it.filePath()); }
    int n = 0; QJsonValue v = doc; relinkValue(v, byName, n); doc = v.toObject();
    return n;
}

bool ProjectIO::saveTyped(const QString& path, const QString& format, const QJsonObject& body) {
    QJsonObject o = body; o["format"] = format; o["schemaVersion"] = 1;
    QSaveFile f(path); if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson()); return f.commit();
}

LoadResult ProjectIO::loadTyped(const QString& path, const QString& format) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { LoadResult r; r.error = "cannot open " + path; return r; }
    return parse(f.readAll(), format);
}

} // namespace lf
