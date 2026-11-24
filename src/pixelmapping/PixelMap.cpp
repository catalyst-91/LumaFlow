#include "PixelMap.h"
#include <QJsonArray>
#include <cmath>

namespace lf {

QString pixelOrderName(PixelOrder o) {
    switch (o) { case PixelOrder::RGB: return "RGB"; case PixelOrder::RBG: return "RBG"; case PixelOrder::GRB: return "GRB"; case PixelOrder::GBR: return "GBR";
    case PixelOrder::BRG: return "BRG"; case PixelOrder::BGR: return "BGR"; case PixelOrder::RGBW: return "RGBW"; case PixelOrder::GRBW: return "GRBW"; case PixelOrder::W: return "W"; }
    return "RGB";
}
PixelOrder pixelOrderFromName(const QString& n) {
    for (auto o : {PixelOrder::RGB, PixelOrder::RBG, PixelOrder::GRB, PixelOrder::GBR, PixelOrder::BRG, PixelOrder::BGR, PixelOrder::RGBW, PixelOrder::GRBW, PixelOrder::W})
        if (pixelOrderName(o) == n.toUpper()) return o;
    return PixelOrder::RGB;
}
int pixelOrderChannels(PixelOrder o) { return o == PixelOrder::W ? 1 : (o == PixelOrder::RGBW || o == PixelOrder::GRBW) ? 4 : 3; }

QJsonObject PixelMapElement::toJson() const {
    QJsonArray pts; for (auto& p : points) pts << QJsonArray{p.x(), p.y()};
    QJsonArray cps; for (auto& p : cellPoints) cps << QJsonArray{p.x(), p.y()};
    return {{"name", name}, {"kind", int(kind)}, {"order", pixelOrderName(order)}, {"universe", universe}, {"address", address},
            {"split", int(split)}, {"points", pts}, {"radius", sampleRadius}, {"fid", fid}, {"cellPoints", cps},
            {"enabled", enabled}, {"calibration", calibration.toJson()}};
}
PixelMapElement PixelMapElement::fromJson(const QJsonObject& o) {
    PixelMapElement e;
    e.name = o["name"].toString(); e.kind = Kind(o["kind"].toInt()); e.order = pixelOrderFromName(o["order"].toString());
    e.universe = o["universe"].toInt(1); e.address = o["address"].toInt(1); e.split = UniverseSplit(o["split"].toInt());
    for (auto v : o["points"].toArray()) e.points << QPointF(v.toArray()[0].toDouble(), v.toArray()[1].toDouble());
    for (auto v : o["cellPoints"].toArray()) e.cellPoints << QPointF(v.toArray()[0].toDouble(), v.toArray()[1].toDouble());
    e.sampleRadius = o["radius"].toDouble(); e.fid = o["fid"].toInt(); e.enabled = o["enabled"].toBool(true);
    e.calibration = ColorCalibration::fromJson(o["calibration"].toObject());
    return e;
}

PixelMapElement PixelMap::makeStrip(const QString& name, QPointF a, QPointF b, int n, int universe, int address, PixelOrder order) {
    PixelMapElement e; e.name = name; e.kind = PixelMapElement::Strip; e.universe = universe; e.address = address; e.order = order;
    for (int i = 0; i < n; ++i) { const double t = n > 1 ? double(i) / (n - 1) : 0.5; e.points << a + (b - a) * t; }
    e.sampleRadius = n > 1 ? 0.5 * std::hypot(b.x() - a.x(), b.y() - a.y()) / (n - 1) : 0.01;
    return e;
}

PixelMapElement PixelMap::makeMatrix(const QString& name, QRectF r, int cols, int rows, int universe, int address,
                                     PixelOrder order, bool zigzag, StartCorner start, bool columnMajor, bool reversed) {
    PixelMapElement e; e.name = name; e.kind = PixelMapElement::Matrix; e.universe = universe; e.address = address; e.order = order;
    const bool flipX = start == StartCorner::TopRight || start == StartCorner::BottomRight;
    const bool flipY = start == StartCorner::BottomLeft || start == StartCorner::BottomRight;
    const int outer = columnMajor ? cols : rows, inner = columnMajor ? rows : cols;
    for (int o = 0; o < outer; ++o) for (int i = 0; i < inner; ++i) {
        int ii = (zigzag && (o % 2)) ? inner - 1 - i : i;
        int x = columnMajor ? o : ii, y = columnMajor ? ii : o;
        if (flipX) x = cols - 1 - x;
        if (flipY) y = rows - 1 - y;
        e.points << QPointF(r.left() + (x + 0.5) * r.width() / cols, r.top() + (y + 0.5) * r.height() / rows);
    }
    if (reversed) std::reverse(e.points.begin(), e.points.end());
    e.sampleRadius = 0.5 * std::min(r.width() / cols, r.height() / rows);
    return e;
}

QVector<PixelMapElement> PixelMap::makeFromPatch(const Patch& patch, Projection proj, double margin) {
    QVector<PixelMapElement> out;
    // Collect world positions of every cell.
    struct Cell { const FixtureInstance* f; QVector<Vec3> pts; };
    QVector<Cell> cells;
    double minA = 1e18, maxA = -1e18, minB = 1e18, maxB = -1e18;
    for (auto& f : patch.fixtures) {
        const FixtureMode* m = patch.modeOf(f);
        if (!m) continue;
        Cell c{&f, {}};
        const Mat4 rot = Mat4::rotationXYZDeg(f.rotation);
        const int n = std::max(1, m->cellCount);
        for (int i = 0; i < n; ++i) {
            const Vec3 off = i < m->cellOffsets.size() ? m->cellOffsets[i] : Vec3();
            c.pts << f.position + rot.transformPoint(off);
        }
        for (auto& p : c.pts) {
            const double a = p.x, b = proj == Projection::Front ? p.z : p.y;
            minA = std::min(minA, a); maxA = std::max(maxA, a); minB = std::min(minB, b); maxB = std::max(maxB, b);
        }
        cells << c;
    }
    const double spanA = std::max(1e-6, maxA - minA), spanB = std::max(1e-6, maxB - minB);
    const double span = std::max(spanA, spanB);
    for (auto& c : cells) {
        PixelMapElement e; e.kind = PixelMapElement::Fixture; e.fid = c.f->fid; e.name = c.f->name.isEmpty() ? QString("Fixture %1").arg(c.f->fid) : c.f->name;
        for (auto& p : c.pts) {
            const double a = p.x, b = proj == Projection::Front ? p.z : p.y;
            // keep aspect ratio; canvas y grows downward, so higher Z (or further Y) maps to smaller v
            const double u = margin + (1 - 2 * margin) * (0.5 + (a - (minA + maxA) / 2) / span);
            const double v = margin + (1 - 2 * margin) * (0.5 - (b - (minB + maxB) / 2) / span);
            e.cellPoints << QPointF(clamp01(u), clamp01(v));
        }
        e.sampleRadius = 0.01;
        out << e;
    }
    return out;
}

QVector<QPair<int, int>> PixelMap::channelSlots(const PixelMapElement& e, int i) {
    const int cpp = pixelOrderChannels(e.order);
    QVector<QPair<int, int>> out;
    if (e.split == UniverseSplit::PixelAligned) {
        const int perUniverseFirst = (kDmxSlots - (e.address - 1)) / cpp;     // pixels fitting in the first universe
        const int perUniverse = kDmxSlots / cpp;
        int u = e.universe, base;
        if (i < perUniverseFirst) base = e.address - 1 + i * cpp;
        else { const int k = i - perUniverseFirst; u += 1 + k / perUniverse; base = (k % perUniverse) * cpp; }
        for (int c = 0; c < cpp; ++c) out << qMakePair(u, base + c);
    } else {
        const int abs0 = absoluteSlot(e.universe, e.address) + i * cpp;
        for (int c = 0; c < cpp; ++c) out << qMakePair((abs0 + c) / kDmxSlots + 1, (abs0 + c) % kDmxSlots);
    }
    return out;
}

static RGB sample(const QImage& img, QPointF p, double radius, Sampling s) {
    if (img.isNull()) return {};
    const int w = img.width(), h = img.height();
    auto px = [&](int x, int y) { const QRgb c = img.pixel(std::clamp(x, 0, w - 1), std::clamp(y, 0, h - 1)); return RGB{qRed(c) / 255.0, qGreen(c) / 255.0, qBlue(c) / 255.0}; };
    const int cx = int(p.x() * w), cy = int(p.y() * h);
    if (s == Sampling::Nearest || radius <= 0) return px(cx, cy);
    const int rx = std::max(0, int(radius * w * 0.7)), ry = std::max(0, int(radius * h * 0.7));
    const int step = std::max(1, std::max(rx, ry) / 3);       // bounded cost: at most 7x7 taps
    RGB acc; int n = 0;
    for (int dy = -ry; dy <= ry; dy += step) for (int dx = -rx; dx <= rx; dx += step) { RGB c = px(cx + dx, cy + dy); acc.r += c.r; acc.g += c.g; acc.b += c.b; ++n; }
    return {acc.r / n, acc.g / n, acc.b / n};
}

static void putPixel(const PixelMapElement& e, int i, const RGB& c, double gamma, double master, QMap<int, DmxFrame>& out) {
    const auto dmxSlots = PixelMap::channelSlots(e, i);
    const ColorCalibration& cal = e.calibration;
    auto enc = [&](double v, double gain) { return uint8_t(std::lround(255.0 * std::pow(clamp01(v * gain * master * cal.maxBrightness), gamma * cal.gamma))); };
    double r = c.r, g = c.g, b = c.b, w = 0;
    if (pixelOrderChannels(e.order) == 4) { w = std::min({r, g, b}); r -= w; g -= w; b -= w; }
    uint8_t R = enc(r, cal.gainR), G = enc(g, cal.gainG), B = enc(b, cal.gainB), W = enc(w, cal.gainW);
    QVector<uint8_t> v;
    switch (e.order) {
    case PixelOrder::RGB: v = {R, G, B}; break; case PixelOrder::RBG: v = {R, B, G}; break; case PixelOrder::GRB: v = {G, R, B}; break;
    case PixelOrder::GBR: v = {G, B, R}; break; case PixelOrder::BRG: v = {B, R, G}; break; case PixelOrder::BGR: v = {B, G, R}; break;
    case PixelOrder::RGBW: v = {R, G, B, W}; break; case PixelOrder::GRBW: v = {G, R, B, W}; break;
    case PixelOrder::W: v = {enc(0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b, cal.gainW)}; break;
    }
    for (int k = 0; k < dmxSlots.size() && k < v.size(); ++k) {
        auto& f = out[dmxSlots[k].first];
        f[dmxSlots[k].second] = v[k];
    }
}

static void renderFixture(const PixelMapElement& e, const Patch& patch, const QVector<RGB>& cellColors, double master, QMap<int, DmxFrame>& out) {
    const FixtureInstance* f = patch.byFid(e.fid);
    if (!f) return;
    const FixtureMode* m = patch.modeOf(*f);
    if (!m) return;
    const Capabilities caps = analyzeCapabilities(*m);
    ColorCalibration cal = e.calibration; cal.maxBrightness *= master;
    AttributeValues vals;
    if (caps.cells > 1) {
        // Multi-cell: per-cell colour via virtual dimmer; physical master dimmer (if any) at full.
        vals.perCell.resize(caps.cells);
        Capabilities cellCaps = caps; cellCaps.dimmer = false;
        for (int i = 0; i < caps.cells && i < cellColors.size(); ++i) {
            const EmitterValues ev = ColorEngine::convert(cellColors[i], 1.0, cellCaps, cal);
            vals.perCell[i] = ColorEngine::toAttributes(ev, cellCaps).perAttribute;
        }
        vals.perAttribute[int(Attribute::Dimmer)] = caps.dimmer ? cal.maxBrightness : 1.0;
        if (caps.shutter) vals.perAttribute[int(Attribute::Shutter)] = 1.0;
    } else {
        const RGB c = cellColors.value(0);
        const double peak = std::max({c.r, c.g, c.b});
        const RGB norm = peak > 1e-6 ? RGB{c.r / peak, c.g / peak, c.b / peak} : RGB{};
        // Sampled brightness becomes intensity; normalised hue goes to the emitters.
        const EmitterValues ev = ColorEngine::convert(caps.hasColor() ? norm : c, caps.hasColor() ? peak : 1.0, caps, cal);
        vals = ColorEngine::toAttributes(ev, caps);
    }
    if (caps.pan || caps.tilt) { vals.perAttribute[int(Attribute::Pan)] = 0.5; vals.perAttribute[int(Attribute::Tilt)] = 0.5; }
    writeFixtureDmx(*f, *m, vals, out);
}

void PixelMap::render(const QImage& canvasIn, const Patch& patch, QMap<int, DmxFrame>& out) const {
    const QImage canvas = canvasIn.format() == QImage::Format_RGB32 || canvasIn.format() == QImage::Format_ARGB32 ? canvasIn : canvasIn.convertToFormat(QImage::Format_RGB32);
    for (const auto& e : elements) {
        if (!e.enabled) continue;
        if (e.kind == PixelMapElement::Fixture) {
            QVector<RGB> colors;
            for (auto& p : e.cellPoints) colors << sample(canvas, p, e.sampleRadius, sampling);
            renderFixture(e, patch, colors, masterBrightness, out);
        } else {
            for (int i = 0; i < e.points.size(); ++i) putPixel(e, i, sample(canvas, e.points[i], e.sampleRadius, sampling), globalGamma, masterBrightness, out);
        }
    }
}

void PixelMap::renderTestPattern(int pattern, double t, const Patch& patch, QMap<int, DmxFrame>& out) const {
    static const RGB ident[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
    int idx = 0;
    for (const auto& e : elements) {
        if (!e.enabled) { ++idx; continue; }
        const int n = e.kind == PixelMapElement::Fixture ? int(e.cellPoints.size()) : int(e.points.size());
        QVector<RGB> colors(n);
        for (int i = 0; i < n; ++i) {
            switch (pattern) {
            case 1: colors[i] = {1, 1, 1}; break;
            case 2: colors[i] = i == 0 ? RGB{1, 1, 1} : ident[idx % 7]; break;     // first pixel white marks start/direction
            case 3: colors[i] = (int(t * 10) % std::max(1, n)) == i ? RGB{1, 1, 1} : RGB{}; break;
            default: colors[i] = {}; break;
            }
        }
        if (e.kind == PixelMapElement::Fixture) renderFixture(e, patch, colors, 1.0, out);
        else for (int i = 0; i < n; ++i) putPixel(e, i, colors[i], 1.0, 1.0, out);
        ++idx;
    }
}

PixelMapStats PixelMap::analyze(const Patch& patch) const {
    PixelMapStats st;
    QMap<QPair<int, int>, QString> owner;
    QSet<int> universes;
    auto claim = [&](int u, int s, const QString& who) {
        universes.insert(u); ++st.channels;
        auto key = qMakePair(u, s);
        auto it = owner.find(key);
        if (it != owner.end() && *it != who) st.conflicts << ChannelConflict{u, s + 1, *it, who};
        else owner.insert(key, who);
    };
    for (const auto& e : elements) {
        if (!e.enabled) continue;
        if (e.kind == PixelMapElement::Fixture) {
            const FixtureInstance* f = patch.byFid(e.fid);
            if (!f) { st.warnings << QString("%1: fixture %2 not in patch").arg(e.name).arg(e.fid); continue; }
            const int fp = patch.footprintOf(*f);
            const int base = absoluteSlot(f->universe, f->address);
            for (int i = 0; i < fp; ++i) claim((base + i) / kDmxSlots + 1, (base + i) % kDmxSlots, e.name);
            st.pixels += e.cellPoints.size();
            const FixtureMode* m = patch.modeOf(*f);
            if (m && e.cellPoints.size() != std::max(1, m->cellCount)) st.warnings << QString("%1: %2 sample points for %3 cells").arg(e.name).arg(e.cellPoints.size()).arg(m->cellCount);
        } else {
            if (e.address < 1 || e.address > kDmxSlots) { st.warnings << e.name + ": invalid start address"; continue; }
            for (int i = 0; i < e.points.size(); ++i) for (auto& s : channelSlots(e, i)) claim(s.first, s.second, e.name);
            st.pixels += e.points.size();
        }
    }
    for (auto& c : st.conflicts) st.warnings << QString("Conflict: universe %1 channel %2 used by '%3' and '%4'").arg(c.universe).arg(c.slot).arg(c.a, c.b);
    st.universes = universes.size();
    return st;
}

QSet<int> PixelMap::universesUsed(const Patch& patch) const {
    QMap<int, DmxFrame> tmp; renderTestPattern(0, 0, patch, tmp);
    QSet<int> s; for (auto k : tmp.keys()) s.insert(k); return s;
}

QJsonObject PixelMap::toJson() const {
    QJsonArray el; for (auto& e : elements) el << e.toJson();
    return {{"elements", el}, {"master", masterBrightness}, {"gamma", globalGamma}, {"sampling", int(sampling)}};
}
PixelMap PixelMap::fromJson(const QJsonObject& o) {
    PixelMap m; for (auto v : o["elements"].toArray()) m.elements << PixelMapElement::fromJson(v.toObject());
    m.masterBrightness = o["master"].toDouble(1); m.globalGamma = o["gamma"].toDouble(2.2); m.sampling = Sampling(o["sampling"].toInt(1));
    return m;
}

} // namespace lf
