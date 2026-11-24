#include "Gdtf.h"
#include "storage/Zip.h"
#include <QDomDocument>
#include <QFileInfo>
#include <QXmlStreamWriter>
#include <QSet>
#include <QDateTime>

namespace lf::gdtf {

static QList<int> parseOffsets(const QString& s) {
    QList<int> out;
    if (s.trimmed().isEmpty() || s == "None") return out;
    for (const auto& p : s.split(',')) out << p.trimmed().toInt();
    return out;
}

static int parseDmxValue(const QString& s, int bytes) {
    // GDTF DMXValue "value/n" where n is byte count of the value's resolution.
    const auto parts = s.split('/');
    if (parts.isEmpty()) return 0;
    long long v = parts[0].toLongLong();
    const int n = parts.size() > 1 ? parts[1].toInt() : 1;
    if (n > bytes) v >>= 8 * (n - bytes); else if (n < bytes) v <<= 8 * (bytes - n);
    return int(v);
}

ImportResult importFile(const QString& path) {
    ImportResult r;
    ZipReader zip;
    if (!zip.open(path)) { r.error = zip.error(); return r; }
    bool ok = false;
    const QByteArray xml = zip.read("description.xml", &ok);
    if (!ok) { r.error = "description.xml missing or corrupt in " + QFileInfo(path).fileName(); return r; }
    return importDescription(xml, "gdtf:" + QFileInfo(path).fileName());
}

ImportResult importDescription(const QByteArray& xml, const QString& sourceName) {
    ImportResult r;
    QDomDocument doc;
    QString perrMsg; int perrLine = 0, perrCol = 0;
    const bool parse = doc.setContent(xml, &perrMsg, &perrLine, &perrCol);
    if (!parse) { r.error = QString("XML parse error at line %1: %2").arg(perrLine).arg(perrMsg); return r; }
    const QDomElement root = doc.documentElement();
    if (root.tagName() != "GDTF") { r.error = "root element is not <GDTF>"; return r; }
    const QString ver = root.attribute("DataVersion");
    if (ver.isEmpty()) r.warnings << "DataVersion attribute missing";
    else if (!ver.startsWith("1.")) r.warnings << "Untested GDTF DataVersion " + ver;
    const QDomElement ft = root.firstChildElement("FixtureType");
    if (ft.isNull()) { r.error = "<FixtureType> missing"; return r; }
    FixtureType& t = r.type;
    t.manufacturer = ft.attribute("Manufacturer");
    t.name = ft.attribute("Name");
    t.id = ft.attribute("FixtureTypeID");
    if (t.id.isEmpty()) { t.id = t.manufacturer + "@" + t.name; r.warnings << "FixtureTypeID missing - using manufacturer@name"; }
    t.source = sourceName;

    // Geometry references (multi-instance): template geometry name -> list of (instance name, DMX offset)
    struct Ref { QString name; int dmxOffset; };
    QMap<QString, QVector<Ref>> refs;
    QMap<QString, Vec3> refPos;
    const QDomNodeList grefs = ft.elementsByTagName("GeometryReference");
    for (int i = 0; i < grefs.size(); ++i) {
        const QDomElement g = grefs.at(i).toElement();
        int off = 1;
        const QDomElement br = g.firstChildElement("Break");
        if (!br.isNull()) off = br.attribute("DMXOffset", "1").toInt();
        refs[g.attribute("Geometry")] << Ref{g.attribute("Name"), off};
        // Position "{a,b,c,x}{...}" 4x4 row-major; translation in last column (mm)
        const QString pos = g.attribute("Position");
        QStringList rows = pos.split('}', Qt::SkipEmptyParts);
        if (rows.size() >= 3) {
            auto col = [&](int row) { QString s = rows[row]; s.remove('{'); auto v = s.split(','); return v.size() >= 4 ? v[3].toDouble() / 1000.0 : 0.0; };
            refPos[g.attribute("Name")] = Vec3(col(0), col(1), col(2));
        }
    }

    const QDomNodeList modes = ft.firstChildElement("DMXModes").elementsByTagName("DMXMode");
    if (modes.isEmpty()) r.warnings << "No DMX modes defined";
    for (int mi = 0; mi < modes.size(); ++mi) {
        const QDomElement me = modes.at(mi).toElement();
        FixtureMode mode; mode.name = me.attribute("Name", QString("Mode %1").arg(mi + 1));
        const QDomNodeList chans = me.firstChildElement("DMXChannels").elementsByTagName("DMXChannel");
        QSet<QString> cellNames;
        for (int ci = 0; ci < chans.size(); ++ci) {
            const QDomElement ce = chans.at(ci).toElement();
            const QList<int> offs = parseOffsets(ce.attribute("Offset"));
            if (offs.isEmpty()) continue;                                 // virtual channel (no DMX)
            const QDomElement lc = ce.firstChildElement("LogicalChannel");
            const QString attrName = lc.attribute("Attribute");
            const QDomElement cf = lc.firstChildElement("ChannelFunction");
            const QString geometry = ce.attribute("Geometry");
            const int bytes = int(offs.size());
            int def = 0;
            const QString defStr = cf.isNull() ? ce.attribute("Default") : cf.attribute("Default", ce.attribute("Default"));
            if (!defStr.isEmpty()) def = parseDmxValue(defStr, bytes) >> (8 * (bytes - 1));
            ChannelDef cd;
            cd.attributeRaw = attrName;
            cd.attribute = attributeFromGdtf(attrName);
            cd.name = attrName;
            cd.defaultValue = def;
            if (cd.attribute == Attribute::Unknown) r.warnings << QString("Mode '%1': unrecognised attribute '%2' (kept as raw)").arg(mode.name, attrName);
            if (bytes > 2) r.warnings << QString("Mode '%1': %2 is %3-byte; LUMAFLOW uses the top 16 bits").arg(mode.name, attrName).arg(bytes);
            const auto instances = refs.value(geometry);
            if (instances.isEmpty()) {
                cd.offset = offs[0] - 1; cd.fineOffset = bytes > 1 ? offs[1] - 1 : -1;
                mode.channels << cd;
            } else {
                for (int k = 0; k < instances.size(); ++k) {
                    ChannelDef c2 = cd;
                    c2.cell = k;
                    c2.offset = offs[0] - 1 + instances[k].dmxOffset - 1;
                    c2.fineOffset = bytes > 1 ? offs[1] - 1 + instances[k].dmxOffset - 1 : -1;
                    mode.channels << c2;
                    cellNames.insert(instances[k].name);
                }
                if (mode.cellOffsets.size() < instances.size()) {
                    mode.cellOffsets.clear();
                    for (auto& in : instances) mode.cellOffsets << refPos.value(in.name);
                }
            }
        }
        mode.cellCount = std::max(1, int(mode.cellOffsets.size()));
        // overlap check inside the mode
        QSet<int> used;
        for (auto& c : mode.channels) {
            for (int o : {c.offset, c.fineOffset}) if (o >= 0) { if (used.contains(o)) r.warnings << QString("Mode '%1': DMX offset %2 used twice").arg(mode.name).arg(o + 1); used.insert(o); }
        }
        std::sort(mode.channels.begin(), mode.channels.end(), [](auto& a, auto& b) { return a.offset < b.offset; });
        t.modes << mode;
    }
    return r;
}

static QString featureFor(const QString& attr) {
    if (attr.startsWith("Dimmer")) return "Dimmer.Dimmer";
    if (attr == "Pan" || attr == "Tilt") return "Position.PanTilt";
    if (attr.startsWith("Color")) return "Color.RGB";
    if (attr.startsWith("Shutter")) return "Beam.Beam";
    if (attr.startsWith("Zoom") || attr.startsWith("Focus") || attr == "Iris") return "Focus.Focus";
    if (attr.startsWith("Gobo")) return "Gobo.Gobo";
    if (attr.startsWith("Prism")) return "Beam.Beam";
    if (attr.startsWith("Media") || attr.startsWith("Video") || attr.startsWith("Play")) return "Video.Video";
    if (attr.startsWith("Effects")) return "Control.Control";
    return "Control.Control";
}

QByteArray exportDescription(const ExportSpec& s) {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument("1.0");
    w.writeStartElement("GDTF");
    w.writeAttribute("DataVersion", "1.2");
    w.writeStartElement("FixtureType");
    w.writeAttribute("Name", s.name);
    w.writeAttribute("ShortName", s.shortName.isEmpty() ? s.name : s.shortName);
    w.writeAttribute("LongName", s.longName.isEmpty() ? s.name : s.longName);
    w.writeAttribute("Manufacturer", s.manufacturer);
    w.writeAttribute("Description", s.description);
    w.writeAttribute("FixtureTypeID", s.fixtureTypeId.toString(QUuid::WithoutBraces).toUpper());
    w.writeAttribute("Thumbnail", "");
    w.writeAttribute("RefFT", "");

    // Attribute definitions: every attribute used in any mode, grouped by feature
    QMap<QString, QSet<QString>> features;   // group -> features
    QMap<QString, QPair<QString, QString>> attrs; // attr -> (pretty, feature)
    for (auto& m : s.modes) for (auto& c : m.channels) {
        const QString feat = c.feature.isEmpty() ? featureFor(c.attribute) : c.feature;
        features[feat.section('.', 0, 0)].insert(feat.section('.', 1, 1));
        attrs[c.attribute] = qMakePair(c.pretty.isEmpty() ? c.name : c.pretty, feat);
    }
    w.writeStartElement("AttributeDefinitions");
    w.writeEmptyElement("ActivationGroups");
    w.writeStartElement("FeatureGroups");
    for (auto it = features.begin(); it != features.end(); ++it) {
        w.writeStartElement("FeatureGroup"); w.writeAttribute("Name", it.key()); w.writeAttribute("Pretty", it.key());
        for (auto& f : it.value()) { w.writeEmptyElement("Feature"); w.writeAttribute("Name", f); }
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeStartElement("Attributes");
    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        w.writeEmptyElement("Attribute"); w.writeAttribute("Name", it.key()); w.writeAttribute("Pretty", it.value().first);
        w.writeAttribute("Feature", it.value().second); w.writeAttribute("PhysicalUnit", "None");
    }
    w.writeEndElement(); // Attributes
    w.writeEndElement(); // AttributeDefinitions
    w.writeEmptyElement("Wheels");
    w.writeStartElement("PhysicalDescriptions"); w.writeEmptyElement("Emitters"); w.writeEmptyElement("Filters"); w.writeEndElement();
    w.writeStartElement("Models");
    w.writeEmptyElement("Model"); w.writeAttribute("Name", "Body"); w.writeAttribute("Length", "0.3"); w.writeAttribute("Width", "0.3"); w.writeAttribute("Height", "0.1"); w.writeAttribute("PrimitiveType", "Cube");
    w.writeEndElement();
    w.writeStartElement("Geometries");
    w.writeEmptyElement("Geometry"); w.writeAttribute("Name", "Body"); w.writeAttribute("Model", "Body"); w.writeAttribute("Position", "{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}");
    w.writeEndElement();
    w.writeStartElement("DMXModes");
    for (auto& m : s.modes) {
        w.writeStartElement("DMXMode"); w.writeAttribute("Name", m.name); w.writeAttribute("Description", m.description); w.writeAttribute("Geometry", "Body");
        w.writeStartElement("DMXChannels");
        for (auto& c : m.channels) {
            const int bytes = c.fine > 0 ? 2 : 1;
            const QString def = QString("%1/%2").arg(c.defaultValue).arg(bytes);
            w.writeStartElement("DMXChannel");
            w.writeAttribute("DMXBreak", "1");
            w.writeAttribute("Offset", c.fine > 0 ? QString("%1,%2").arg(c.coarse).arg(c.fine) : QString::number(c.coarse));
            w.writeAttribute("InitialFunction", QString("Body_%1.%1.%2").arg(c.attribute, c.name));
            w.writeAttribute("Highlight", "None");
            w.writeAttribute("Geometry", "Body");
            w.writeStartElement("LogicalChannel");
            w.writeAttribute("Attribute", c.attribute); w.writeAttribute("Snap", "No"); w.writeAttribute("Master", c.attribute == "Dimmer" ? "Grand" : "None");
            w.writeAttribute("MibFade", "0"); w.writeAttribute("DMXChangeTimeLimit", "0");
            w.writeStartElement("ChannelFunction");
            w.writeAttribute("Name", c.name); w.writeAttribute("Attribute", c.attribute); w.writeAttribute("OriginalAttribute", "");
            w.writeAttribute("DMXFrom", QString("0/%1").arg(bytes)); w.writeAttribute("Default", def);
            w.writeAttribute("PhysicalFrom", QString::number(c.physicalFrom)); w.writeAttribute("PhysicalTo", QString::number(c.physicalTo));
            w.writeAttribute("RealFade", "0"); w.writeAttribute("RealAcceleration", "0");
            for (auto& cs : c.sets) {
                w.writeEmptyElement("ChannelSet"); w.writeAttribute("Name", cs.name); w.writeAttribute("DMXFrom", QString("%1/%2").arg(cs.dmxFrom).arg(bytes));
            }
            w.writeEndElement(); w.writeEndElement(); w.writeEndElement();
        }
        w.writeEndElement(); // DMXChannels
        w.writeEmptyElement("Relations"); w.writeEmptyElement("FTMacros");
        w.writeEndElement(); // DMXMode
    }
    w.writeEndElement(); // DMXModes
    w.writeStartElement("Revisions");
    w.writeEmptyElement("Revision"); w.writeAttribute("Text", s.revisionText.isEmpty() ? "Generated by LUMAFLOW" : s.revisionText);
    w.writeAttribute("Date", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)); w.writeAttribute("UserID", "0");
    w.writeEndElement();
    w.writeEmptyElement("FTPresets"); w.writeEmptyElement("Protocols");
    w.writeEndElement(); // FixtureType
    w.writeEndElement(); // GDTF
    w.writeEndDocument();
    return out;
}

