#include "Timeline.h"
#include <QJsonArray>
#include <cmath>
#include <algorithm>

namespace lf {

QJsonObject Cue::toJson() const {
    return {{"number", number}, {"name", name}, {"scene", sceneId}, {"fade", fadeTime}, {"delay", delay},
            {"follow", int(follow)}, {"followTime", followTime}, {"actions", QJsonObject::fromVariantMap(actions)}};
}
Cue Cue::fromJson(const QJsonObject& o) {
    Cue c; c.number = o["number"].toDouble(1); c.name = o["name"].toString(); c.sceneId = o["scene"].toString();
    c.fadeTime = o["fade"].toDouble(1); c.delay = o["delay"].toDouble(); c.follow = Follow(o["follow"].toInt());
    c.followTime = o["followTime"].toDouble(); c.actions = o["actions"].toObject().toVariantMap(); return c;
}

void CueList::sort() { std::sort(cues.begin(), cues.end(), [](auto& a, auto& b) { return a.number < b.number; }); }
int CueList::indexOf(double n) const { for (int i = 0; i < cues.size(); ++i) if (std::abs(cues[i].number - n) < 1e-6) return i; return -1; }
const Cue* CueList::go() { if (cues.isEmpty()) return nullptr; current = current + 1 >= cues.size() ? 0 : current + 1; m_firedAt = -1; return &cues[current]; }
const Cue* CueList::back() { if (cues.isEmpty() || current <= 0) return nullptr; --current; m_firedAt = -1; return &cues[current]; }
const Cue* CueList::gotoCue(double n) { const int i = indexOf(n); if (i < 0) return nullptr; current = i; m_firedAt = -1; return &cues[i]; }
const Cue* CueList::update(double now) {
    if (current < 0 || current >= cues.size() || m_firedAt < 0) return nullptr;
    const Cue& c = cues[current];
    if (current + 1 >= cues.size()) return nullptr;
    double due = -1;
    if (c.follow == Cue::AutoFollow) due = m_firedAt + c.delay + c.fadeTime;
    else if (c.follow == Cue::Wait) due = m_firedAt + c.delay + c.fadeTime + c.followTime;
    if (due >= 0 && now >= due) { ++current; m_firedAt = -1; return &cues[current]; }
    return nullptr;
}
double CueList::nextCueNumber() const { return cues.isEmpty() ? 1.0 : std::floor(cues.last().number) + 1.0; }
QJsonObject CueList::toJson() const { QJsonArray a; for (auto& c : cues) a << c.toJson(); return {{"name", name}, {"cues", a}}; }
CueList CueList::fromJson(const QJsonObject& o) { CueList l; l.name = o["name"].toString("Main"); for (auto v : o["cues"].toArray()) l.cues << Cue::fromJson(v.toObject()); l.sort(); return l; }

static const char* kCurves[] = {"linear", "step", "ease-in-out", "ease-in", "ease-out", "smooth"};
QString curveName(Curve c) { return kCurves[int(c)]; }
Curve curveFromName(const QString& s) { for (int i = 0; i < 6; ++i) if (s == kCurves[i]) return Curve(i); return Curve::Linear; }
static const char* kTracks[] = {"parameter", "effect", "media", "layer", "audio", "lighting", "osc", "midi", "dmx", "scene", "marker"};
QString trackTypeName(TrackType t) { return kTracks[int(t)]; }
TrackType trackTypeFromName(const QString& s) { for (int i = 0; i < 11; ++i) if (s == kTracks[i]) return TrackType(i); return TrackType::Parameter; }

static double shape(Curve c, double u) {
    switch (c) {
    case Curve::Linear: return u;
    case Curve::Step: return 0.0;
    case Curve::EaseInOut: return u < 0.5 ? 2 * u * u : 1 - std::pow(-2 * u + 2, 2) / 2;
    case Curve::EaseIn: return u * u;
    case Curve::EaseOut: return 1 - (1 - u) * (1 - u);
    case Curve::Smooth: return u * u * (3 - 2 * u);
    }
    return u;
}

double Track::evaluate(double t, bool* has) const {
    if (has) *has = !keys.isEmpty();
    if (keys.isEmpty()) return 0;
    if (t <= keys.first().time) return keys.first().value;
    if (t >= keys.last().time) return keys.last().value;
    auto it = std::upper_bound(keys.begin(), keys.end(), t, [](double v, const Keyframe& k) { return v < k.time; });
    const Keyframe& b = *it; const Keyframe& a = *(it - 1);
    const double u = (t - a.time) / std::max(1e-9, b.time - a.time);
    return a.value + (b.value - a.value) * shape(a.curve, u);
}

void Track::addKey(double time, double value, Curve c) {
    for (auto& k : keys) if (std::abs(k.time - time) < 1e-6) { k.value = value; k.curve = c; return; }
    keys << Keyframe{time, value, c};
    std::sort(keys.begin(), keys.end(), [](auto& a, auto& b) { return a.time < b.time; });
}

QJsonObject Track::toJson() const {
    QJsonArray k; for (auto& x : keys) k << QJsonArray{x.time, x.value, curveName(x.curve)};
    QJsonArray e; for (auto& x : events) e << QJsonObject{{"time", x.time}, {"action", x.action}, {"args", QJsonObject::fromVariantMap(x.args)}, {"label", x.label}};
    return {{"name", name}, {"type", trackTypeName(type)}, {"target", target}, {"keys", k}, {"events", e}, {"enabled", enabled}, {"solo", solo}};
}
Track Track::fromJson(const QJsonObject& o) {
    Track t; t.name = o["name"].toString(); t.type = trackTypeFromName(o["type"].toString()); t.target = o["target"].toString();
    for (auto v : o["keys"].toArray()) { auto a = v.toArray(); t.keys << Keyframe{a[0].toDouble(), a[1].toDouble(), curveFromName(a[2].toString())}; }
    for (auto v : o["events"].toArray()) { auto x = v.toObject(); t.events << TimelineEvent{x["time"].toDouble(), x["action"].toString(), x["args"].toObject().toVariantMap(), x["label"].toString()}; }
    std::sort(t.events.begin(), t.events.end(), [](auto& a, auto& b) { return a.time < b.time; });
    t.enabled = o["enabled"].toBool(true); t.solo = o["solo"].toBool();
    return t;
}

void Timeline::seek(double t) { m_pos = std::clamp(t, 0.0, duration); }

void Timeline::fireBetween(double a, double b, const EventHandler& h) const {
    if (!h) return;
    const bool anySolo = std::any_of(tracks.begin(), tracks.end(), [](auto& t) { return t.solo; });
    for (const auto& tr : tracks) {
        if (!tr.enabled || (anySolo && !tr.solo)) continue;
        for (const auto& e : tr.events) if (e.time > a && e.time <= b) h(e);
    }
}

void Timeline::applyAt(double t, const ParamWriter& writer) const {
    if (!writer) return;
    const bool anySolo = std::any_of(tracks.begin(), tracks.end(), [](auto& t) { return t.solo; });
    for (const auto& tr : tracks) {
        if (!tr.enabled || tr.target.isEmpty() || tr.keys.isEmpty() || (anySolo && !tr.solo)) continue;
        writer(tr.target, tr.evaluate(t));
    }
}

void Timeline::advance(double dt, const ParamWriter& writer, const EventHandler& events) {
    if (!m_playing) return;
    double a = m_pos, b = m_pos + dt * speed;
    const double out = loopOut > loopIn ? loopOut : duration;
    if (loop && b >= out) {
        fireBetween(a, out, events);
        b = loopIn + std::fmod(b - out, std::max(1e-6, out - loopIn));
        fireBetween(loopIn - 1e-9, b, events);
    } else if (b >= duration) { fireBetween(a, duration, events); b = duration; m_playing = false; }
    else fireBetween(a, b, events);
    m_pos = b;
    applyAt(m_pos, writer);
}

void Timeline::chase(double t, const ParamWriter& writer, const EventHandler& events) {
    if (t > m_pos && t - m_pos < 1.0) fireBetween(m_pos, t, events);   // only fire on small forward steps
    m_pos = std::clamp(t, 0.0, duration);
    applyAt(m_pos, writer);
}

double Timeline::snap(double t, double grid) const { const double g = beatDuration() * grid; return std::round(t / g) * g; }

QJsonObject Timeline::toJson() const {
    QJsonArray tr; for (auto& t : tracks) tr << t.toJson();
    QJsonArray mk; for (auto& m : markers) mk << QJsonObject{{"time", m.time}, {"name", m.name}};
    return {{"name", name}, {"tracks", tr}, {"markers", mk}, {"duration", duration}, {"bpm", bpm}, {"beatsPerBar", beatsPerBar},
            {"loop", loop}, {"loopIn", loopIn}, {"loopOut", loopOut}, {"speed", speed}};
}
Timeline Timeline::fromJson(const QJsonObject& o) {
    Timeline t; t.name = o["name"].toString("Show");
    for (auto v : o["tracks"].toArray()) t.tracks << Track::fromJson(v.toObject());
    for (auto v : o["markers"].toArray()) t.markers << Marker{v.toObject()["time"].toDouble(), v.toObject()["name"].toString()};
    t.duration = o["duration"].toDouble(120); t.bpm = o["bpm"].toDouble(120); t.beatsPerBar = o["beatsPerBar"].toInt(4);
    t.loop = o["loop"].toBool(); t.loopIn = o["loopIn"].toDouble(); t.loopOut = o["loopOut"].toDouble(); t.speed = o["speed"].toDouble(1);
    return t;
}

} // namespace lf
