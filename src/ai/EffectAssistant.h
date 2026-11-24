#pragma once
// AI-assisted effect generation.
// The built-in generator is fully local and deterministic: it parses the
// prompt for effect families, colours, motion, audio reactivity and spatial
// ordering and produces presets, node graphs and spatial configurations.
// An optional external model (Anthropic Messages API) can be enabled in
// settings to generate GLSL; it is never required, and every generated shader
// must pass validation (isolated test compile) before the operator activates it.
#include "nodes/NodeGraph.h"
#include "spatial/Spatial.h"
#include <QObject>
#include <QJsonObject>
#include <QStringList>

class QNetworkAccessManager;

namespace lf {

struct AiSuggestion {
    enum Kind { LayerPreset, NodeEffect, ShaderEffect, Spatial } kind = LayerPreset;
    QString title;
    QString explanation;
    QString effectId;            // for LayerPreset: base effect
    QJsonObject layerValues;     // parameter values for a layer (key -> value)
    std::array<double, 3> colorA{1, 1, 1}, colorB{0, 0, 0};
    NodeGraph graph;             // NodeEffect
    QString glsl;                // ShaderEffect (body with effect())
    SpatialEffect spatial;       // Spatial
    QStringList mappings;        // human readable mapping suggestions, e.g. "audio.bass -> layer1.scale"
    bool needsValidation = true;
    QJsonObject toJson() const;
};

struct AiConfig {
    bool externalEnabled = false;
    QString endpoint = "https://api.anthropic.com/v1/messages";
    QString model = "claude-opus-5-5";
    QString apiKeyEnv = "ANTHROPIC_API_KEY";   // the key is read from the environment, never stored in projects
};

class EffectAssistant : public QObject {
    Q_OBJECT
public:
    explicit EffectAssistant(QObject* parent = nullptr);
    void setConfig(const AiConfig& c) { m_cfg = c; }
    AiConfig config() const { return m_cfg; }
    // Local generator (synchronous).
    AiSuggestion generateLocal(const QString& prompt, const QStringList& availableEffects) const;
    // External generator (asynchronous); emits externalResult or externalError.
    bool requestExternal(const QString& prompt, const QString& previousError = {});
    // Attempts simple automatic repairs on GLSL that failed to compile.
    static QString repairGlsl(const QString& glsl, const QString& compilerLog);
    static RGB parseColour(const QString& word, bool* ok = nullptr);
signals:
    void externalResult(const lf::AiSuggestion& s);
    void externalError(const QString& message);
private:
    AiConfig m_cfg;
    QNetworkAccessManager* m_net = nullptr;
};

} // namespace lf
