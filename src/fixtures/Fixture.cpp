#include "Fixture.h"
#include <QJsonArray>
#include <algorithm>

namespace lf {

struct AttrInfo { Attribute a; const char* name; const char* gdtf; };
static const AttrInfo kAttrs[] = {
    {Attribute::Dimmer, "Dimmer", "Dimmer"}, {Attribute::DimmerFine, "DimmerFine", "Dimmer"},
    {Attribute::Red, "Red", "ColorAdd_R"}, {Attribute::Green, "Green", "ColorAdd_G"}, {Attribute::Blue, "Blue", "ColorAdd_B"},
    {Attribute::White, "White", "ColorAdd_W"}, {Attribute::Amber, "Amber", "ColorAdd_A"}, {Attribute::UV, "UV", "ColorAdd_UV"},
    {Attribute::Lime, "Lime", "ColorAdd_GY"}, {Attribute::Cyan, "Cyan", "ColorSub_C"}, {Attribute::Magenta, "Magenta", "ColorSub_M"},
    {Attribute::Yellow, "Yellow", "ColorSub_Y"}, {Attribute::CTO, "CTO", "CTO"}, {Attribute::ColorWheel, "ColorWheel", "Color1"},
    {Attribute::Pan, "Pan", "Pan"}, {Attribute::Tilt, "Tilt", "Tilt"}, {Attribute::PanFine, "PanFine", "Pan"}, {Attribute::TiltFine, "TiltFine", "Tilt"},
    {Attribute::Zoom, "Zoom", "Zoom"}, {Attribute::Focus, "Focus", "Focus1"}, {Attribute::Iris, "Iris", "Iris"},
    {Attribute::Shutter, "Shutter", "Shutter1"}, {Attribute::Strobe, "Strobe", "Shutter1Strobe"}, {Attribute::Gobo, "Gobo", "Gobo1"},
    {Attribute::GoboRotate, "GoboRotate", "Gobo1Pos"}, {Attribute::Prism, "Prism", "Prism1"}, {Attribute::Control, "Control", "Control1"},
    {Attribute::Effect, "Effect", "Effects1"}, {Attribute::Speed, "Speed", "Effects1Rate"},
};

QString attributeName(Attribute a) { for (auto& i : kAttrs) if (i.a == a) return i.name; return "Unknown"; }
Attribute attributeFromName(const QString& n) { for (auto& i : kAttrs) if (n.compare(i.name, Qt::CaseInsensitive) == 0) return i.a; return Attribute::Unknown; }
QString attributeToGdtf(Attribute a) { for (auto& i : kAttrs) if (i.a == a) return i.gdtf; return "NoFeature"; }

Attribute attributeFromGdtf(const QString& g) {
    static const QHash<QString, Attribute> extra = {
        {"ColorRGB_Red", Attribute::Red}, {"ColorRGB_Green", Attribute::Green}, {"ColorRGB_Blue", Attribute::Blue},
        {"ColorAdd_RY", Attribute::Amber}, {"ColorAdd_WW", Attribute::White}, {"ColorAdd_CW", Attribute::White},
        {"ColorAdd_C", Attribute::Cyan}, {"ColorAdd_M", Attribute::Magenta}, {"ColorAdd_Y", Attribute::Yellow},
        {"Shutter1StrobeRandom", Attribute::Strobe}, {"StrobeFrequency", Attribute::Strobe}, {"StrobeModeShutter", Attribute::Shutter},
        {"Gobo1WheelSpin", Attribute::GoboRotate}, {"Gobo1PosRotate", Attribute::GoboRotate}, {"Prism1Pos", Attribute::Prism},
        {"Focus", Attribute::Focus}, {"CTC", Attribute::CTO}, {"CTB", Attribute::CTO}, {"Color2", Attribute::ColorWheel},
        {"ColorMacro1", Attribute::ColorWheel}, {"Control", Attribute::Control}, {"Effects1Rate", Attribute::Speed},
    };
    for (auto& i : kAttrs) if (g == i.gdtf && i.a != Attribute::DimmerFine && i.a != Attribute::PanFine && i.a != Attribute::TiltFine) return i.a;
    auto it = extra.find(g); if (it != extra.end()) return *it;
    if (g.startsWith("Control")) return Attribute::Control;
    if (g.startsWith("Gobo") && !g.contains("Pos") && !g.contains("Spin")) return Attribute::Gobo;
    if (g.startsWith("Prism")) return Attribute::Prism;
    if (g.startsWith("Shutter") && g.contains("Strobe")) return Attribute::Strobe;
    if (g.startsWith("Shutter")) return Attribute::Shutter;
    return Attribute::Unknown;
}

QString colorModelName(ColorModel m) {
    switch (m) { case ColorModel::None: return "none"; case ColorModel::RGB: return "RGB"; case ColorModel::RGBW: return "RGBW";
    case ColorModel::RGBA: return "RGBA"; case ColorModel::RGBWA: return "RGBWA"; case ColorModel::RGBWAUV: return "RGBWAUV"; case ColorModel::CMY: return "CMY"; }
    return "none";
}

QJsonObject ChannelDef::toJson() const {
    QJsonObject o{{"attr", attributeName(attribute)}, {"offset", offset}, {"cell", cell}, {"default", defaultValue}};
    if (fineOffset >= 0) o["fine"] = fineOffset;
    if (!attributeRaw.isEmpty()) o["raw"] = attributeRaw;
    if (!name.isEmpty()) o["name"] = name;
    return o;
}
ChannelDef ChannelDef::fromJson(const QJsonObject& o) {
    ChannelDef c; c.attribute = attributeFromName(o["attr"].toString()); c.offset = o["offset"].toInt();
    c.fineOffset = o["fine"].toInt(-1); c.cell = o["cell"].toInt(); c.defaultValue = o["default"].toInt();
    c.attributeRaw = o["raw"].toString(); c.name = o["name"].toString(); return c;
}

int FixtureMode::footprint() const {
    int fp = 0;
    for (auto& c : channels) { fp = std::max(fp, c.offset + 1); if (c.fineOffset >= 0) fp = std::max(fp, c.fineOffset + 1); }
    return fp;
}
QJsonObject FixtureMode::toJson() const {
    QJsonArray ch; for (auto& c : channels) ch << c.toJson();
    QJsonArray off; for (auto& v : cellOffsets) off << v.toJson();
    return {{"name", name}, {"channels", ch}, {"cells", cellCount}, {"cellOffsets", off}};
}
FixtureMode FixtureMode::fromJson(const QJsonObject& o) {
    FixtureMode m; m.name = o["name"].toString(); m.cellCount = o["cells"].toInt(1);
    for (auto v : o["channels"].toArray()) m.channels << ChannelDef::fromJson(v.toObject());
    for (auto v : o["cellOffsets"].toArray()) m.cellOffsets << Vec3::fromJson(v.toArray());
    return m;
}

QStringList Capabilities::describe() const {
    QStringList s;
    if (dimmer) s << "Dimmer"; if (color != ColorModel::None) s << colorModelName(color);
    if (pan) s << "Pan"; if (tilt) s << "Tilt"; if (zoom) s << "Zoom"; if (focus) s << "Focus"; if (iris) s << "Iris";
    if (shutter) s << "Shutter"; if (strobe) s << "Strobe"; if (gobo) s << "Gobo"; if (prism) s << "Prism";
    if (colorWheel) s << "ColorWheel"; if (cto) s << "CTO"; if (cells > 1) s << QString("%1 cells").arg(cells);
    if (needsVirtualDimmer()) s << "(virtual dimmer)";
    return s;
}

Capabilities analyzeCapabilities(const FixtureMode& m) {
    Capabilities c; QSet<Attribute> attrs; QSet<int> cells;
    for (auto& ch : m.channels) { attrs.insert(ch.attribute); cells.insert(ch.cell); }
    c.dimmer = attrs.contains(Attribute::Dimmer); c.pan = attrs.contains(Attribute::Pan); c.tilt = attrs.contains(Attribute::Tilt);
    c.zoom = attrs.contains(Attribute::Zoom); c.focus = attrs.contains(Attribute::Focus); c.iris = attrs.contains(Attribute::Iris);
    c.shutter = attrs.contains(Attribute::Shutter); c.strobe = attrs.contains(Attribute::Strobe); c.gobo = attrs.contains(Attribute::Gobo);
    c.prism = attrs.contains(Attribute::Prism); c.colorWheel = attrs.contains(Attribute::ColorWheel); c.cto = attrs.contains(Attribute::CTO);
    const bool rgb = attrs.contains(Attribute::Red) && attrs.contains(Attribute::Green) && attrs.contains(Attribute::Blue);
    const bool w = attrs.contains(Attribute::White), a = attrs.contains(Attribute::Amber), uv = attrs.contains(Attribute::UV);
    if (rgb) c.color = (w && a && uv) ? ColorModel::RGBWAUV : (w && a) ? ColorModel::RGBWA : w ? ColorModel::RGBW : a ? ColorModel::RGBA : ColorModel::RGB;
    else if (attrs.contains(Attribute::Cyan) && attrs.contains(Attribute::Magenta) && attrs.contains(Attribute::Yellow)) c.color = ColorModel::CMY;
    c.cells = std::max<int>({1, m.cellCount, int(cells.size())});
    return c;
}

const FixtureMode* FixtureType::mode(const QString& n) const {
    for (auto& m : modes) if (m.name == n) return &m;
    return modes.isEmpty() ? nullptr : (n.isEmpty() ? &modes.first() : nullptr);
}
QJsonObject FixtureType::toJson() const {
    QJsonArray ms; for (auto& m : modes) ms << m.toJson();
    return {{"id", id}, {"manufacturer", manufacturer}, {"name", name}, {"source", source}, {"modes", ms}};
}
FixtureType FixtureType::fromJson(const QJsonObject& o) {
    FixtureType t; t.id = o["id"].toString(); t.manufacturer = o["manufacturer"].toString(); t.name = o["name"].toString();
    t.source = o["source"].toString(); for (auto v : o["modes"].toArray()) t.modes << FixtureMode::fromJson(v.toObject());
    return t;
}

QJsonObject FixtureInstance::toJson() const {
    return {{"fid", fid}, {"uuid", uuid}, {"name", name}, {"type", typeId}, {"mode", modeName}, {"universe", universe},
            {"address", address}, {"position", position.toJson()}, {"rotation", rotation.toJson()},
            {"groups", QJsonArray::fromStringList(groups)}, {"excluded", excluded}, {"layer", layer}};
}
FixtureInstance FixtureInstance::fromJson(const QJsonObject& o) {
    FixtureInstance f; f.fid = o["fid"].toInt(); f.uuid = o["uuid"].toString(); f.name = o["name"].toString();
    f.typeId = o["type"].toString(); f.modeName = o["mode"].toString(); f.universe = o["universe"].toInt(1);
    f.address = o["address"].toInt(1); f.position = Vec3::fromJson(o["position"].toArray());
    f.rotation = Vec3::fromJson(o["rotation"].toArray());
    for (auto v : o["groups"].toArray()) f.groups << v.toString();
    f.excluded = o["excluded"].toBool(); f.layer = o["layer"].toString();
    return f;
}

// ---------------------------------------------------------------- built-in library
static ChannelDef ch(Attribute a, int off, int cell = 0, int fine = -1, int def = 0) { ChannelDef c; c.attribute = a; c.offset = off; c.cell = cell; c.fineOffset = fine; c.defaultValue = def; return c; }

static FixtureType simple(const QString& name, const QString& modeName, std::initializer_list<Attribute> attrs) {
    FixtureType t; t.manufacturer = "Generic"; t.name = name; t.id = "Generic@" + name; t.source = "builtin";
    FixtureMode m; m.name = modeName; int off = 0;
    for (auto a : attrs) m.channels << ch(a, off++);
    t.modes << m; return t;
}

static FixtureType pixelBar(const QString& name, int cells, double length, bool rgbw, bool withDimmer) {
    FixtureType t; t.manufacturer = "Generic"; t.name = name; t.id = "Generic@" + name; t.source = "builtin";
    FixtureMode m; m.name = QString("%1px %2%3").arg(cells).arg(rgbw ? "RGBW" : "RGB").arg(withDimmer ? "+Dim" : "");
    m.cellCount = cells; int off = 0;
    if (withDimmer) m.channels << ch(Attribute::Dimmer, off++);
    for (int c = 0; c < cells; ++c) {
        m.channels << ch(Attribute::Red, off++, c) << ch(Attribute::Green, off++, c) << ch(Attribute::Blue, off++, c);
        if (rgbw) m.channels << ch(Attribute::White, off++, c);
        m.cellOffsets << Vec3(cells > 1 ? -length / 2 + length * (c + 0.5) / cells : 0, 0, 0);
    }
    t.modes << m; return t;
}

FixtureLibrary& FixtureLibrary::builtin() {
    static FixtureLibrary lib = [] {
        FixtureLibrary l;
        l.add(simple("Dimmer", "1ch", {Attribute::Dimmer}));
        l.add(simple("RGB Par", "3ch", {Attribute::Red, Attribute::Green, Attribute::Blue}));
        l.add(simple("RGBW Par", "4ch", {Attribute::Red, Attribute::Green, Attribute::Blue, Attribute::White}));
        l.add(simple("RGBW Par Dim", "6ch", {Attribute::Dimmer, Attribute::Red, Attribute::Green, Attribute::Blue, Attribute::White, Attribute::Strobe}));
        l.add(simple("RGBWAUV Par", "6ch", {Attribute::Red, Attribute::Green, Attribute::Blue, Attribute::White, Attribute::Amber, Attribute::UV}));
        l.add(simple("Strobe", "2ch", {Attribute::Dimmer, Attribute::Strobe}));
        l.add(pixelBar("LED Bar 8", 8, 1.0, false, false));
        l.add(pixelBar("LED Batten 16 RGBW", 16, 1.2, true, true));
        l.add(pixelBar("Pixel Strip 170", 170, 5.0, false, false));
        l.add(pixelBar("Pixel Tube 32", 32, 1.0, false, false));
        {   // moving head spot, 16-bit pan/tilt
            FixtureType t; t.manufacturer = "Generic"; t.name = "Moving Spot"; t.id = "Generic@Moving Spot"; t.source = "builtin";
            FixtureMode m; m.name = "16ch";
            m.channels << ch(Attribute::Pan, 0, 0, 1, 128) << ch(Attribute::Tilt, 2, 0, 3, 128) << ch(Attribute::Dimmer, 4, 0, 5)
                       << ch(Attribute::Shutter, 6, 0, -1, 255) << ch(Attribute::ColorWheel, 7) << ch(Attribute::Gobo, 8)
                       << ch(Attribute::GoboRotate, 9) << ch(Attribute::Prism, 10) << ch(Attribute::Focus, 11, 0, -1, 128)
                       << ch(Attribute::Zoom, 12, 0, -1, 128) << ch(Attribute::Iris, 13) << ch(Attribute::Control, 14) << ch(Attribute::Speed, 15);
            t.modes << m; l.add(t);
        }
        {   // moving wash RGBW + zoom
            FixtureType t; t.manufacturer = "Generic"; t.name = "Moving Wash"; t.id = "Generic@Moving Wash"; t.source = "builtin";
            FixtureMode m; m.name = "14ch";
            m.channels << ch(Attribute::Pan, 0, 0, 1, 128) << ch(Attribute::Tilt, 2, 0, 3, 128) << ch(Attribute::Dimmer, 4, 0, 5)
                       << ch(Attribute::Strobe, 6) << ch(Attribute::Red, 7) << ch(Attribute::Green, 8) << ch(Attribute::Blue, 9)
                       << ch(Attribute::White, 10) << ch(Attribute::CTO, 11) << ch(Attribute::Zoom, 12) << ch(Attribute::Control, 13);
            t.modes << m; l.add(t);
        }
        {   // 8x8 LED matrix panel
            FixtureType t; t.manufacturer = "Generic"; t.name = "Matrix 8x8"; t.id = "Generic@Matrix 8x8"; t.source = "builtin";
            FixtureMode m; m.name = "64px RGB"; m.cellCount = 64; int off = 0;
            for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
                const int c = y * 8 + x;
                m.channels << ch(Attribute::Red, off++, c) << ch(Attribute::Green, off++, c) << ch(Attribute::Blue, off++, c);
                m.cellOffsets << Vec3(-0.35 + x * 0.1, 0, 0.35 - y * 0.1);
            }
            t.modes << m; l.add(t);
        }
        return l;
    }();
    return lib;
}

