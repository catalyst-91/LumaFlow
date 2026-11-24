#pragma once
// Shared parameter registry. Every controllable value in LUMAFLOW (layer
// opacity, effect speed, master intensity, ...) is addressed by a stable string
// id such as "layer1.speed" or "master.intensity". Console mappings, OSC, MIDI,
// remote clients, the timeline and the fixture personality generator all go
// through this registry so that there is exactly one source of truth.
#include <QString>
#include <QStringList>
#include <QHash>
#include <QReadWriteLock>
#include <QJsonObject>
#include <QVector>
#include <functional>

namespace lf {

enum class ParamType { Continuous, Index, Trigger, Toggle };

struct ParameterDef {
    QString id;          // stable identifier, never reused for a different meaning
    QString name;        // human readable
    QString group;       // "Master", "Layer 1", ...
    ParamType type = ParamType::Continuous;
    double minimum = 0.0;
    double maximum = 1.0;
    double defaultValue = 0.0;
    int resolution = 8;  // preferred DMX resolution when exported (8 or 16)
    int indexCount = 0;  // for Index params: number of discrete entries
    QJsonObject toJson() const;
    static ParameterDef fromJson(const QJsonObject& o);
    double normalize(double v) const { return maximum > minimum ? (v - minimum) / (maximum - minimum) : 0.0; }
    double denormalize(double n) const { return minimum + n * (maximum - minimum); }
};

class ParameterRegistry {
public:
    using Listener = std::function<void(const QString& id, double value, const QString& source)>;

    void define(const ParameterDef& def);
    bool has(const QString& id) const;
    ParameterDef def(const QString& id) const;
    QVector<ParameterDef> all() const;   // in definition order
    QStringList ids() const;

    double value(const QString& id) const;
    // Sets a value (clamped). Returns false for unknown ids.
    bool set(const QString& id, double value, const QString& source = QStringLiteral("internal"));
    bool setNormalized(const QString& id, double n, const QString& source = QStringLiteral("internal"));
    void resetToDefaults();

    int addListener(Listener l);
    void removeListener(int handle);
    quint64 revision() const;        // increments on any value change
    quint64 structureRevision() const; // increments when definitions change

    QJsonObject valuesToJson() const;
    void valuesFromJson(const QJsonObject& o);

private:
    mutable QReadWriteLock m_lock;
    QVector<ParameterDef> m_defs;
    QHash<QString, int> m_index;
    QHash<QString, double> m_values;
    QHash<int, Listener> m_listeners;
    int m_nextListener = 1;
    quint64 m_revision = 0;
    quint64 m_structRevision = 0;
};

} // namespace lf
