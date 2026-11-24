#pragma once
// Multi-layer composition model, scenes and presets.
// Layer parameters live in a ParameterRegistry under ids "layerN.<param>".
#include "core/Parameters.h"
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <array>

namespace lf {

enum class BlendMode { Alpha = 0, Additive, Multiply, Screen, Masked };
QString blendModeName(BlendMode b);
QStringList blendModeNames();

enum class SourceType { Effect = 0, Media, Image };
enum class MaskType { None = 0, Circle, Rectangle, LinearGradient, LayerBelowLuma };
QString maskTypeName(MaskType m);
QStringList maskTypeNames();

struct LayerSource {
    SourceType type = SourceType::Effect;
    QString effectId = "plasma";
    QString mediaPath;           // media library path for Media/Image
    QString text = "LUMAFLOW";   // for the Text effect
    MaskType mask = MaskType::None;
    double maskSize = 0.5, maskFeather = 0.1;
    bool enabled = true;
    bool solo = false;
    double mediaIn = 0.0, mediaOut = 0.0; // in/out points seconds (0 = full)
    bool mediaLoop = true;
    bool mediaReverse = false;
    std::array<double, 3> colorA{0.1, 0.5, 1.0}, colorB{0.7, 0.1, 1.0};   // layer colours (effect defaults applied on effect change)
    QJsonObject toJson() const;
    static LayerSource fromJson(const QJsonObject& o);
};

constexpr int kMaxLayers = 8;

struct LayerParamSpec { const char* key; const char* name; double min, max, def; int res; };
const QVector<LayerParamSpec>& layerParamSpecs();

class Composition {
public:
    explicit Composition(int layers = 4);
    ParameterRegistry params;
    QVector<LayerSource> layers;
    int layerCount() const { return int(layers.size()); }
    static QString pid(int layer, const char* key) { return QString("layer%1.%2").arg(layer + 1).arg(key); }
    double lp(int layer, const char* key) const { return params.value(pid(layer, key)); }
    static void defineParams(ParameterRegistry& reg, int layerCount, int effectCount, int sceneCount);
    void updateIndexRanges(int effectCount, int sceneCount);

    QJsonObject toJson() const;                  // full snapshot (sources + values)
    void fromJson(const QJsonObject& o);         // restores values (and sources)
    QJsonObject layerToJson(int layer) const;    // preset snapshot of one layer
    void layerFromJson(int layer, const QJsonObject& o);
    void copyFrom(const Composition& other);
};

struct Scene {
    QString id;
    QString name;
    QJsonObject state;       // Composition::toJson()
    double transition = -1;  // seconds; <0 = use show.transition
    QJsonObject toJson() const { return {{"id", id}, {"name", name}, {"state", state}, {"transition", transition}}; }
    static Scene fromJson(const QJsonObject& o) { return {o["id"].toString(), o["name"].toString(), o["state"].toObject(), o["transition"].toDouble(-1)}; }
};

struct Preset {
    QString id;
    QString name;
    QString category = "User";
    QJsonObject layer;       // Composition::layerToJson()
    QJsonObject toJson() const { return {{"id", id}, {"name", name}, {"category", category}, {"layer", layer}, {"format", "lfpreset"}, {"version", 1}}; }
    static Preset fromJson(const QJsonObject& o) { return {o["id"].toString(), o["name"].toString(), o["category"].toString("User"), o["layer"].toObject()}; }
};

} // namespace lf