void FixtureLibrary::add(const FixtureType& t) { m_types.insert(t.id, t); }
const FixtureType* FixtureLibrary::find(const QString& id) const { auto it = m_types.find(id); return it == m_types.end() ? nullptr : &*it; }
QJsonArray FixtureLibrary::toJson() const { QJsonArray a; for (auto& t : m_types) a << t.toJson(); return a; }

// ---------------------------------------------------------------- patch
const FixtureType* Patch::typeOf(const FixtureInstance& f) const {
    if (auto* t = types.find(f.typeId)) return t;
    return FixtureLibrary::builtin().find(f.typeId);
}
const FixtureMode* Patch::modeOf(const FixtureInstance& f) const { auto* t = typeOf(f); return t ? t->mode(f.modeName) : nullptr; }
int Patch::footprintOf(const FixtureInstance& f) const { auto* m = modeOf(f); return m ? m->footprint() : 0; }
FixtureInstance* Patch::byFid(int fid) { for (auto& f : fixtures) if (f.fid == fid) return &f; return nullptr; }
const FixtureInstance* Patch::byFid(int fid) const { for (auto& f : fixtures) if (f.fid == fid) return &f; return nullptr; }

QVector<PatchIssue> Patch::validate(bool allowSpan) const {
    QVector<PatchIssue> issues;
    struct Range { int start, end, fid; };
    QVector<Range> ranges;
    for (auto& f : fixtures) {
        const FixtureMode* m = modeOf(f);
        if (!m) { issues << PatchIssue{PatchIssue::MissingType, f.fid, 0, QString("Fixture %1: unknown type/mode '%2' / '%3'").arg(f.fid).arg(f.typeId, f.modeName)}; continue; }
        if (f.address < 1 || f.address > kDmxSlots || f.universe < 1) { issues << PatchIssue{PatchIssue::InvalidAddress, f.fid, 0, QString("Fixture %1: invalid address %2.%3").arg(f.fid).arg(f.universe).arg(f.address)}; continue; }
        const int fp = m->footprint();
        if (!allowSpan && f.address + fp - 1 > kDmxSlots)
            issues << PatchIssue{PatchIssue::CrossesUniverse, f.fid, 0, QString("Fixture %1 (%2 ch at %3.%4) crosses the universe boundary").arg(f.fid).arg(fp).arg(f.universe).arg(f.address)};
        const int s = absoluteSlot(f.universe, f.address);
        ranges << Range{s, s + fp - 1, f.fid};
    }
    std::sort(ranges.begin(), ranges.end(), [](auto& a, auto& b) { return a.start < b.start; });
    for (int i = 0; i < ranges.size(); ++i)
        for (int j = i + 1; j < ranges.size() && ranges[j].start <= ranges[i].end; ++j)
            issues << PatchIssue{PatchIssue::Overlap, ranges[i].fid, ranges[j].fid, QString("Fixtures %1 and %2 overlap in DMX address space").arg(ranges[i].fid).arg(ranges[j].fid)};
    return issues;
}

