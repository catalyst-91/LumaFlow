#pragma once
// Colour conversion and virtual dimmer engine.
// Converts an RGB intent (0..1, linear-ish "operator" values) plus an intensity
// into the emitter values of a particular fixture colour model, applying white
// extraction, calibration, saturation, gamma and brightness limiting.
#include "Fixture.h"

namespace lf {

struct RGB { double r = 0, g = 0, b = 0; };

enum class WhiteMethod { None, MinExtract, Luminance };   // how W is derived from RGB

struct ColorCalibration {
    double gainR = 1, gainG = 1, gainB = 1, gainW = 1, gainA = 1, gainUV = 1;
    double gamma = 1.0;            // output gamma (2.2 typical for LED perceptual response)
    double maxBrightness = 1.0;    // output limiter
    double saturation = 1.0;       // 0 = mono, 1 = unchanged, >1 boost
    WhiteMethod white = WhiteMethod::MinExtract;
    double amberFromRG = 0.0;      // 0..1 how much amber to extract from R+G overlap
    QJsonObject toJson() const;
    static ColorCalibration fromJson(const QJsonObject& o);
};

struct EmitterValues { double dimmer = 1, r = 0, g = 0, b = 0, w = 0, a = 0, uv = 0, c = 0, m = 0, y = 0; };

class ColorEngine {
public:
    // Core conversion. `intensity` is the operator dimmer level.
    // If the fixture has a physical dimmer, intensity goes to the dimmer and the
    // colour mix stays at full (no double dimming). Otherwise the virtual
    // dimmer scales the colour emitters.
    static EmitterValues convert(const RGB& rgb, double intensity, const Capabilities& caps, const ColorCalibration& cal = {});
    static AttributeValues toAttributes(const EmitterValues& e, const Capabilities& caps);
    static RGB applySaturation(const RGB& c, double s);
    static RGB fromHsv(double h, double s, double v);
    static RGB fromKelvin(double kelvin);          // approximate blackbody (1000..40000K)
    static double gammaEncode(double v, double gamma) { return gamma == 1.0 ? v : std::pow(clamp01(v), 1.0 / gamma); }
    static double gammaDecode(double v, double gamma) { return gamma == 1.0 ? v : std::pow(clamp01(v), gamma); }
};

} // namespace lf
