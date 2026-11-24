#include "Composition.h"
#include <QJsonArray>

namespace lf {

static const char* kBlend[] = {"Alpha", "Additive", "Multiply", "Screen", "Masked"};
QString blendModeName(BlendMode b) { return kBlend[int(b)]; }
QStringList blendModeNames() { QStringList l; for (auto n : kBlend) l << n; return l; }
static const char* kMask[] = {"None", "Circle", "Rectangle", "Linear gradient", "Luma of layer below"};
QString maskTypeName(MaskType m) { return kMask[int(m)]; }
QStringList maskTypeNames() { QStringList l; for (auto n : kMask) l << n; return l; }

QJsonObject LayerSource::toJson() const {
    return {{"type", int(type)}, {"effect", effectId}, {"media", mediaPath}, {"text", text}, {"mask", int(mask)},
            {"maskSize", maskSize}, {"maskFeather", maskFeather}, {"enabled", enabled}, {"solo", solo},
            {"mediaIn", mediaIn}, {"mediaOut", mediaOut}, {"mediaLoop", mediaLoop}, {"mediaReverse", mediaReverse},
            {"colorA", QJsonArray{colorA[0], colorA[1], colorA[2]}}, {"colorB", QJsonArray{colorB[0], colorB[1], colorB[2]}}};
}
LayerSource LayerSource::fromJson(const QJsonObject& o) {
    LayerSource s; s.type = SourceType(o["type"].toInt()); s.effectId = o["effect"].toString("plasma"); s.mediaPath = o["media"].toString();
    s.text = o["text"].toString("LUMAFLOW"); s.mask = MaskType(o["mask"].toInt()); s.maskSize = o["maskSize"].toDouble(0.5);
    s.maskFeather = o["maskFeather"].toDouble(0.1); s.enabled = o["enabled"].toBool(true); s.solo = o["solo"].toBool();
    s.mediaIn = o["mediaIn"].toDouble(); s.mediaOut = o["mediaOut"].toDouble(); s.mediaLoop = o["mediaLoop"].toBool(true); s.mediaReverse = o["mediaReverse"].toBool();
    auto ca = o["colorA"].toArray(), cb = o["colorB"].toArray();
    if (ca.size() == 3) s.colorA = {ca[0].toDouble(), ca[1].toDouble(), ca[2].toDouble()};
    if (cb.size() == 3) s.colorB = {cb[0].toDouble(), cb[1].toDouble(), cb[2].toDouble()};
    return s;
}

const QVector<LayerParamSpec>& layerParamSpecs() {
    static const QVector<LayerParamSpec> s = {
        {"opacity", "Opacity", 0, 1, 1, 8}, {"speed", "Speed", 0, 4, 1, 8}, {"scale", "Scale", 0.1, 4, 1, 8},
        {"rotation", "Rotation", -1, 1, 0, 8}, {"posx", "Position X", -1, 1, 0, 16}, {"posy", "Position Y", -1, 1, 0, 16},
        {"hue", "Hue shift", 0, 1, 0, 8}, {"hue2", "Colour B hue", 0, 1, 0, 8}, {"saturation", "Saturation", 0, 2, 1, 8},
        {"brightness", "Brightness", 0, 2, 1, 8}, {"contrast", "Contrast", 0, 2, 1, 8},
        {"p1", "Param 1", 0, 1, 0.5, 8}, {"p2", "Param 2", 0, 1, 0.5, 8}, {"p3", "Param 3", 0, 1, 0.5, 8}, {"p4", "Param 4", 0, 1, 0.5, 8},
        {"audio", "Audio reactivity", 0, 1, 0, 8},
    };
    return s;
}

Composition::Composition(int n) { layers.resize(std::clamp(n, 1, kMaxLayers)); defineParams(params, layerCount(), 64, 32); params.set("layer1.opacity", 1); for (int i = 1; i < layerCount(); ++i) params.set(pid(i, "opacity"), 0); }

void Composition::defineParams(ParameterRegistry& reg, int layerCount, int effectCount, int sceneCount) {
    auto def = [&](const QString& id, const QString& name, const QString& group, ParamType t, double mn, double mx, double d, int res = 8, int idx = 0) {
        ParameterDef p; p.id = id; p.name = name; p.group = group; p.type = t; p.minimum = mn; p.maximum = mx; p.defaultValue = d; p.resolution = res; p.indexCount = idx;
        reg.define(p);
    };
    def("master.intensity", "Intensity", "Master", ParamType::Continuous, 0, 1, 1, 16);
    def("master.strobe", "Strobe", "Master", ParamType::Continuous, 0, 1, 0);
    def("master.red", "Tint red", "Master", ParamType::Continuous, 0, 1, 1);
    def("master.green", "Tint green", "Master", ParamType::Continuous, 0, 1, 1);
    def("master.blue", "Tint blue", "Master", ParamType::Continuous, 0, 1, 1);
    def("master.saturation", "Saturation", "Master", ParamType::Continuous, 0, 2, 1);
    def("master.blackout", "Blackout", "Master", ParamType::Toggle, 0, 1, 0);
    def("show.scene", "Scene", "Show", ParamType::Index, 0, std::max(0, sceneCount - 1), 0, 8, sceneCount);
    def("show.transition", "Transition time", "Show", ParamType::Continuous, 0, 10, 1);
    def("show.control", "Control", "Show", ParamType::Index, 0, 6, 0, 8, 7);
    def("show.bpm", "BPM", "Show", ParamType::Continuous, 40, 240, 120);
    def("audio.reactivity", "Audio reactivity", "Audio", ParamType::Continuous, 0, 1, 0.5);
    def("audio.gain", "Audio gain", "Audio", ParamType::Continuous, 0, 4, 1);
    def("audio.smoothing", "Audio smoothing", "Audio", ParamType::Continuous, 0, 0.99, 0.6);
    for (int l = 0; l < layerCount; ++l) {
        const QString g = QString("Layer %1").arg(l + 1);
        def(pid(l, "effect"), "Effect", g, ParamType::Index, 0, std::max(0, effectCount - 1), 0, 8, effectCount);
        for (const auto& s : layerParamSpecs()) def(pid(l, s.key), s.name, g, ParamType::Continuous, s.min, s.max, s.def, s.res);
        def(pid(l, "blend"), "Blend mode", g, ParamType::Index, 0, 4, 0, 8, 5);
    }
}

void Composition::updateIndexRanges(int effectCount, int sceneCount) {
    for (int l = 0; l < layerCount(); ++l) { ParameterDef d = params.def(pid(l, "effect")); d.maximum = std::max(0, effectCount - 1); d.indexCount = effectCount; params.define(d); }
    ParameterDef s = params.def("show.scene"); s.maximum = std::max(0, sceneCount - 1); s.indexCount = sceneCount; params.define(s);
}

QJsonObject Composition::toJson() const {
    QJsonArray ls; for (auto& l : layers) ls << l.toJson();
    return {{"layers", ls}, {"values", params.valuesToJson()}};
}

void Composition::fromJson(const QJsonObject& o) {
    const QJsonArray ls = o["layers"].toArray();
    for (int i = 0; i < ls.size() && i < layers.size(); ++i) layers[i] = LayerSource::fromJson(ls[i].toObject());
    params.valuesFromJson(o["values"].toObject());
}

QJsonObject Composition::layerToJson(int l) const {
    QJsonObject vals;
    for (const auto& s : layerParamSpecs()) vals[s.key] = params.value(pid(l, s.key));
    vals["blend"] = params.value(pid(l, "blend"));
    return {{"source", layers[l].toJson()}, {"values", vals}};
}

void Composition::layerFromJson(int l, const QJsonObject& o) {
    if (l < 0 || l >= layers.size()) return;
    layers[l] = LayerSource::fromJson(o["source"].toObject());
    const QJsonObject v = o["values"].toObject();
    for (auto it = v.begin(); it != v.end(); ++it) params.set(pid(l, it.key().toLatin1().constData()), it.value().toDouble(), "preset");
}

void Composition::copyFrom(const Composition& other) {
    layers = other.layers;
    params.valuesFromJson(other.params.valuesToJson());
}

} // namespace lf
