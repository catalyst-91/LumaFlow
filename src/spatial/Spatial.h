#pragma once
// Spatial effect generator: orders fixtures by their real 3D positions and
// evaluates phase-offset effects (chases, waves, colour sweeps) that respect
// each fixture's capabilities (virtual dimmer for colour-only fixtures,
// intensity-only effects for dimmers, per-cell evaluation for multi-cell).
#include "fixtures/Fixture.h"
#include "fixtures/ColorEngine.h"
#include <QMap>

namespace lf {

enum class SpatialOrder {
    LeftToRight, RightToLeft, FrontToBack, BackToFront, TopToBottom, BottomToTop,
    CentreOut, OutsideIn, RadialCW, RadialCCW, Diagonal, Symmetrical, Mirrored,
    Random, CustomGrid, UserDefined
};
QString spatialOrderName(SpatialOrder o);
SpatialOrder spatialOrderFromName(const QString& s);
QStringList spatialOrderNames();

enum class Waveform { Sine, Square, Sawtooth, Ramp, Triangle, Pulse, Chase };
QString waveformName(Waveform w);
Waveform waveformFromName(const QString& s);

struct SpatialEffect {
    QString name = "Spatial Effect";
    SpatialOrder order = SpatialOrder::LeftToRight;
    Waveform waveform = Waveform::Sine;
    double speed = 0.5;        // cycles per second
    double phaseSpread = 1.0;  // total phase distributed across ordering (1 = one full cycle)
    double width = 0.5;        // duty for square/pulse/chase (0..1)
    double offset = 0.0;       // phase offset 0..1
    double delay = 0.0;        // seconds of extra delay per ordering step (adds to phase)
    int wings = 1;             // mirror ordering into N wings
    int blocks = 1;            // fixtures per block share a phase
    int groups = 1;            // repeat phase pattern across N groups
    bool reverse = false;      // direction
    bool bounce = false;       // ping-pong
    quint32 seed = 1;          // reproducible randomisation
    double low = 0.0, high = 1.0; // intensity range
    RGB colorA{1, 1, 1}, colorB{0, 0, 0};  // effect colour at high / low
    bool colorSweep = false;   // hue rotates across fixtures
    QVector<int> userOrder;    // fixture IDs for UserDefined
    QString group;             // restrict to fixture group ("" = all)
    QSet<int> excluded;        // fixture IDs excluded
    int gridColumns = 4;       // CustomGrid
    QJsonObject toJson() const;
    static SpatialEffect fromJson(const QJsonObject& o);
};

struct SpatialSample { int fid; int cell; Vec3 position; double phase; };

class SpatialEngine {
public:
    // Computes normalised phase (0..1) for every fixture cell participating.
    static QVector<SpatialSample> computePhases(const Patch& patch, const SpatialEffect& fx);
    static double waveform(Waveform w, double x, double width);   // x in cycles, returns 0..1
    // Evaluate the effect at time t and write DMX for all participating fixtures.
    static void render(const Patch& patch, const SpatialEffect& fx, double t, QMap<int, DmxFrame>& out,
                       const ColorCalibration& cal = {}, QMap<int, double>* levelsOut = nullptr);
    // Evaluate per-fixture level/colour without DMX (for previews).
    struct Result { int fid; int cell; double level; RGB color; Vec3 position; };
    static QVector<Result> evaluate(const Patch& patch, const SpatialEffect& fx, double t);
};

} // namespace lf
