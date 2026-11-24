#include "Spatial.h"
#include <QJsonArray>
#include <random>
#include <numeric>
#include <cmath>

namespace lf {

static const char* kOrderNames[] = {"Left to right", "Right to left", "Front to back", "Back to front", "Top to bottom", "Bottom to top",
    "Centre outward", "Outside inward", "Radial clockwise", "Radial anticlockwise", "Diagonal", "Symmetrical", "Mirrored",
    "Random", "Custom grid", "User defined"};
QString spatialOrderName(SpatialOrder o) { return kOrderNames[int(o)]; }
SpatialOrder spatialOrderFromName(const QString& s) { for (int i = 0; i < 16; ++i) if (s.compare(kOrderNames[i], Qt::CaseInsensitive) == 0) return SpatialOrder(i); return SpatialOrder::LeftToRight; }
QStringList spatialOrderNames() { QStringList l; for (auto n : kOrderNames) l << n; return l; }

static const char* kWaveNames[] = {"Sine", "Square", "Sawtooth", "Ramp", "Triangle", "Pulse", "Chase"};
QString waveformName(Waveform w) { return kWaveNames[int(w)]; }
Waveform waveformFromName(const QString& s) { for (int i = 0; i < 7; ++i) if (s.compare(kWaveNames[i], Qt::CaseInsensitive) == 0) return Waveform(i); return Waveform::Sine; }

QJsonObject SpatialEffect::toJson() const {
    QJsonArray uo; for (int f : userOrder) uo << f;
    QJsonArray ex; for (int f : excluded) ex << f;
    return {{"name", name}, {"order", spatialOrderName(order)}, {"waveform", waveformName(waveform)}, {"speed", speed},
            {"phaseSpread", phaseSpread}, {"width", width}, {"offset", offset}, {"delay", delay}, {"wings", wings},
            {"blocks", blocks}, {"groups", groups}, {"reverse", reverse}, {"bounce", bounce}, {"seed", qint64(seed)},
            {"low", low}, {"high", high}, {"colorA", QJsonArray{colorA.r, colorA.g, colorA.b}}, {"colorB", QJsonArray{colorB.r, colorB.g, colorB.b}},
            {"colorSweep", colorSweep}, {"userOrder", uo}, {"group", group}, {"excluded", ex}, {"gridColumns", gridColumns}};
}
SpatialEffect SpatialEffect::fromJson(const QJsonObject& o) {
    SpatialEffect f;
    f.name = o["name"].toString(f.name); f.order = spatialOrderFromName(o["order"].toString()); f.waveform = waveformFromName(o["waveform"].toString());
    f.speed = o["speed"].toDouble(0.5); f.phaseSpread = o["phaseSpread"].toDouble(1); f.width = o["width"].toDouble(0.5);
    f.offset = o["offset"].toDouble(); f.delay = o["delay"].toDouble(); f.wings = o["wings"].toInt(1); f.blocks = o["blocks"].toInt(1);
    f.groups = o["groups"].toInt(1); f.reverse = o["reverse"].toBool(); f.bounce = o["bounce"].toBool(); f.seed = quint32(o["seed"].toInteger(1));
    f.low = o["low"].toDouble(0); f.high = o["high"].toDouble(1);
    auto rgb = [](const QJsonValue& v, RGB d) { auto a = v.toArray(); return a.size() == 3 ? RGB{a[0].toDouble(), a[1].toDouble(), a[2].toDouble()} : d; };
    f.colorA = rgb(o["colorA"], {1, 1, 1}); f.colorB = rgb(o["colorB"], {0, 0, 0}); f.colorSweep = o["colorSweep"].toBool();
    for (auto v : o["userOrder"].toArray()) f.userOrder << v.toInt();
    f.group = o["group"].toString(); for (auto v : o["excluded"].toArray()) f.excluded.insert(v.toInt());
    f.gridColumns = o["gridColumns"].toInt(4);
    return f;
}

double SpatialEngine::waveform(Waveform w, double x, double width) {
    double p = x - std::floor(x);
    width = std::clamp(width, 0.01, 1.0);
    switch (w) {
    case Waveform::Sine: return 0.5 + 0.5 * std::sin(2 * M_PI * p - M_PI / 2);
    case Waveform::Square: return p < width ? 1.0 : 0.0;
    case Waveform::Sawtooth: return 1.0 - p;
    case Waveform::Ramp: return p;
    case Waveform::Triangle: return p < 0.5 ? p * 2 : 2 - p * 2;
    case Waveform::Pulse: return p < width ? 0.5 - 0.5 * std::cos(2 * M_PI * p / width) : 0.0;
    case Waveform::Chase: return p < width ? 1.0 - p / width : 0.0;
    }
    return 0;
}

QVector<SpatialSample> SpatialEngine::computePhases(const Patch& patch, const SpatialEffect& fx) {
    QVector<SpatialSample> s;
    Vec3 mn(1e18, 1e18, 1e18), mx(-1e18, -1e18, -1e18);
    for (auto& f : patch.fixtures) {
        if (f.excluded || fx.excluded.contains(f.fid)) continue;
        if (!fx.group.isEmpty() && !f.groups.contains(fx.group)) continue;
        const FixtureMode* m = patch.modeOf(f);
        if (!m) continue;
        const Mat4 rot = Mat4::rotationXYZDeg(f.rotation);
        const int cells = std::max(1, m->cellCount);
        for (int c = 0; c < cells; ++c) {
            const Vec3 p = f.position + rot.transformPoint(c < m->cellOffsets.size() ? m->cellOffsets[c] : Vec3());
            s << SpatialSample{f.fid, c, p, 0.0};
            mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
            mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
        }
    }
    if (s.isEmpty()) return s;
    const Vec3 centre = (mn + mx) * 0.5;
    const double maxR = std::max(1e-9, (mx - mn).length() * 0.5);
    auto norm = [](double v, double a, double b) { return b - a > 1e-9 ? (v - a) / (b - a) : 0.0; };

    // Raw scalar key per sample -> converted into rank order (so spacing is uniform).
    QVector<double> key(s.size());
    std::mt19937 rng(fx.seed);
    for (int i = 0; i < s.size(); ++i) {
        const Vec3& p = s[i].position;
        switch (fx.order) {
        case SpatialOrder::LeftToRight: key[i] = norm(p.x, mn.x, mx.x); break;
        case SpatialOrder::RightToLeft: key[i] = 1 - norm(p.x, mn.x, mx.x); break;
        case SpatialOrder::FrontToBack: key[i] = norm(p.y, mn.y, mx.y); break;      // +Y is upstage
        case SpatialOrder::BackToFront: key[i] = 1 - norm(p.y, mn.y, mx.y); break;
        case SpatialOrder::TopToBottom: key[i] = 1 - norm(p.z, mn.z, mx.z); break;
        case SpatialOrder::BottomToTop: key[i] = norm(p.z, mn.z, mx.z); break;
        case SpatialOrder::CentreOut: key[i] = (p - centre).length() / maxR; break;
        case SpatialOrder::OutsideIn: key[i] = 1 - (p - centre).length() / maxR; break;
        case SpatialOrder::RadialCW: key[i] = std::fmod(1.0 - (std::atan2(p.z - centre.z, p.x - centre.x) / (2 * M_PI) + 0.5) + 0.25, 1.0); break;
        case SpatialOrder::RadialCCW: key[i] = std::fmod((std::atan2(p.z - centre.z, p.x - centre.x) / (2 * M_PI) + 0.5) + 0.75, 1.0); break;
        case SpatialOrder::Diagonal: key[i] = 0.5 * (norm(p.x, mn.x, mx.x) + 1 - norm(p.z, mn.z, mx.z)); break;
        case SpatialOrder::Symmetrical: key[i] = std::abs(norm(p.x, mn.x, mx.x) - 0.5) * 2; break;
        case SpatialOrder::Mirrored: key[i] = 1 - std::abs(norm(p.x, mn.x, mx.x) - 0.5) * 2; break;
        case SpatialOrder::Random: key[i] = std::uniform_real_distribution<double>(0, 1)(rng); break;
        case SpatialOrder::CustomGrid: {
            const int cols = std::max(1, fx.gridColumns);
            const int col = std::min(cols - 1, int(norm(p.x, mn.x, mx.x) * cols));
            const double row = 1 - norm(p.z, mn.z, mx.z);
            key[i] = (std::round(row * 8) * cols + col) / double(9 * cols); break;
        }
        case SpatialOrder::UserDefined: {
            const int idx = fx.userOrder.indexOf(s[i].fid);
            key[i] = idx < 0 ? 1.0 + s[i].fid * 1e-6 : idx + s[i].cell * 1e-4; break;
        }
        }
    }
    // Rank with stable tie-break; ties (same key) share a phase -> symmetric effects work.
    QVector<int> idx(s.size()); std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return key[a] < key[b]; });
    QVector<int> rank(s.size());
    int r = -1; double last = -1e18;
    for (int i : idx) { if (std::abs(key[i] - last) > 1e-6) { ++r; last = key[i]; } rank[i] = r; }
    int steps = r + 1;
    // blocks: consecutive ranks grouped
    const int blocks = std::max(1, fx.blocks);
    for (auto& v : rank) v /= blocks;
    steps = (steps + blocks - 1) / blocks;
    // wings: ordering mirrored N times across the rank range
    const int wings = std::max(1, fx.wings);
    if (wings > 1) {
        const int per = std::max(1, (steps + wings - 1) / wings);
        for (auto& v : rank) { const int w = v / per, pos = v % per; v = (w % 2) ? per - 1 - pos : pos; }
        steps = per;
    }
    const int groups = std::max(1, fx.groups);
    if (groups > 1) { const int per = std::max(1, (steps + groups - 1) / groups); for (auto& v : rank) v %= per; steps = per; }
    for (int i = 0; i < s.size(); ++i) {
        double ph = steps > 1 ? double(rank[i]) / steps : 0.0;
        if (fx.reverse) ph = 1.0 - ph;
        s[i].phase = ph * fx.phaseSpread + fx.offset + rank[i] * fx.delay * fx.speed;
    }
    return s;
}

