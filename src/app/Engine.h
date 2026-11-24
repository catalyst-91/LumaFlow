#pragma once
// LUMAFLOW application engine: owns every subsystem and connects them.
// Threads: main (UI + control tick), render, network I/O, remote server,
// sync, decoder and audio threads. The render thread pulls immutable
// snapshots via buildRenderInput(); control state is guarded by m_state.
#include "core/Parameters.h"
#include "layers/Composition.h"
#include "effects/EffectLibrary.h"
#include "nodes/NodeGraph.h"
#include "fixtures/Fixture.h"
#include "pixelmapping/PixelMap.h"
#include "spatial/Spatial.h"
#include "console/Personality.h"
#include "console/Mapping.h"
#include "protocols/NetworkIO.h"
#include "protocols/Midi.h"
#include "protocols/Timecode.h"
#include "timeline/Timeline.h"
#include "audio/AudioAnalyzer.h"
#include "media/MediaPlayer.h"
#include "renderer/RenderThread.h"
#include "output/Output.h"
#include "output/StreamSinks.h"
#include "remote/RemoteServer.h"
#include "sync/SyncService.h"
#include "plugins/PluginHost.h"
#include "ai/EffectAssistant.h"
#include "recovery/Recovery.h"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QRecursiveMutex>
#include <QJsonObject>
#include <memory>

namespace lf {

struct EngineOptions {
    bool safeMode = false;
    bool headless = false;            // CLI / tests: no remote/sync autostart
    bool startRenderer = true;
    bool startNetwork = true;
    QString dataDir;                  // default: QStandardPaths::AppDataLocation
    int canvasWidth = 1280, canvasHeight = 720;
    double fps = 60.0;
};

struct ConsoleLink {
    QString name = "grandMA3";        // "grandMA3" | "MagicQ" | "generic"
    bool enabled = false;
    int universe = 1, address = 1;
    PersonalityMode mode = PersonalityMode::Standard32;
    QString oscHost; int oscPort = 0; // feedback target
    QJsonObject toJson() const;
    static ConsoleLink fromJson(const QJsonObject& o);
};

struct SpatialSlot { SpatialEffect fx; bool enabled = false; double level = 1.0; };

struct AudioSettings { QString device = "test:120"; bool enabled = false; double gain = 1.0, gate = 0.01, smoothing = 0.6, sensitivity = 1.4; };

struct EngineStatus {
    RenderStats render;
    NetworkStats network;
    AudioFeatures audio;
    SyncStatus sync;
    double transition = 1.0;
    int activeScene = -1;
    int activeCue = -1;
    QString timecode;
    bool blackout = false;
    int faults = 0;
    bool recording = false;
};

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(const EngineOptions& opt = {}, QObject* parent = nullptr);
    ~Engine() override;
    bool start();
    void shutdown();
    const EngineOptions& options() const { return m_opt; }

    // ---- subsystems
    EffectLibrary& effects() { return m_effects; }
    Composition& program() { return m_program; }
    Composition& preview() { return m_preview; }
    ParameterRegistry& params() { return m_program.params; }
    MappingEngine& mapping() { return *m_mapping; }
    ProfileManager& profiles() { return m_profiles; }
    NetworkIO& network() { return *m_net; }
    Patch& patch() { return m_patch; }
    PixelMap& pixelMap() { return m_pixelMap; }
    QVector<SpatialSlot>& spatial() { return m_spatial; }
    Timeline& timeline() { return m_timeline; }
    CueList& cueList() { return m_cues; }
    QVector<Scene>& scenes() { return m_scenes; }
    QVector<Preset>& presets() { return m_presets; }
    QVector<ConsoleLink>& consoles() { return m_consoles; }
    PluginHost& plugins() { return m_plugins; }
    EffectAssistant& assistant() { return m_assistant; }
    RemoteServer& remote() { return m_remote; }
    SyncService& sync() { return m_sync; }
    AudioAnalyzer& audio() { return m_audio; }
    MediaLibrary& mediaLibrary() { return m_mediaLib; }
    SessionGuard& session() { return *m_session; }
    VideoOutputConfig& videoOutputs() { return m_videoOut; }
    OutputCalibration& calibration() { return m_calibration; }
    SafetyLimits& safety() { return m_safety; }
    RenderThread* renderThread() { return m_render.get(); }
    StreamOutputManager& streams() { return *m_streams; }
    void applyStreamConfigs(const QVector<StreamConfig>& c);
    QRecursiveMutex& stateMutex() { return m_state; }
    midi::Backend* midiBackend() { return m_midi.get(); }