bool exportFile(const QString& path, const ExportSpec& spec, QString* error) {
    const QByteArray xml = exportDescription(spec);
    const QStringList problems = validateDescription(xml);
    if (!problems.isEmpty()) { if (error) *error = problems.join("; "); return false; }
    ZipWriter z; z.add("description.xml", xml);
    if (!z.save(path)) { if (error) *error = "cannot write " + path; return false; }
    return true;
}

ExportSpec specFromFixtureType(const FixtureType& t) {
    ExportSpec s; s.manufacturer = t.manufacturer; s.name = t.name;
    s.fixtureTypeId = QUuid::fromString(t.id).isNull() ? QUuid::createUuidV5(QUuid("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}"), t.id) : QUuid::fromString(t.id);
    for (auto& m : t.modes) {
        ExportMode em; em.name = m.name;
        for (auto& c : m.channels) {
            ExportChannel ec;
            ec.attribute = !c.attributeRaw.isEmpty() ? c.attributeRaw : attributeToGdtf(c.attribute);
            if (m.cellCount > 1) ec.attribute += "";   // cells flattened into consecutive channels
            ec.name = m.cellCount > 1 ? QString("%1 %2").arg(attributeName(c.attribute)).arg(c.cell + 1) : attributeName(c.attribute);
            ec.coarse = c.offset + 1; ec.fine = c.fineOffset >= 0 ? c.fineOffset + 1 : 0; ec.defaultValue = c.defaultValue;
            em.channels << ec;
        }
        s.modes << em;
    }
    return s;
}