QVector<SpatialEngine::Result> SpatialEngine::evaluate(const Patch& patch, const SpatialEffect& fx, double t) {
    QVector<Result> out;
    const auto phases = computePhases(patch, fx);
    for (const auto& s : phases) {
        double x = t * fx.speed - s.phase;
        if (fx.bounce) { const double c = x - std::floor(x / 2) * 2; x = c < 1 ? c : 2 - c; }
        const double w = waveform(fx.waveform, x, fx.width);
        const double level = fx.low + (fx.high - fx.low) * w;
        RGB col;
        if (fx.colorSweep) col = ColorEngine::fromHsv(s.phase + t * fx.speed * 0.25, 1, 1);
        else col = {lerp(fx.colorB.r, fx.colorA.r, w), lerp(fx.colorB.g, fx.colorA.g, w), lerp(fx.colorB.b, fx.colorA.b, w)};
        out << Result{s.fid, s.cell, level, col, s.position};
    }
    return out;
}

void SpatialEngine::render(const Patch& patch, const SpatialEffect& fx, double t, QMap<int, DmxFrame>& out, const ColorCalibration& cal, QMap<int, double>* levelsOut) {
    const auto res = evaluate(patch, fx, t);
    QMap<int, QVector<const Result*>> byFixture;
    for (auto& r : res) byFixture[r.fid] << &r;
    for (auto it = byFixture.begin(); it != byFixture.end(); ++it) {
        const FixtureInstance* f = patch.byFid(it.key());
        const FixtureMode* m = f ? patch.modeOf(*f) : nullptr;
        if (!m) continue;
        const Capabilities caps = analyzeCapabilities(*m);
        AttributeValues vals;
        if (caps.cells > 1) {
            Capabilities cc = caps; cc.dimmer = false;
            vals.perCell.resize(caps.cells);
            for (auto* r : *it) if (r->cell < caps.cells) vals.perCell[r->cell] = ColorEngine::toAttributes(ColorEngine::convert(r->color, r->level, cc, cal), cc).perAttribute;
            vals.perAttribute[int(Attribute::Dimmer)] = 1.0;
            if (caps.shutter) vals.perAttribute[int(Attribute::Shutter)] = 1.0;
        } else {
            const Result* r = it->first();
            // colour-only -> virtual dimmer, dimmer+colour -> physical dimmer, intensity-only -> dimmer level
            RGB c = caps.hasColor() ? r->color : RGB{1, 1, 1};
            vals = ColorEngine::toAttributes(ColorEngine::convert(c, r->level, caps, cal), caps);
            if (levelsOut) (*levelsOut)[r->fid] = r->level;
        }
        if (caps.pan || caps.tilt) { vals.perAttribute[int(Attribute::Pan)] = 0.5; vals.perAttribute[int(Attribute::Tilt)] = 0.5; }
        writeFixtureDmx(*f, *m, vals, out);
    }
}

} // namespace lf
