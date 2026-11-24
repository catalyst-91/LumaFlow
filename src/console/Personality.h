#pragma once
// Media-server fixture personality generator.
// A single internal channel catalogue (ordered list of exposed parameters) is
// used to derive the Basic(16) / Standard(32) / Extended(64) / Advanced(128)
// modes. Lower modes are strict prefixes of higher modes and the catalogue is
// append-only, so adding parameters never moves existing DMX channels.
// Profiles are versioned; any change to an active profile must be approved by
// the operator, breaking changes are flagged, and previous versions can be
// restored (rollback).
#include "core/Parameters.h"
#include "fixtures/Gdtf.h"
#include <QString>
#include <QVector>
#include <QJsonObject>

namespace lf {

enum class PersonalityMode { Basic16, Standard32, Extended64, Advanced128, Custom };
QString personalityModeName(PersonalityMode m);
int personalityModeChannels(PersonalityMode m);
PersonalityMode personalityModeFromName(const QString& s);

struct PersonalityChannel {
    int offset = 1;              // 1-based coarse channel
    int fineOffset = 0;          // 1-based fine channel or 0
    QString paramId;
    QString name;
    QString gdtfAttribute;
    QStringList indexNames;      // for index parameters: names of DMX ranges
    QJsonObject toJson() const;
    static PersonalityChannel fromJson(const QJsonObject& o);
};

struct PersonalityProfile {
    QString name = "LUMAFLOW Media Server";
    PersonalityMode mode = PersonalityMode::Standard32;
    int version = 1;
    QString createdAt;
    QVector<PersonalityChannel> channels;
    int footprint() const;
    QString fingerprint() const; // stable hash of layout
    QJsonObject toJson() const;
    static PersonalityProfile fromJson(const QJsonObject& o);
    const PersonalityChannel* channelFor(const QString& paramId) const;
};

struct ProfileChange {
    enum Kind { Added, Removed, Moved, ResolutionChanged, Renamed, IndexRangesChanged } kind;
    QString paramId; QString detail; bool breaking;
};

struct ProfileDiff {
    QVector<ProfileChange> changes;
    bool breaking() const { for (auto& c : changes) if (c.breaking) return true; return false; }
    bool empty() const { return changes.isEmpty(); }
    QString summary() const;
};

class PersonalityGenerator {
public:
    // Channel catalogue entry: parameter id + resolution
    struct CatalogEntry { QString paramId; int resolution; QString gdtfAttribute; };
    static QVector<CatalogEntry> catalogue(int layerCount = 6);
    static PersonalityProfile generate(const ParameterRegistry& reg, PersonalityMode mode, const QStringList& effectNames,
                                       const QStringList& sceneNames, const QStringList& customParams = {});
    static ProfileDiff compare(const PersonalityProfile& oldP, const PersonalityProfile& newP);

    // Exporters
    static gdtf::ExportSpec toGdtfSpec(const QVector<PersonalityProfile>& modes);
    static bool exportGdtf(const QString& path, const QVector<PersonalityProfile>& modes, QString* err = nullptr);
    static QString magicqChannelSheet(const PersonalityProfile& p);   // CSV for MagicQ Head Editor
    static QString grandma3Guide(const PersonalityProfile& p);         // patch/programming notes
    static QString dmxReference(const PersonalityProfile& p);          // markdown table
    static QVector<double> decodeDmx(const PersonalityProfile& p, const quint8* dmxSlots, int count); // normalised values per channel
};

// Holds the active (console-facing) profile per mode, pending candidates, and history.
class ProfileManager {
public:
    PersonalityProfile active(PersonalityMode m) const;
    bool hasActive(PersonalityMode m) const { return m_active.contains(int(m)); }
    // Proposes a regenerated profile. Returns the diff vs. active. The candidate
    // is held as pending and NOT applied until approve() is called.
    ProfileDiff propose(const PersonalityProfile& candidate);
    bool hasPending(PersonalityMode m) const { return m_pending.contains(int(m)); }
    PersonalityProfile pending(PersonalityMode m) const { return m_pending.value(int(m)); }
    // Approves the pending profile. Breaking changes need allowBreaking=true.
    bool approve(PersonalityMode m, bool allowBreaking, QString* why = nullptr);
    void reject(PersonalityMode m) { m_pending.remove(int(m)); }
    bool rollback(PersonalityMode m);
    QVector<PersonalityProfile> history(PersonalityMode m) const { return m_history.value(int(m)); }
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
private:
    QMap<int, PersonalityProfile> m_active, m_pending;
    QMap<int, QVector<PersonalityProfile>> m_history;
};

} // namespace lf