    // ---- layers & effects
    void setLayerEffect(int layer, const QString& effectId, bool previewSide = false);
    void setLayerMedia(int layer, const QString& path, bool previewSide = false);
    void setLayerText(int layer, const QString& text);
    void setLayerSource(int layer, const LayerSource& src, bool previewSide = false);
    void applyEffectDefaults(int layer, bool previewSide = false);
    void setLayerColors(int layer, const std::array<double, 3>& a, const std::array<double, 3>& b);
    std::array<double, 3> layerColorA(int layer) const;
    std::array<double, 3> layerColorB(int layer) const;
    bool addCustomEffect(const EffectDef& e, QString* err);         // validates (isolated compile) before adding
    bool compileNodeEffect(const NodeGraph& g, const QString& id, QString* err, EffectDef* out = nullptr);
    bool validateShader(const QString& glsl, QString* log);

    // ---- show control
    int storeScene(const QString& name, int index = -1);            // snapshot of program
    bool recallScene(int index, double transitionSec = -1);
    void take(double transitionSec = -1);                           // preview -> program
    void copyProgramToPreview();
    int storePreset(int layer, const QString& name);
    bool recallPreset(int presetIndex, int layer, bool previewSide = false);
    void cueGo(); void cueBack(); bool cueGoto(double number);
    void tapTempo();
    void setBlackout(bool on);
    bool blackout() const { return m_blackout; }
    void setTestPattern(int pattern) { m_testPattern = pattern; }
    int testPattern() const { return m_testPattern; }
    void setPixelTestPattern(int p) { m_pixelTest = p; }
    int activeScene() const { return m_activeScene; }
    double transitionProgress() const;
    void executeAction(const QString& action, double value, ControlSource src);

    // ---- console / personality
    PersonalityProfile generatePersonality(PersonalityMode mode, const QStringList& custom = {}) const;
    ProfileDiff checkPersonalityUpdates(PersonalityMode mode);     // proposes, needs approval
    bool exportConsoleFiles(const QString& dir, QString* report);

    // ---- outputs
    void applyNetworkConfig(const NetworkConfig& c);
    bool startRecording(const QString& path, QString* err);
    void stopRecording();
    bool recording() const { return m_recorder.recording(); }
    QImage lastProgramFrame() const;
    QImage lastPreviewFrame() const;
    QMap<int, DmxFrame> lastDmxOutput() const;

    // ---- audio / timecode / sync / remote
    void applyAudioSettings(const AudioSettings& a);
    AudioSettings audioSettings() const { return m_audioSettings; }
    void setTimecodeSource(const QString& src);                    // "internal", "mtc", "ltc", "sync"
    QString timecodeSource() const { return m_tcSource; }
    bool startRemote(quint16 port);
    bool startSync(SyncRole role, quint16 port = 7700);

    // ---- project
    QJsonObject toProjectJson() const;
    bool fromProjectJson(const QJsonObject& o, QStringList* warnings = nullptr);
    bool saveProject(const QString& path, QString* err = nullptr);
    bool loadProject(const QString& path, QStringList* warnings = nullptr, QString* err = nullptr);
    bool newProject();
    QString projectPath() const { return m_projectPath; }
    bool dirty() const;
    void markClean();
    bool autosaveNow();
    void createDemoShow();                                         // example fixtures, pixel map, scenes, cues