int Patch::nextFreeAddress(int universe, int fp) const {
    QVector<bool> used(kDmxSlots, false);
    for (auto& f : fixtures) {
        const int ffp = footprintOf(f);
        for (int i = 0; i < ffp; ++i) { const int abs = absoluteSlot(f.universe, f.address) + i; if (abs / kDmxSlots == universe - 1) used[abs % kDmxSlots] = true; }
    }
    for (int a = 0; a + fp <= kDmxSlots; ++a) {
        bool ok = true; for (int i = 0; i < fp && ok; ++i) ok = !used[a + i];
        if (ok) return a + 1;
    }
    return -1;
}

QJsonObject Patch::toJson() const {
    QJsonArray fx; for (auto& f : fixtures) fx << f.toJson();
    return {{"fixtures", fx}, {"types", types.toJson()}};
}
Patch Patch::fromJson(const QJsonObject& o) {
    Patch p;
    for (auto v : o["types"].toArray()) p.types.add(FixtureType::fromJson(v.toObject()));
    for (auto v : o["fixtures"].toArray()) p.fixtures << FixtureInstance::fromJson(v.toObject());
    return p;
}

double AttributeValues::get(Attribute a, int cell, double fallback) const {
    if (cell >= 0 && cell < perCell.size()) { auto it = perCell[cell].find(int(a)); if (it != perCell[cell].end()) return *it; }
    auto it = perAttribute.find(int(a));
    return it == perAttribute.end() ? fallback : *it;
}

void writeFixtureDmx(const FixtureInstance& f, const FixtureMode& mode, const AttributeValues& v, QMap<int, DmxFrame>& universes) {
    const int base = absoluteSlot(f.universe, f.address);
    auto put = [&](int offset, uint8_t value) {
        const int abs = base + offset;
        const int u = abs / kDmxSlots + 1, s = abs % kDmxSlots;
        if (!universes.contains(u)) universes[u].fill(0);
        universes[u][s] = value;
    };
    for (const auto& c : mode.channels) {
        double def = c.defaultValue / 255.0;
        const double n = clamp01(v.get(c.attribute, c.cell, def));
        if (c.fineOffset >= 0) {
            const int v16 = int(std::lround(n * 65535.0));
            put(c.offset, uint8_t(v16 >> 8)); put(c.fineOffset, uint8_t(v16 & 0xFF));
        } else put(c.offset, uint8_t(std::lround(n * 255.0)));
    }
}

} // namespace lf
