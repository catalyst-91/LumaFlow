#pragma once
// Fixture model: fixture types with DMX modes, a capability profile derived
// from the channel attributes, fixture instances placed in 3D space and a
// patch with conflict and universe-boundary validation.
#include "core/Math.h"
#include "protocols/Dmx.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

namespace lf {

enum class Attribute {
    Unknown = 0, Dimmer, DimmerFine,
    Red, Green, Blue, White, Amber, UV, Lime, Cyan, Magenta, Yellow, CTO, ColorWheel,
    Pan, Tilt, PanFine, TiltFine, Zoom, Focus, Iris, Shutter, Strobe, Gobo, GoboRotate, Prism,
    Control, Effect, Speed
};

QString attributeName(Attribute a);          // LUMAFLOW name e.g. "Red"
Attribute attributeFromName(const QString& n);
Attribute attributeFromGdtf(const QString& gdtfAttr);   // e.g. "ColorAdd_R" -> Red
QString attributeToGdtf(Attribute a);

enum class ColorModel { None, RGB, RGBW, RGBA, RGBWA, RGBWAUV, RGBAW_UV = RGBWAUV, CMY };
QString colorModelName(ColorModel m);

struct ChannelDef {
    Attribute attribute = Attribute::Unknown;
    QString attributeRaw;       // original attribute string (GDTF) for round-trip
    int offset = 0;             // 0-based offset of coarse byte within the mode footprint
    int fineOffset = -1;        // 0-based offset of fine byte (16-bit) or -1
    int cell = 0;               // multi-cell index (0 for single-cell parts)
    int defaultValue = 0;       // DMX default (coarse resolution)
    QString name;
    QJsonObject toJson() const;
    static ChannelDef fromJson(const QJsonObject& o);
};

struct FixtureMode {
    QString name;
    QVector<ChannelDef> channels;
    int cellCount = 1;
    QVector<Vec3> cellOffsets;  // per-cell offsets (metres) relative to fixture origin
    int footprint() const;
    QJsonObject toJson() const;
    static FixtureMode fromJson(const QJsonObject& o);
};

struct Capabilities {
    bool dimmer = false, pan = false, tilt = false, zoom = false, focus = false, iris = false;
    bool shutter = false, strobe = false, gobo = false, prism = false, colorWheel = false, cto = false;
    ColorModel color = ColorModel::None;
    int cells = 1;
    bool multiCell() const { return cells > 1; }
    bool hasColor() const { return color != ColorModel::None; }
    bool needsVirtualDimmer() const { return !dimmer && hasColor(); }
    QStringList describe() const;
};

struct FixtureType {
    QString id;                 // unique: "Manufacturer@Name" or GDTF FixtureTypeID
    QString manufacturer;
    QString name;
    QString source;             // "builtin", "gdtf:<file>", "user"
    QVector<FixtureMode> modes;
    const FixtureMode* mode(const QString& name) const;
    QJsonObject toJson() const;
    static FixtureType fromJson(const QJsonObject& o);
};

Capabilities analyzeCapabilities(const FixtureMode& mode);

struct FixtureInstance {
    int fid = 0;                // operator fixture ID
    QString uuid;               // MVR UUID preserved for round-trip
    QString name;
    QString typeId;
    QString modeName;
    int universe = 1;           // 1-based
    int address = 1;            // 1..512
    Vec3 position;              // metres, LUMAFLOW coords: X right, Y depth (upstage +), Z up
    Vec3 rotation;              // degrees XYZ
    QStringList groups;
    bool excluded = false;      // exclude from spatial effects
    QString layer;              // MVR layer name
    QJsonObject toJson() const;
    static FixtureInstance fromJson(const QJsonObject& o);
};

// Absolute DMX slot helpers (0-based absolute = (universe-1)*512 + address-1)
inline int absoluteSlot(int universe, int address) { return (universe - 1) * kDmxSlots + (address - 1); }

class FixtureLibrary {
public:
    static FixtureLibrary& builtin();          // generic fixtures shipped with LUMAFLOW
    void add(const FixtureType& t);
    const FixtureType* find(const QString& id) const;
    QVector<FixtureType> all() const { return m_types.values().toVector(); }
    QStringList ids() const { return m_types.keys(); }
    QJsonArray toJson() const;
private:
    QMap<QString, FixtureType> m_types;
};

struct PatchIssue { enum Kind { Overlap, CrossesUniverse, MissingType, InvalidAddress } kind; int fid; int otherFid; QString message; };

class Patch {
public:
    QVector<FixtureInstance> fixtures;
    FixtureLibrary types;                      // types referenced by this show (copied in)

    const FixtureType* typeOf(const FixtureInstance& f) const;
    const FixtureMode* modeOf(const FixtureInstance& f) const;
    int footprintOf(const FixtureInstance& f) const;
    FixtureInstance* byFid(int fid);
    const FixtureInstance* byFid(int fid) const;
    QVector<PatchIssue> validate(bool allowUniverseSpan = false) const;
    int nextFreeAddress(int universe, int footprint) const;   // -1 if universe full
    QJsonObject toJson() const;
    static Patch fromJson(const QJsonObject& o);
};

// Writes attribute values (0..1 normalised) for one fixture into DMX universes.
// Handles 8/16-bit channels, per-cell values and channels that span universes
// when the fixture is patched across a boundary.
struct AttributeValues {
    QHash<int, double> perAttribute;           // key = int(Attribute) for cell 0 / whole fixture
    QVector<QHash<int, double>> perCell;       // optional per-cell overrides
    double get(Attribute a, int cell, double fallback) const;
};
void writeFixtureDmx(const FixtureInstance& f, const FixtureMode& mode, const AttributeValues& v,
                     QMap<int, DmxFrame>& universes);

} // namespace lf
