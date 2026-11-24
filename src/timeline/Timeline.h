#pragma once
// Cue lists and multi-track show timeline with keyframed parameter automation,
// event tracks (OSC / MIDI / DMX / scene / cue triggers), markers, loop
// regions, tempo mapping and snap-to-grid.
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QVariantMap>
#include <functional>

namespace lf {

// ---------------------------------------------------------------- cue list
struct Cue {
    double number = 1.0;
    QString name;
    QString sceneId;            // scene to recall
    double fadeTime = 1.0;      // seconds
    double delay = 0.0;
    enum Follow { Manual, AutoFollow, Wait } follow = Manual;
    double followTime = 0.0;    // for Wait: seconds after cue completion
    QVariantMap actions;        // optional extra actions: {"osc": "/x 1", "timeline.play": true}
    QJsonObject toJson() const;
    static Cue fromJson(const QJsonObject& o);
};

class CueList {
public:
    QString name = "Main";
    QVector<Cue> cues;
    int current = -1;                   // index of active cue
    void sort();
    int indexOf(double number) const;
    // Navigation returns the cue to execute or nullptr.
    const Cue* go();
    const Cue* back();
    const Cue* gotoCue(double number);
    // Advance auto-follow/wait logic. Returns cue to fire if timing elapsed.
    const Cue* update(double now);
    void markFired(double now) { m_firedAt = now; }
    double nextCueNumber() const;
    QJsonObject toJson() const;
    static CueList fromJson(const QJsonObject& o);
private:
    double m_firedAt = -1;
};

// ---------------------------------------------------------------- timeline
enum class Curve { Linear, Step, EaseInOut, EaseIn, EaseOut, Smooth };
QString curveName(Curve c);
Curve curveFromName(const QString& s);

struct Keyframe { double time = 0; double value = 0; Curve curve = Curve::Linear; };

enum class TrackType { Parameter, Effect, Media, Layer, Audio, Lighting, Osc, Midi, Dmx, Scene, Marker };
QString trackTypeName(TrackType t);
TrackType trackTypeFromName(const QString& s);

struct TimelineEvent { double time = 0; QString action; QVariantMap args; QString label; };

struct Track {
    QString name;
    TrackType type = TrackType::Parameter;
    QString target;                    // parameter id for automation tracks
    QVector<Keyframe> keys;            // sorted by time
    QVector<TimelineEvent> events;     // sorted by time (trigger tracks)
    bool enabled = true;
    bool solo = false;
    double evaluate(double t, bool* has = nullptr) const;
    void addKey(double time, double value, Curve c = Curve::Linear);
    QJsonObject toJson() const;
    static Track fromJson(const QJsonObject& o);
};

struct Marker { double time; QString name; };

class Timeline {
public:
    QString name = "Show";
    QVector<Track> tracks;
    QVector<Marker> markers;
    double duration = 120.0;
    double bpm = 120.0;
    int beatsPerBar = 4;
    bool loop = false;
    double loopIn = 0.0, loopOut = 0.0;
    double speed = 1.0;

    using ParamWriter = std::function<void(const QString& id, double value)>;
    using EventHandler = std::function<void(const TimelineEvent& e)>;

    // Transport
    void play() { m_playing = true; }
    void pause() { m_playing = false; }
    void stop() { m_playing = false; seek(0); }
    bool playing() const { return m_playing; }
    void seek(double t);
    double position() const { return m_pos; }
    // Advance by dt (seconds, wall clock). Fires events crossed and writes automation.
    void advance(double dt, const ParamWriter& writer, const EventHandler& events);
    // Chase an external timecode position (seconds); fires events when moving forward.
    void chase(double t, const ParamWriter& writer, const EventHandler& events);
    void applyAt(double t, const ParamWriter& writer) const;

    double snap(double t, double gridBeats = 1.0) const;   // beat grid
    double beatDuration() const { return 60.0 / std::max(1.0, bpm); }
    QJsonObject toJson() const;
    static Timeline fromJson(const QJsonObject& o);
private:
    void fireBetween(double a, double b, const EventHandler& h) const;
    double m_pos = 0.0;
    bool m_playing = false;
};

} // namespace lf