    // ---- status
    EngineStatus status() const;
    double showTime() const { return m_offline ? m_virtualTime : m_showClock.elapsed() / 1000.0; }
    // Offline (non-real-time) rendering: a virtual clock advanced explicitly.
    void setOfflineClock(bool on) { m_offline = on; m_virtualTime = 0; }
    void stepOffline(double dt) { m_virtualTime += dt; controlTick(); }
    QJsonObject remoteState() const;

    // render-thread callbacks
    RenderInput buildRenderInput(double dt);
    void onRenderOutput(const RenderOutput& out);

signals:
    void frameReady();
    void parameterChanged(const QString& id, double value, const QString& source);
    void showStateChanged();                       // scenes/cues/presets/layers changed
    void message(const QString& text);
    void projectLoaded();

private:
    void controlTick();
    void onParam(const QString& id, double value, const QString& source);
    void handleOsc(const osc::Message& m, const QString& sender);
    void handleDmxInput(int universe);
    void pollMidi();
    void updateIndexRanges();
    LayerSnapshot snapshotLayer(Composition& c, int i, double dt, std::array<double, 2>& layerClock, QHash<QString, std::shared_ptr<MediaPlayer>>& players, bool advance);
    HttpResponse handleRemote(const HttpRequest& r);
    void sendFeedback(const ControlMapping& m, double n);
    QImage textImage(const QString& text);
    void sendStatusOsc(const QString& path, const QVariant& v);

    EngineOptions m_opt;
    mutable QRecursiveMutex m_state;
    EffectLibrary m_effects;
    Composition m_program{6}, m_preview{6}, m_outgoing{6};
    QVector<Scene> m_scenes;
    QVector<Preset> m_presets;
    CueList m_cues;
    Timeline m_timeline;
    Patch m_patch;
    PixelMap m_pixelMap;
    QVector<SpatialSlot> m_spatial;
    std::unique_ptr<MappingEngine> m_mapping;
    ProfileManager m_profiles;
    QVector<ConsoleLink> m_consoles;
    std::unique_ptr<NetworkIO> m_net;
    std::unique_ptr<midi::Backend> m_midi;
    midi::Parser m_midiParser;
    tc::MtcDecoder m_mtc;
    std::unique_ptr<tc::LtcDecoder> m_ltc;
    QString m_tcSource = "internal";
    double m_tcSeconds = 0;
    AudioAnalyzer m_audio;
    std::unique_ptr<AudioInput> m_audioIn;
    AudioSettings m_audioSettings;
    MediaLibrary m_mediaLib;
    PluginHost m_plugins;
    EffectAssistant m_assistant;
    RemoteServer m_remote;
    SyncService m_sync;
    std::unique_ptr<SessionGuard> m_session;
    std::unique_ptr<RenderThread> m_render;
    Recorder m_recorder;
    std::unique_ptr<StreamOutputManager> m_streams;
    VideoOutputConfig m_videoOut;
    OutputCalibration m_calibration;
    SafetyLimits m_safety;
    QTimer m_tick;
    QElapsedTimer m_showClock;
    double m_lastTick = 0;
    // transitions
    double m_transStart = -1, m_transDur = 0;
    int m_activeScene = -1;
    bool m_blackout = false;
    int m_testPattern = -1, m_pixelTest = 0;
    QVector<double> m_taps;
    // render-thread state
    std::array<std::array<double, 2>, kMaxLayers> m_clockP{}, m_clockV{}, m_clockO{};
    QHash<QString, std::shared_ptr<MediaPlayer>> m_playersP, m_playersV, m_playersO;
    QHash<QString, QImage> m_textCache;
    mutable QMutex m_frameMutex;
    QImage m_lastProgram, m_lastPreview;
    QMap<int, DmxFrame> m_lastDmx;
    RenderStats m_lastStats;
    // project
    QString m_projectPath;
    quint64 m_cleanRevision = 0;
    quint64 m_showRevision = 0;
    double m_lastAutosave = 0;
    double m_loadedAt = 0; bool m_lkgWritten = false;
    int m_paramListener = 0;
    bool m_applyingSync = false;
    int m_lastControl = 0;
    bool m_offline = false;
    double m_virtualTime = 0;
};

} // namespace lf
