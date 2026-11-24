#pragma once
// GDTF (DIN SPEC 15800) fixture description import/export.
// Import extracts fixture type, DMX modes, channel offsets (8/16/24-bit),
// attributes, defaults and multi-instance geometry references. Export writes a
// self-consistent description.xml (GDTF 1.2 structure) inside a .gdtf zip.
#include "Fixture.h"
#include <QUuid>

namespace lf::gdtf {

struct ExportChannelSet { QString name; int dmxFrom = 0; };
struct ExportChannel {
    QString name;               // human name, e.g. "Effect Select"
    QString attribute;          // GDTF attribute name, e.g. "Dimmer", "MediaContent1"
    QString pretty;             // short UI name
    QString feature;            // "Dimmer.Dimmer", "Control.Control", ...
    int coarse = 1;             // 1-based offset
    int fine = 0;               // 1-based offset of fine byte or 0
    int defaultValue = 0;       // in channel resolution
    double physicalFrom = 0, physicalTo = 1;
    QVector<ExportChannelSet> sets;
};
struct ExportMode { QString name; QString description; QVector<ExportChannel> channels; };
struct ExportSpec {
    QString manufacturer, name, shortName, longName, description;
    QUuid fixtureTypeId;
    QString revisionText;
    QVector<ExportMode> modes;
};

struct ImportResult {
    FixtureType type;
    QStringList warnings;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

ImportResult importFile(const QString& path);
ImportResult importDescription(const QByteArray& descriptionXml, const QString& sourceName);
QByteArray exportDescription(const ExportSpec& spec);
bool exportFile(const QString& path, const ExportSpec& spec, QString* error = nullptr);
ExportSpec specFromFixtureType(const FixtureType& t);

// Structural validation of an exported description (used by tests/CLI).
QStringList validateDescription(const QByteArray& xml);

} // namespace lf::gdtf
