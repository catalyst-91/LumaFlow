#pragma once
// MVR (My Virtual Rig, DIN SPEC 15801) scene import/export.
// Coordinates: MVR uses millimetres, right-handed, Z up. LUMAFLOW uses metres
// with the same axis orientation; an optional user transform is applied.
#include "Fixture.h"

namespace lf::mvr {

struct ImportOptions {
    double unitScale = 0.001;          // MVR mm -> metres
    Vec3 originOffset;                 // added after scaling
    bool importGdtfTypes = true;       // load embedded .gdtf files
    int firstFid = 1;                  // used when FixtureID is missing
};

struct ImportResult {
    Patch patch;
    QStringList warnings;
    QString error;
    QStringList groups;
    bool ok() const { return error.isEmpty(); }
};

ImportResult importFile(const QString& path, const ImportOptions& opt = {});
ImportResult importScene(const QByteArray& sceneXml, const QMap<QString, QByteArray>& gdtfFiles, const ImportOptions& opt);
bool exportFile(const QString& path, const Patch& patch, QString* error = nullptr);
QByteArray exportScene(const Patch& patch);

// Parses "{a,b,c}{d,e,f}{g,h,i}{x,y,z}" into a Mat4 (translation in source units).
Mat4 parseMatrix(const QString& s);
QString formatMatrix(const Vec3& posMm, const Vec3& rotDeg);

} // namespace lf::mvr