QStringList validateDescription(const QByteArray& xml) {
    QStringList p;
    QDomDocument doc;
    if (!doc.setContent(xml)) return {"not well-formed XML"};
    const QDomElement root = doc.documentElement();
    if (root.tagName() != "GDTF" || root.attribute("DataVersion").isEmpty()) p << "missing <GDTF DataVersion>";
    const QDomElement ft = root.firstChildElement("FixtureType");
    for (const char* a : {"Name", "ShortName", "LongName", "Manufacturer", "FixtureTypeID"})
        if (ft.attribute(a).isEmpty()) p << QString("FixtureType/@%1 empty").arg(a);
    if (QUuid::fromString(ft.attribute("FixtureTypeID")).isNull()) p << "FixtureTypeID is not a UUID";
    QSet<QString> defined;
    const QDomNodeList attrs = ft.firstChildElement("AttributeDefinitions").firstChildElement("Attributes").elementsByTagName("Attribute");
    for (int i = 0; i < attrs.size(); ++i) defined.insert(attrs.at(i).toElement().attribute("Name"));
    QSet<QString> geoms;
    const QDomNodeList gs = ft.firstChildElement("Geometries").elementsByTagName("Geometry");
    for (int i = 0; i < gs.size(); ++i) geoms.insert(gs.at(i).toElement().attribute("Name"));
    const QDomNodeList modes = ft.firstChildElement("DMXModes").elementsByTagName("DMXMode");
    if (modes.isEmpty()) p << "no DMXMode";
    QSet<QString> modeNames;
    for (int i = 0; i < modes.size(); ++i) {
        const QDomElement m = modes.at(i).toElement();
        const QString mn = m.attribute("Name");
        if (modeNames.contains(mn)) p << "duplicate mode name " + mn;
        modeNames.insert(mn);
        if (!geoms.contains(m.attribute("Geometry"))) p << "mode " + mn + " references unknown geometry";
        QSet<int> used;
        const QDomNodeList ch = m.elementsByTagName("DMXChannel");
        for (int c = 0; c < ch.size(); ++c) {
            const QDomElement ce = ch.at(c).toElement();
            for (int o : parseOffsets(ce.attribute("Offset"))) {
                if (o < 1 || o > 512) p << QString("mode %1: offset %2 out of range").arg(mn).arg(o);
                if (used.contains(o)) p << QString("mode %1: offset %2 duplicated").arg(mn).arg(o);
                used.insert(o);
            }
            const QString attr = ce.firstChildElement("LogicalChannel").attribute("Attribute");
            if (!defined.contains(attr)) p << QString("mode %1: attribute %2 not defined").arg(mn, attr);
            if (!geoms.contains(ce.attribute("Geometry"))) p << QString("mode %1: channel geometry unknown").arg(mn);
        }
    }
    return p;
}

} // namespace lf::gdtf
