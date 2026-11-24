#pragma once
// Bidirectional control mapping: DMX / OSC / MIDI inputs -> parameters, with
// scaling, inversion, deadband, smoothing, thresholds, edge-triggering, soft
// takeover, per-parameter ownership policies and feedback-loop prevention.
#include "core/Parameters.h"
#include "protocols/Osc.h"
#include "protocols/Midi.h"
#include <QString>
#include <QVector>
#include <QHash>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

namespace lf {

enum class ControlSource { Dmx, Osc, Midi, Touch, Remote, Timeline, Sync, Audio, Internal };
QString controlSourceName(ControlSource s);
ControlSource controlSourceFromName(const QString& s);

enum class OwnershipPolicy { LastChangeWins, ConsolePriority, TouchPriority, Locked };
QString ownershipPolicyName(OwnershipPolicy p);
OwnershipPolicy ownershipPolicyFromName(const QString& s);

enum class TriggerEdge { None, Rising, Falling, Both };

struct ControlMapping {
    QString id;
    QString console = "generic";      // "grandMA3", "MagicQ", "generic" - independent mapping sets
    ControlSource source = ControlSource::Osc;
    // address: DMX "universe/channel[/fine]", OSC address pattern, MIDI key ("cc:0:7")
    QString address;
    int oscArgIndex = 0;
    QString target;                   // parameter id, or action: "action:cue.go", "action:scene.recall"
    double inMin = 0, inMax = 1;      // input range (DMX normalised 0..1, MIDI 0..1, OSC raw)
    double outMin = 0, outMax = 1;    // normalised target range
    bool invert = false;
    double deadband = 0.0;            // ignore changes smaller than this (normalised)
    double smoothing = 0.0;           // 0 = none, 0.9 = heavy (one-pole)
    double threshold = 0.5;           // for triggers
    TriggerEdge edge = TriggerEdge::None;
    bool softTakeover = false;        // ignore input until it crosses the current value
    bool feedback = true;             // send value back to source when changed elsewhere
    bool enabled = true;
    QJsonObject toJson() const;
    static ControlMapping fromJson(const QJsonObject& o);
};

struct MappingState { double last = -1; double smoothed = -1; bool armed = true; bool pickedUp = false; bool triggerHigh = false; };

class MappingEngine {
public:
    using ActionHandler = std::function<void(const QString& action, double value, ControlSource src)>;
    using FeedbackSender = std::function<void(const ControlMapping& m, double normalisedValue)>;

    explicit MappingEngine(ParameterRegistry* reg);

    QVector<ControlMapping> mappings() const { return m_mappings; }
    void setMappings(const QVector<ControlMapping>& m);
    void add(const ControlMapping& m);
    void remove(const QString& id);

    void setPolicy(const QString& paramId, OwnershipPolicy p, ControlSource lockedOwner = ControlSource::Touch);
    OwnershipPolicy policy(const QString& paramId) const;
    void setDefaultPolicy(OwnershipPolicy p) { m_defaultPolicy = p; }
    OwnershipPolicy defaultPolicy() const { return m_defaultPolicy; }
    // Window during which a higher-priority source holds a parameter (ms).
    void setPriorityHoldMs(int ms) { m_holdMs = ms; }

    void setActionHandler(ActionHandler h) { m_action = std::move(h); }
    void setFeedbackSender(FeedbackSender f) { m_feedback = std::move(f); }

    // Inputs. Return number of mappings that fired.
    int handleOsc(const osc::Message& m);
    int handleMidi(const midi::Message& m);
    int handleDmx(int universe, const quint8* dmxSlots, int count);

    // Direct write from a UI/remote/timeline source, subject to ownership policy.
    bool write(const QString& paramId, double normalised, ControlSource src);
    ControlSource lastOwner(const QString& paramId) const { return m_owner.value(paramId).source; }

    // Learn mode: next input of any kind creates a mapping to this target.
    void startLearn(const QString& target, const QString& console = "generic");
    void cancelLearn() { m_learnTarget.clear(); }
    bool learning() const { return !m_learnTarget.isEmpty(); }
    QString lastLearned() const { return m_lastLearned; }

    // Called when a parameter changed (by any source) to emit feedback to other sources.
    void onParameterChanged(const QString& paramId, double value, const QString& sourceTag);

    QJsonArray toJson() const;
    void fromJson(const QJsonArray& a);
    static QString sourceTag(ControlSource s) { return "map:" + controlSourceName(s); }

private:
    int apply(ControlMapping& m, double raw);
    bool allowed(const QString& paramId, ControlSource src);
    void learnFrom(ControlSource src, const QString& address);

    ParameterRegistry* m_reg;
    QVector<ControlMapping> m_mappings;
    QHash<QString, MappingState> m_state;
    struct Owner { ControlSource source = ControlSource::Internal; qint64 atMs = -1000000; };
    QHash<QString, Owner> m_owner;
    QHash<QString, QPair<OwnershipPolicy, ControlSource>> m_policies;
    OwnershipPolicy m_defaultPolicy = OwnershipPolicy::LastChangeWins;
    int m_holdMs = 2000;
    QElapsedTimer m_clock;
    ActionHandler m_action;
    FeedbackSender m_feedback;
    QString m_learnTarget, m_learnConsole, m_lastLearned;
    QHash<QString, double> m_lastFeedback;   // loop prevention
    QHash<QString, quint8> m_lastDmx[1];
};

} // namespace lf
