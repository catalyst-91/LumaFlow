#pragma once
// Immutable per-frame snapshot handed from the engine to the render thread.
// The render thread never touches engine objects directly, so UI, network and
// storage activity cannot block or race with rendering.
#include "layers/Composition.h"
#include "fixtures/ColorEngine.h"
#include <QImage>
#include <QVector>
#include <array>

namespace lf {

struct LayerSnapshot {
    bool visible = false;
    QString programKey;          // effect id (shader cache key)
    QString glsl;                // effect body; empty -> use cached program for key
    quint64 glslHash = 0;
    BlendMode blend = BlendMode::Alpha;
    MaskType mask = MaskType::None;
    float maskSize = 0.5f, maskFeather = 0.1f;
    float opacity = 1;
    float time = 0;              // effect time (already speed-integrated)
    float scale = 1, rotation = 0, posX = 0, posY = 0;
    float hue = 0, hue2 = 0, saturation = 1, brightness = 1, contrast = 1;
    std::array<float, 4> p{0.5f, 0.5f, 0.5f, 0.5f};
    std::array<float, 3> colorA{1, 1, 1}, colorB{0, 0, 1};
    float audioAmount = 0;
    QImage texture;              // media frame / text / logo
    bool feedback = false;       // effect samples its previous frame
};

struct CompositionSnapshot {
    QVector<LayerSnapshot> layers;
};

struct SafetyLimits {
    double maxStrobeHz = 3.0;    // photosensitive limiter (WCAG 2.3.1 general flash threshold)
    double maxBrightness = 1.0;  // global output limiter
    bool limiterEnabled = true;
};

struct OutputCalibration {
    double gamma = 1.0;
    double gainR = 1, gainG = 1, gainB = 1;
    double brightness = 1.0;
    double whitePointK = 6500;   // white point adjustment (6500 = neutral)
    QJsonObject toJson() const { return {{"gamma", gamma}, {"gainR", gainR}, {"gainG", gainG}, {"gainB", gainB}, {"brightness", brightness}, {"whitePointK", whitePointK}}; }
    static OutputCalibration fromJson(const QJsonObject& o) { OutputCalibration c; c.gamma = o["gamma"].toDouble(1); c.gainR = o["gainR"].toDouble(1); c.gainG = o["gainG"].toDouble(1); c.gainB = o["gainB"].toDouble(1); c.brightness = o["brightness"].toDouble(1); c.whitePointK = o["whitePointK"].toDouble(6500); return c; }
};

struct MasterSnapshot {
    float intensity = 1, strobe = 0, saturation = 1;
    std::array<float, 3> tint{1, 1, 1};
    bool blackout = false;
    float time = 0;
};

struct RenderInput {
    int width = 1280, height = 720;
    CompositionSnapshot program, preview, outgoing;
    float transition = 1.0f;     // 0 = outgoing, 1 = program
    bool renderPreview = true;
    MasterSnapshot master;
    std::array<float, 4> audio{0, 0, 0, 0};
    float beat = 0;
    std::array<float, 32> spectrum{};
    std::array<float, 8> ext{};
    float timecode = 0;
    SafetyLimits safety;
    OutputCalibration calibration;
    double qualityScale = 1.0;   // render resolution scale for layers (adaptive quality)
    int testPattern = -1;        // >= 0 overrides program with a test pattern
};

struct RenderStats {
    double frameMs = 0, cpuMs = 0, gpuMs = 0, readbackMs = 0, fps = 0;
    double qualityScale = 1.0;
    quint64 frames = 0;
    qint64 gpuMemoryKB = -1;     // -1 unknown
    qint64 textureMemoryKB = 0;  // allocated by LUMAFLOW
    int programsCached = 0;
    QStringList shaderErrors;
    QString glRenderer, glVersion;
};

struct RenderOutput {
    QImage program;
    QImage preview;
    RenderStats stats;
    double time = 0;
};

} // namespace lf
