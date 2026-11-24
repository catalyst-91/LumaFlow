#pragma once
// Effect library: built-in GLSL effects (from resources/effects.json), custom
// node-graph effects (.lfeffect), user shader effects and plugin effects.
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QMap>
#include <QReadWriteLock>
#include <array>

namespace lf {

struct EffectParam { QString name; double defaultValue = 0.0; };

struct EffectDef {
    QString id;
    QString name;
    QString category;
    QString description;
    QString source;             // "builtin", "custom", "plugin:<name>", "user", "ai"
    QString glsl;               // body defining vec4 effect(vec2 p, vec2 uv, float t)
    std::array<EffectParam, 4> params;
    std::array<double, 3> colorA{1, 1, 1}, colorB{0, 0, 1};
    bool photosensitive = false;
    bool favourite = false;
    QJsonObject nodeGraph;      // for node-based custom effects
    QJsonObject toJson() const;
    static EffectDef fromJson(const QJsonObject& o);
};

class EffectLibrary {
public:
    static QString prelude();          // shader prelude (uniforms + helpers)
    static QString epilogue();         // main() wrapper
    static QString vertexShader();

    bool loadBuiltin();                // from Qt resources
    int loadDirectory(const QString& dir);   // *.lfeffect
    bool add(const EffectDef& e);      // replaces same id
    bool remove(const QString& id);
    EffectDef get(const QString& id) const;
    bool contains(const QString& id) const;
    QVector<EffectDef> all() const;
    QStringList ids() const;           // stable order: builtin order then custom by name
    QStringList categories() const;
    QVector<EffectDef> search(const QString& text, const QString& category = {}) const;
    int indexOf(const QString& id) const;
    QString idAt(int index) const;
    quint64 revision() const;
    void setFavourite(const QString& id, bool fav);
    static bool saveEffectFile(const QString& path, const EffectDef& e);
    static bool loadEffectFile(const QString& path, EffectDef& e, QString* err = nullptr);
private:
    mutable QReadWriteLock m_lock;
    QMap<QString, EffectDef> m_effects;
    QStringList m_order;
    quint64 m_rev = 0;
};

} // namespace lf
