#pragma once
// Pixel mapping engine: samples a rendered canvas at mapped pixel positions
// and produces DMX universes for LED strips, bars, matrices and multi-cell or
// single-cell fixtures. Handles channel order, universe splitting, gamma,
// brightness limiting, colour calibration and conflict detection.
#include "fixtures/Fixture.h"
#include "fixtures/ColorEngine.h"
#include <QImage>
#include <QPointF>
#include <QMap>

namespace lf {

enum class PixelOrder { RGB, RBG, GRB, GBR, BRG, BGR, RGBW, GRBW, W };
QString pixelOrderName(PixelOrder o);
PixelOrder pixelOrderFromName(const QString& n);
int pixelOrderChannels(PixelOrder o);

enum class UniverseSplit { PixelAligned, Continuous };   // never split a pixel / split anywhere
enum class Sampling { Nearest, Area };

// A mapped element: either a raw pixel run (strip/matrix) or a patched fixture.
struct PixelMapElement {
    QString name;
    enum Kind { Strip, Matrix, Fixture } kind = Strip;
    // Strip/Matrix
    PixelOrder order = PixelOrder::RGB;
    int universe = 1, address = 1;
    UniverseSplit split = UniverseSplit::PixelAligned;
    QVector<QPointF> points;     // normalised canvas coordinates (0..1), in DMX order
    double sampleRadius = 0.0;   // normalised; 0 = nearest
    // Fixture
    int fid = 0;                 // patched fixture
    QVector<QPointF> cellPoints; // per-cell sample points
    // common
    bool enabled = true;
    ColorCalibration calibration;
    QJsonObject toJson() const;
    static PixelMapElement fromJson(const QJsonObject& o);
};

struct ChannelConflict { int universe; int slot; QString a, b; };

struct PixelMapStats { int pixels = 0; int universes = 0; int channels = 0; QVector<ChannelConflict> conflicts; QStringList warnings; };

class PixelMap {
public:
    QVector<PixelMapElement> elements;
    double masterBrightness = 1.0;     // global limiter (0..1)
    double globalGamma = 2.2;          // perceptual correction applied to all pixel outputs
    Sampling sampling = Sampling::Area;

    // --- generators
    static PixelMapElement makeStrip(const QString& name, QPointF a, QPointF b, int pixels, int universe, int address, PixelOrder order = PixelOrder::RGB);
    enum class StartCorner { TopLeft, TopRight, BottomLeft, BottomRight };
    static PixelMapElement makeMatrix(const QString& name, QRectF area, int cols, int rows, int universe, int address,
                                      PixelOrder order = PixelOrder::RGB, bool zigzag = true, StartCorner start = StartCorner::TopLeft,
                                      bool columnMajor = false, bool reversed = false);
    // Spatial mapping: projects fixture positions (front view X/Z, or top view X/Y) into canvas space.
    enum class Projection { Front, Top };
    static QVector<PixelMapElement> makeFromPatch(const Patch& patch, Projection proj, double marginNorm = 0.05);

    // --- runtime
    // Computes DMX output for all elements. `canvas` is the program output image.
    void render(const QImage& canvas, const Patch& patch, QMap<int, DmxFrame>& out) const;
    // Test patterns (independent of canvas): 0 = off, 1 = full white, 2 = RGB identify per element, 3 = chase
    void renderTestPattern(int pattern, double time, const Patch& patch, QMap<int, DmxFrame>& out) const;

    // Address planning for a strip element; returns (universe, slot) for each channel of pixel i.
    static QVector<QPair<int, int>> channelSlots(const PixelMapElement& e, int pixelIndex);
    PixelMapStats analyze(const Patch& patch) const;
    QSet<int> universesUsed(const Patch& patch) const;

    QJsonObject toJson() const;
    static PixelMap fromJson(const QJsonObject& o);
};

} // namespace lf
