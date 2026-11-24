#include "Personality.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QUuid>

namespace lf {

QString personalityModeName(PersonalityMode m) {
    switch (m) { case PersonalityMode::Basic16: return "Basic 16ch"; case PersonalityMode::Standard32: return "Standard 32ch";
    case PersonalityMode::Extended64: return "Extended 64ch"; case PersonalityMode::Advanced128: return "Advanced 128ch"; case PersonalityMode::Custom: return "Custom"; }
    return "Custom";
}
int personalityModeChannels(PersonalityMode m) {
    switch (m) { case PersonalityMode::Basic16: return 16; case PersonalityMode::Standard32: return 32; case PersonalityMode::Extended64: return 64;
    case PersonalityMode::Advanced128: return 128; case PersonalityMode::Custom: return 512; }
    return 512;
}
PersonalityMode personalityModeFromName(const QString& s) {
    for (auto m : {PersonalityMode::Basic16, PersonalityMode::Standard32, PersonalityMode::Extended64, PersonalityMode::Advanced128, PersonalityMode::Custom})
        if (personalityModeName(m).startsWith(s.section(' ', 0, 0), Qt::CaseInsensitive)) return m;
    return PersonalityMode::Standard32;
}

QJsonObject PersonalityChannel::toJson() const {
    return {{"offset", offset}, {"fine", fineOffset}, {"param", paramId}, {"name", name}, {"gdtf", gdtfAttribute}, {"index", QJsonArray::fromStringList(indexNames)}};
}
PersonalityChannel PersonalityChannel::fromJson(const QJsonObject& o) {
    PersonalityChannel c; c.offset = o["offset"].toInt(1); c.fineOffset = o["fine"].toInt(); c.paramId = o["param"].toString();
    c.name = o["name"].toString(); c.gdtfAttribute = o["gdtf"].toString();
    for (auto v : o["index"].toArray()) c.indexNames << v.toString();
    return c;
}

int PersonalityProfile::footprint() const { int f = 0; for (auto& c : channels) f = std::max({f, c.offset, c.fineOffset}); return f; }
QString PersonalityProfile::fingerprint() const {
    QByteArray b;
    for (auto& c : channels) b += QString("%1:%2:%3;").arg(c.offset).arg(c.fineOffset).arg(c.paramId).toUtf8();
    return QCryptographicHash::hash(b, QCryptographicHash::Sha1).toHex().left(12);
}
QJsonObject PersonalityProfile::toJson() const {
    QJsonArray ch; for (auto& c : channels) ch << c.toJson();
    return {{"name", name}, {"mode", personalityModeName(mode)}, {"version", version}, {"createdAt", createdAt}, {"fingerprint", fingerprint()}, {"channels", ch}};
}
PersonalityProfile PersonalityProfile::fromJson(const QJsonObject& o) {
    PersonalityProfile p; p.name = o["name"].toString(p.name); p.mode = personalityModeFromName(o["mode"].toString());
    p.version = o["version"].toInt(1); p.createdAt = o["createdAt"].toString();
    for (auto v : o["channels"].toArray()) p.channels << PersonalityChannel::fromJson(v.toObject());
    return p;
}
const PersonalityChannel* PersonalityProfile::channelFor(const QString& id) const { for (auto& c : channels) if (c.paramId == id) return &c; return nullptr; }

QString ProfileDiff::summary() const {
    if (changes.isEmpty()) return "No changes";
    QStringList s;
    for (auto& c : changes) s << QString("%1%2: %3").arg(c.breaking ? "[BREAKING] " : "").arg(c.paramId, c.detail);
    return s.join('\n');
}

// Catalogue order is a compatibility contract: append new entries only.
QVector<PersonalityGenerator::CatalogEntry> PersonalityGenerator::catalogue(int layers) {
    QVector<CatalogEntry> c;
    // --- Basic (16 channels)
    c << CatalogEntry{"master.intensity", 16, "Dimmer"}
      << CatalogEntry{"layer1.effect", 8, "Effects1"} << CatalogEntry{"layer1.opacity", 8, "Effects1Adjust1"}
      << CatalogEntry{"layer1.speed", 8, "Effects1Rate"} << CatalogEntry{"layer1.scale", 8, "Effects1Adjust2"}
      << CatalogEntry{"layer1.rotation", 8, "Effects1Adjust3"} << CatalogEntry{"layer1.hue", 8, "Effects1Adjust4"}
      << CatalogEntry{"layer1.saturation", 8, "Effects1Adjust5"} << CatalogEntry{"show.scene", 8, "MediaContent1"}
      << CatalogEntry{"show.transition", 8, "Effects1Fade"} << CatalogEntry{"layer2.effect", 8, "Effects2"}
      << CatalogEntry{"layer2.opacity", 8, "Effects2Adjust1"} << CatalogEntry{"master.strobe", 8, "Shutter1Strobe"}
      << CatalogEntry{"show.control", 8, "Control1"} << CatalogEntry{"audio.reactivity", 8, "Effects9Adjust1"};
    // --- Standard (to 32)
    c << CatalogEntry{"layer1.posx", 16, "Effects1Adjust6"} << CatalogEntry{"layer1.posy", 16, "Effects1Adjust7"}
      << CatalogEntry{"layer1.p1", 8, "Effects1Adjust8"} << CatalogEntry{"layer1.p2", 8, "Effects1Adjust9"}
      << CatalogEntry{"layer1.p3", 8, "Effects1Adjust10"} << CatalogEntry{"layer1.p4", 8, "Effects1Adjust11"}
      << CatalogEntry{"layer1.brightness", 8, "Effects1Adjust12"} << CatalogEntry{"layer1.contrast", 8, "Effects1Adjust13"}
      << CatalogEntry{"layer1.hue2", 8, "Effects1Adjust14"} << CatalogEntry{"layer2.speed", 8, "Effects2Rate"}
      << CatalogEntry{"layer2.scale", 8, "Effects2Adjust2"} << CatalogEntry{"layer2.hue", 8, "Effects2Adjust4"}
      << CatalogEntry{"layer3.effect", 8, "Effects3"} << CatalogEntry{"layer3.opacity", 8, "Effects3Adjust1"};
    // --- Extended/Advanced: full sets for every layer in a fixed order
    const char* per[] = {"rotation", "saturation", "brightness", "contrast", "p1", "p2", "p3", "p4", "hue2", "blend"};
    for (int l = 2; l <= layers; ++l) {
        if (l >= 3) { c << CatalogEntry{QString("layer%1.speed").arg(l), 8, QString("Effects%1Rate").arg(l)}
                        << CatalogEntry{QString("layer%1.scale").arg(l), 8, QString("Effects%1Adjust2").arg(l)}
                        << CatalogEntry{QString("layer%1.hue").arg(l), 8, QString("Effects%1Adjust4").arg(l)}; }
        if (l == 4) c << CatalogEntry{"layer4.effect", 8, "Effects4"} << CatalogEntry{"layer4.opacity", 8, "Effects4Adjust1"};
        int adj = 3;
        for (auto p : per) { const QString pid = QString("layer%1.%2").arg(l).arg(p); c << CatalogEntry{pid, 8, QString("Effects%1Adjust%2").arg(l).arg(adj == 4 ? ++adj : adj)}; ++adj; }
        c << CatalogEntry{QString("layer%1.posx").arg(l), 16, QString("Effects%1Adjust20").arg(l)} << CatalogEntry{QString("layer%1.posy").arg(l), 16, QString("Effects%1Adjust21").arg(l)};
    }
    c << CatalogEntry{"layer1.blend", 8, "Effects1Adjust15"} << CatalogEntry{"audio.gain", 8, "Effects9Adjust2"}
      << CatalogEntry{"audio.smoothing", 8, "Effects9Adjust3"} << CatalogEntry{"show.bpm", 8, "Effects9Rate"}
      << CatalogEntry{"master.red", 8, "ColorRGB_Red"} << CatalogEntry{"master.green", 8, "ColorRGB_Green"} << CatalogEntry{"master.blue", 8, "ColorRGB_Blue"}
      << CatalogEntry{"master.saturation", 8, "Effects9Adjust4"};
    // Layers 5+ (appended later in the catalogue's history; order must never change)
    for (int l = 5; l <= layers; ++l) {
        c << CatalogEntry{QString("layer%1.effect").arg(l), 8, QString("Effects%1").arg(l)} << CatalogEntry{QString("layer%1.opacity").arg(l), 8, QString("Effects%1Adjust1").arg(l)}
          << CatalogEntry{QString("layer%1.speed").arg(l), 8, QString("Effects%1Rate").arg(l)} << CatalogEntry{QString("layer%1.scale").arg(l), 8, QString("Effects%1Adjust2").arg(l)}
          << CatalogEntry{QString("layer%1.hue").arg(l), 8, QString("Effects%1Adjust4").arg(l)};
        int adj = 3;
        for (auto p : per) { c << CatalogEntry{QString("layer%1.%2").arg(l).arg(p), 8, QString("Effects%1Adjust%2").arg(l).arg(adj == 4 ? ++adj : adj)}; ++adj; }
        c << CatalogEntry{QString("layer%1.posx").arg(l), 16, QString("Effects%1Adjust20").arg(l)} << CatalogEntry{QString("layer%1.posy").arg(l), 16, QString("Effects%1Adjust21").arg(l)};
    }
    return c;
}

PersonalityProfile PersonalityGenerator::generate(const ParameterRegistry& reg, PersonalityMode mode, const QStringList& effects,
                                                  const QStringList& scenes, const QStringList& custom) {
    PersonalityProfile p; p.mode = mode; p.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const int limit = personalityModeChannels(mode);
    QVector<CatalogEntry> entries;
    if (mode == PersonalityMode::Custom) {
        const auto cat = catalogue();
        for (const auto& id : custom) {
            CatalogEntry e{id, reg.def(id).resolution, QString()};
            for (auto& c : cat) if (c.paramId == id) e = c;
            if (e.gdtfAttribute.isEmpty()) e.gdtfAttribute = QString("Effects9Adjust%1").arg(10 + int(entries.size()));
            entries << e;
        }
    } else entries = catalogue();
    int ch = 1;
    for (const auto& e : entries) {
        if (!reg.has(e.paramId)) continue;            // parameter not present in this build/show
        const int need = e.resolution == 16 ? 2 : 1;
        if (ch + need - 1 > limit) break;
        PersonalityChannel pc; pc.offset = ch; pc.fineOffset = need == 2 ? ch + 1 : 0; pc.paramId = e.paramId;
        const ParameterDef d = reg.def(e.paramId);
        pc.name = (d.group.isEmpty() ? QString() : d.group + " ") + d.name;
        pc.gdtfAttribute = e.gdtfAttribute;
        if (e.paramId.endsWith(".effect")) pc.indexNames = effects;
        else if (e.paramId == "show.scene") pc.indexNames = scenes;
        else if (e.paramId == "show.control") pc.indexNames = {"Idle", "Cue Go", "Cue Back", "Blackout", "Tap tempo", "Freeze", "Take preview"};
        else if (d.type == ParamType::Index) for (int i = 0; i < d.indexCount; ++i) pc.indexNames << QString::number(i);
        p.channels << pc;
        ch += need;
    }
    return p;
}

ProfileDiff PersonalityGenerator::compare(const PersonalityProfile& a, const PersonalityProfile& b) {
    ProfileDiff d;
    for (const auto& oc : a.channels) {
        const PersonalityChannel* nc = b.channelFor(oc.paramId);
        if (!nc) { d.changes << ProfileChange{ProfileChange::Removed, oc.paramId, QString("removed from channel %1").arg(oc.offset), true}; continue; }
        if (nc->offset != oc.offset) d.changes << ProfileChange{ProfileChange::Moved, oc.paramId, QString("moved %1 -> %2").arg(oc.offset).arg(nc->offset), true};
        if ((nc->fineOffset > 0) != (oc.fineOffset > 0)) d.changes << ProfileChange{ProfileChange::ResolutionChanged, oc.paramId, QString("resolution %1 -> %2 bit").arg(oc.fineOffset ? 16 : 8).arg(nc->fineOffset ? 16 : 8), true};
        if (nc->name != oc.name) d.changes << ProfileChange{ProfileChange::Renamed, oc.paramId, QString("renamed '%1' -> '%2'").arg(oc.name, nc->name), false};
        if (nc->indexNames != oc.indexNames) {
            // Appending entries keeps old DMX ranges valid only if the range width is unchanged; we use fixed-width ranges
            // (256/entries) so any count change moves boundaries: flag as breaking unless only names changed.
            const bool countChanged = nc->indexNames.size() != oc.indexNames.size();
            d.changes << ProfileChange{ProfileChange::IndexRangesChanged, oc.paramId, countChanged ? QString("index entries %1 -> %2 (DMX ranges move)").arg(oc.indexNames.size()).arg(nc->indexNames.size()) : "index names changed", countChanged};
        }
    }
    for (const auto& nc : b.channels)
        if (!a.channelFor(nc.paramId)) d.changes << ProfileChange{ProfileChange::Added, nc.paramId, QString("added at channel %1").arg(nc.offset), nc.offset <= a.footprint()};
    return d;
}

// Index channels use fixed 8-step ranges so adding effects keeps existing ranges stable (up to 32 entries).
static int indexDmxFrom(int i) { return i * 8; }

gdtf::ExportSpec PersonalityGenerator::toGdtfSpec(const QVector<PersonalityProfile>& modes) {
    gdtf::ExportSpec s;
    s.manufacturer = "LUMAFLOW"; s.name = "LUMAFLOW Media Server"; s.shortName = "LUMAFLOW"; s.longName = "LUMAFLOW Media Server " LUMAFLOW_VERSION;
    s.description = "Fixture-style control personality for the LUMAFLOW media server";
    s.fixtureTypeId = QUuid("{7f3c2a10-4b1e-4c8a-9a51-1f0e5d6c7b80}");   // stable across versions
    QStringList rev; for (auto& m : modes) rev << QString("%1 v%2 (%3)").arg(personalityModeName(m.mode)).arg(m.version).arg(m.fingerprint());
    s.revisionText = "Generated by LUMAFLOW: " + rev.join(", ");
    for (const auto& p : modes) {
        gdtf::ExportMode em; em.name = personalityModeName(p.mode); em.description = QString("version %1").arg(p.version);
        for (const auto& c : p.channels) {
            gdtf::ExportChannel ec; ec.name = c.name; ec.attribute = c.gdtfAttribute; ec.pretty = c.name.left(12);
            ec.coarse = c.offset; ec.fine = c.fineOffset;
            ec.defaultValue = c.paramId == "layer1.opacity" ? 255 : (c.paramId.endsWith(".speed") || c.paramId.endsWith(".scale") ? 128 : 0);
            if (c.fineOffset) ec.defaultValue *= 257;
            for (int i = 0; i < c.indexNames.size() && i < 32; ++i) ec.sets << gdtf::ExportChannelSet{c.indexNames[i], indexDmxFrom(i)};
            em.channels << ec;
        }
        s.modes << em;
    }
    return s;
}

bool PersonalityGenerator::exportGdtf(const QString& path, const QVector<PersonalityProfile>& modes, QString* err) {
    return gdtf::exportFile(path, toGdtfSpec(modes), err);
}

QString PersonalityGenerator::magicqChannelSheet(const PersonalityProfile& p) {
    QString s = "# LUMAFLOW Media Server - " + personalityModeName(p.mode) + " v" + QString::number(p.version) + " (" + p.fingerprint() + ")\n";
    s += "# Channel sheet for building/verifying the head in MagicQ Head Editor, or for checking a GDTF import.\n";
    s += "Channel,Fine,Attribute,MagicQ type suggestion,Name,Ranges\n";
    for (auto& c : p.channels) {
        QString type = c.paramId == "master.intensity" ? "Dimmer (Intensity)" : c.paramId == "master.strobe" ? "Shutter" : (c.paramId.endsWith(".effect") || c.paramId == "show.scene") ? "Gobo (Beam) - ranged" : "Effect/Generic (Beam)";
        QStringList ranges; for (int i = 0; i < c.indexNames.size() && i < 32; ++i) ranges << QString("%1-%2 %3").arg(indexDmxFrom(i)).arg(indexDmxFrom(i) + 7).arg(c.indexNames[i]);
        s += QString("%1,%2,%3,%4,\"%5\",\"%6\"\n").arg(c.offset).arg(c.fineOffset ? QString::number(c.fineOffset) : "").arg(c.gdtfAttribute, type, c.name, ranges.join("; "));
    }
    return s;
}

QString PersonalityGenerator::grandma3Guide(const PersonalityProfile& p) {
    QString s = "# grandMA3 setup - LUMAFLOW " + personalityModeName(p.mode) + " (v" + QString::number(p.version) + ")\n\n";
    s += "1. Copy `LUMAFLOW_MediaServer.gdtf` to a USB stick under `grandMA3/shared/resource/fixturetypes/` or to the console's library folder.\n";
    s += "2. Menu > Patch > Fixture Types > Import > select *LUMAFLOW Media Server* > mode **" + personalityModeName(p.mode) + "**.\n";
    s += QString("3. Insert one fixture at the DMX address configured in LUMAFLOW (Console Integration > DMX input), footprint %1 channels.\n").arg(p.footprint());
    s += "4. In LUMAFLOW enable Art-Net or sACN input for that universe. grandMA3 DMX output: Menu > DMX Protocols > Art-Net/sACN, set the universe to Output.\n";
    s += "5. Program effects with the fixture's attributes (Effects1 = effect select on layer 1, Dimmer = master intensity, etc.) and store cues as usual.\n\n";
    s += "OSC (optional): Menu > In & Out > OSC: add a line with LUMAFLOW's IP, port 8000, Send enabled. Use `SendOSC` commands in macros, e.g.\n";
    s += "`SendOSC 1 \"/lumaflow/scene/recall,i,3\"` or `SendOSC 1 \"/lumaflow/cue/go\"`.\n";
    s += "LUMAFLOW feedback is sent as OSC to configured targets; grandMA3 can receive it when OSC Receive is enabled, but feedback does not\n";
    s += "change the console programmer or playback state. (Verify these menu paths against your grandMA3 software version.)\n\n";
    s += dmxReference(p);
    return s;
}

QString PersonalityGenerator::dmxReference(const PersonalityProfile& p) {
    QString s = "| Ch | Fine | Parameter | Name | GDTF attribute | Ranges |\n|---:|---:|---|---|---|---|\n";
    for (auto& c : p.channels) {
        QStringList r; for (int i = 0; i < c.indexNames.size() && i < 32; ++i) r << QString("%1-%2 %3").arg(indexDmxFrom(i)).arg(indexDmxFrom(i) + 7).arg(c.indexNames[i]);
        s += QString("| %1 | %2 | `%3` | %4 | %5 | %6 |\n").arg(c.offset).arg(c.fineOffset ? QString::number(c.fineOffset) : "").arg(c.paramId, c.name, c.gdtfAttribute, r.join(", "));
    }
    return s;
}

QVector<double> PersonalityGenerator::decodeDmx(const PersonalityProfile& p, const quint8* dmxSlots, int count) {
    QVector<double> v;
    for (auto& c : p.channels) {
        if (c.offset < 1 || c.offset > count) { v << -1; continue; }
        double n = dmxSlots[c.offset - 1] / 255.0;
        if (c.fineOffset > 0 && c.fineOffset <= count) n = (dmxSlots[c.offset - 1] * 256 + dmxSlots[c.fineOffset - 1]) / 65535.0;
        if (!c.indexNames.isEmpty()) n = std::min<int>(dmxSlots[c.offset - 1] / 8, int(c.indexNames.size()) - 1);   // index value
        v << n;
    }
    return v;
}

PersonalityProfile ProfileManager::active(PersonalityMode m) const { return m_active.value(int(m)); }

ProfileDiff ProfileManager::propose(const PersonalityProfile& c) {
    if (!m_active.contains(int(c.mode))) {
        PersonalityProfile p = c; p.version = 1; m_pending[int(c.mode)] = p;
        ProfileDiff d; for (auto& ch : c.channels) d.changes << ProfileChange{ProfileChange::Added, ch.paramId, QString("channel %1").arg(ch.offset), false};
        return d;
    }
    const PersonalityProfile& a = m_active[int(c.mode)];
    ProfileDiff d = PersonalityGenerator::compare(a, c);
    if (d.empty()) { m_pending.remove(int(c.mode)); return d; }
    PersonalityProfile p = c; p.version = a.version + 1;
    m_pending[int(c.mode)] = p;
    return d;
}

bool ProfileManager::approve(PersonalityMode m, bool allowBreaking, QString* why) {
    if (!m_pending.contains(int(m))) { if (why) *why = "nothing pending"; return false; }
    const PersonalityProfile p = m_pending[int(m)];
    if (m_active.contains(int(m))) {
        const ProfileDiff d = PersonalityGenerator::compare(m_active[int(m)], p);
        if (d.breaking() && !allowBreaking) { if (why) *why = "breaking changes require explicit confirmation:\n" + d.summary(); return false; }
        m_history[int(m)] << m_active[int(m)];
    }
    m_active[int(m)] = p;
    m_pending.remove(int(m));
    return true;
}

bool ProfileManager::rollback(PersonalityMode m) {
    auto& h = m_history[int(m)];
    if (h.isEmpty()) return false;
    m_active[int(m)] = h.takeLast();
    m_pending.remove(int(m));
    return true;
}

QJsonObject ProfileManager::toJson() const {
    QJsonObject o, act, pend, hist;
    for (auto it = m_active.begin(); it != m_active.end(); ++it) act[QString::number(it.key())] = it->toJson();
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) pend[QString::number(it.key())] = it->toJson();
    for (auto it = m_history.begin(); it != m_history.end(); ++it) { QJsonArray a; for (auto& p : *it) a << p.toJson(); hist[QString::number(it.key())] = a; }
    o["active"] = act; o["pending"] = pend; o["history"] = hist;
    return o;
}

void ProfileManager::fromJson(const QJsonObject& o) {
    m_active.clear(); m_pending.clear(); m_history.clear();
    const QJsonObject act = o["active"].toObject(), pend = o["pending"].toObject(), hist = o["history"].toObject();
    for (auto it = act.begin(); it != act.end(); ++it) m_active[it.key().toInt()] = PersonalityProfile::fromJson(it.value().toObject());
    for (auto it = pend.begin(); it != pend.end(); ++it) m_pending[it.key().toInt()] = PersonalityProfile::fromJson(it.value().toObject());
    for (auto it = hist.begin(); it != hist.end(); ++it) for (auto v : it.value().toArray()) m_history[it.key().toInt()] << PersonalityProfile::fromJson(v.toObject());
}

} // namespace lf
