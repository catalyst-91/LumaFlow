#include "ColorEngine.h"
#include <cmath>

namespace lf {

QJsonObject ColorCalibration::toJson() const {
    return {{"gainR", gainR}, {"gainG", gainG}, {"gainB", gainB}, {"gainW", gainW}, {"gainA", gainA}, {"gainUV", gainUV},
            {"gamma", gamma}, {"maxBrightness", maxBrightness}, {"saturation", saturation}, {"white", int(white)}, {"amberFromRG", amberFromRG}};
}
ColorCalibration ColorCalibration::fromJson(const QJsonObject& o) {
    ColorCalibration c;
    c.gainR = o["gainR"].toDouble(1); c.gainG = o["gainG"].toDouble(1); c.gainB = o["gainB"].toDouble(1);
    c.gainW = o["gainW"].toDouble(1); c.gainA = o["gainA"].toDouble(1); c.gainUV = o["gainUV"].toDouble(1);
    c.gamma = o["gamma"].toDouble(1); c.maxBrightness = o["maxBrightness"].toDouble(1); c.saturation = o["saturation"].toDouble(1);
    c.white = WhiteMethod(o["white"].toInt(1)); c.amberFromRG = o["amberFromRG"].toDouble(0);
    return c;
}

RGB ColorEngine::applySaturation(const RGB& c, double s) {
    const double l = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    return {clamp01(l + (c.r - l) * s), clamp01(l + (c.g - l) * s), clamp01(l + (c.b - l) * s)};
}

RGB ColorEngine::fromHsv(double h, double s, double v) {
    h = std::fmod(h, 1.0); if (h < 0) h += 1.0;
    const double i = std::floor(h * 6), f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (int(i) % 6) { case 0: return {v, t, p}; case 1: return {q, v, p}; case 2: return {p, v, t}; case 3: return {p, q, v}; case 4: return {t, p, v}; default: return {v, p, q}; }
}

RGB ColorEngine::fromKelvin(double k) {
    // Tanner Helland's curve fit of the CIE 1964 blackbody locus.
    const double t = std::clamp(k, 1000.0, 40000.0) / 100.0;
    double r, g, b;
    if (t <= 66) { r = 255; g = 99.4708025861 * std::log(t) - 161.1195681661; }
    else { r = 329.698727446 * std::pow(t - 60, -0.1332047592); g = 288.1221695283 * std::pow(t - 60, -0.0755148492); }
    if (t >= 66) b = 255; else if (t <= 19) b = 0; else b = 138.5177312231 * std::log(t - 10) - 305.0447927307;
    return {clamp01(r / 255), clamp01(g / 255), clamp01(b / 255)};
}

EmitterValues ColorEngine::convert(const RGB& in, double intensity, const Capabilities& caps, const ColorCalibration& cal) {
    EmitterValues e;
    RGB c = cal.saturation != 1.0 ? applySaturation(in, cal.saturation) : in;
    intensity = clamp01(intensity) * clamp01(cal.maxBrightness);
    const bool physical = caps.dimmer;
    const double colourScale = physical ? 1.0 : intensity;    // virtual dimmer
    e.dimmer = physical ? intensity : 1.0;

    double r = c.r, g = c.g, b = c.b, w = 0, a = 0;
    const bool hasW = caps.color == ColorModel::RGBW || caps.color == ColorModel::RGBWA || caps.color == ColorModel::RGBWAUV;
    const bool hasA = caps.color == ColorModel::RGBA || caps.color == ColorModel::RGBWA || caps.color == ColorModel::RGBWAUV;
    if (hasW && cal.white != WhiteMethod::None) {
        // White extraction: the common component of R,G,B is moved to the white emitter,
        // preserving hue; Luminance mode keeps a little RGB to retain saturation.
        const double mn = std::min({r, g, b});
        w = cal.white == WhiteMethod::MinExtract ? mn : mn * 0.8;
        r -= w; g -= w; b -= w;
    }
    if (hasA && cal.amberFromRG > 0) {
        const double rg = std::min(r, g * 2.0) * cal.amberFromRG;   // amber ~ red + half green
        a = rg; r -= rg; g -= rg * 0.5;
    }
    auto out = [&](double v, double gain) { return gammaEncode(clamp01(v * gain * colourScale), cal.gamma); };
    if (caps.color == ColorModel::CMY) {
        e.c = gammaEncode(1.0 - clamp01(c.r * colourScale), cal.gamma);
        e.m = gammaEncode(1.0 - clamp01(c.g * colourScale), cal.gamma);
        e.y = gammaEncode(1.0 - clamp01(c.b * colourScale), cal.gamma);
    } else {
        e.r = out(r, cal.gainR); e.g = out(g, cal.gainG); e.b = out(b, cal.gainB);
        e.w = out(w, cal.gainW); e.a = out(a, cal.gainA);
        e.uv = 0;
    }
    if (physical) e.dimmer = gammaEncode(e.dimmer, cal.gamma);
    if (!caps.hasColor()) e.dimmer = gammaEncode(intensity * std::max({c.r, c.g, c.b, 0.0}) , cal.gamma); // intensity-only: use luminance of the intent
    return e;
}

AttributeValues ColorEngine::toAttributes(const EmitterValues& e, const Capabilities& caps) {
    AttributeValues v;
    v.perAttribute[int(Attribute::Dimmer)] = e.dimmer;
    v.perAttribute[int(Attribute::Red)] = e.r; v.perAttribute[int(Attribute::Green)] = e.g; v.perAttribute[int(Attribute::Blue)] = e.b;
    v.perAttribute[int(Attribute::White)] = e.w; v.perAttribute[int(Attribute::Amber)] = e.a; v.perAttribute[int(Attribute::UV)] = e.uv;
    v.perAttribute[int(Attribute::Cyan)] = e.c; v.perAttribute[int(Attribute::Magenta)] = e.m; v.perAttribute[int(Attribute::Yellow)] = e.y;
    if (caps.shutter) v.perAttribute[int(Attribute::Shutter)] = 1.0;    // open
    return v;
}

} // namespace lf
