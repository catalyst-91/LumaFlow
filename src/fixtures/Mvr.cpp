#include "Mvr.h"
#include "Gdtf.h"
#include "storage/Zip.h"
#include <QDomDocument>
#include <QXmlStreamWriter>
#include <QUuid>
#include <QFileInfo>

namespace lf::mvr {

Mat4 parseMatrix(const QString& s) {
    Mat4 m;
    QString t = s; t.remove(' ');
    const QStringList groups = t.split('}', Qt::SkipEmptyParts);
    QVector<QVector<double>> rows;
    for (QString g : groups) { g.remove('{'); QVector<double> r; for (auto& v : g.split(',')) r << v.toDouble(); rows << r; }
    if (rows.size() == 4 && rows[0].size() == 3) {
        // MVR: rows are u, v, w basis vectors and o (translation). Matrix columns = basis vectors.
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) m.m[j * 4 + i] = rows[i][j];
        m.m[3] = rows[3][0]; m.m[7] = rows[3][1]; m.m[11] = rows[3][2];
    } else if (rows.size() == 4 && rows[0].size() == 4) {      // GDTF-style 4x4 row-major
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m.m[i * 4 + j] = rows[i][j];
    }
    return m;
}

QString formatMatrix(const Vec3& p, const Vec3& rotDeg) {
    const Mat4 r = Mat4::rotationXYZDeg(rotDeg);
    auto f = [](double v) { return QString::number(std::abs(v) < 1e-12 ? 0.0 : v, 'g', 12); };
    return QString("{%1,%2,%3}{%4,%5,%6}{%7,%8,%9}{%10,%11,%12}")
        .arg(f(r.m[0]), f(r.m[4]), f(r.m[8])).arg(f(r.m[1]), f(r.m[5]), f(r.m[9]))
        .arg(f(r.m[2]), f(r.m[6]), f(r.m[10])).arg(f(p.x), f(p.y), f(p.z));
}

ImportResult importFile(const QString& path, const ImportOptions& opt) {
    ImportResult r;
    ZipReader z;
    if (!z.open(path)) { r.error = z.error(); return r; }
    bool ok = false;
    const QByteArray xml = z.read("GeneralSceneDescription.xml", &ok);
    if (!ok) { r.error = "GeneralSceneDescription.xml missing in " + QFileInfo(path).fileName(); return r; }
    QMap<QString, QByteArray> gdtfs;
    for (const auto& e : z.entries()) if (e.endsWith(".gdtf", Qt::CaseInsensitive)) gdtfs.insert(QFileInfo(e).fileName(), z.read(e));
    return importScene(xml, gdtfs, opt);
}

static void walk(const QDomElement& parent, const Mat4& parentM, const QString& layer, const QString& group,
                 const QMap<QString, QByteArray>& gdtfs, const ImportOptions& opt, ImportResult& r, QMap<QString, QString>& typeIdByFile, int& nextFid) {
    for (QDomElement e = parent.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        const QString tag = e.tagName();
        Mat4 local = parseMatrix(e.firstChildElement("Matrix").text());
        if (e.firstChildElement("Matrix").isNull()) local = Mat4::identity();
        const Mat4 world = parentM * local;
        if (tag == "GroupObject" || tag == "ChildList") {
            const QString g = tag == "GroupObject" ? e.attribute("name") : group;
            if (tag == "GroupObject" && !g.isEmpty() && !r.groups.contains(g)) r.groups << g;
            walk(tag == "GroupObject" ? e.firstChildElement("ChildList") : e, tag == "GroupObject" ? world : parentM, layer, g, gdtfs, opt, r, typeIdByFile, nextFid);
            continue;
        }
        if (tag != "Fixture") {
            if (tag == "Truss" || tag == "SceneObject" || tag == "VideoScreen" || tag == "Projector" || tag == "Support")
                walk(e.firstChildElement("ChildList"), world, layer, group, gdtfs, opt, r, typeIdByFile, nextFid);
            continue;
        }
        FixtureInstance f;
        f.uuid = e.attribute("uuid");
        f.name = e.attribute("name");
        f.layer = layer;
        bool ok = false;
        f.fid = e.firstChildElement("FixtureID").text().toInt(&ok);
        if (!ok || f.fid <= 0) { f.fid = e.firstChildElement("UnitNumber").text().toInt(&ok); }
        if (!ok || f.fid <= 0) { f.fid = nextFid; r.warnings << QString("Fixture '%1' has no FixtureID - assigned %2").arg(f.name).arg(f.fid); }
        nextFid = std::max(nextFid, f.fid + 1);
        const QString spec = e.firstChildElement("GDTFSpec").text().trimmed();
        f.modeName = e.firstChildElement("GDTFMode").text().trimmed();
        if (!group.isEmpty()) f.groups << group;
        for (QDomElement fg = e.firstChildElement("Group"); !fg.isNull(); fg = fg.nextSiblingElement("Group")) f.groups << fg.text();
        // Address: either absolute integer (1-based over 512-slot universes) or "universe.address"
        const QDomElement addrs = e.firstChildElement("Addresses");
        const QDomElement a0 = addrs.firstChildElement("Address");
        if (a0.isNull()) { r.warnings << QString("Fixture %1 '%2' is unpatched (no address)").arg(f.fid).arg(f.name); f.universe = 0; f.address = 0; }
        else {
            const QString v = a0.text().trimmed();
            if (v.contains('.')) { f.universe = v.section('.', 0, 0).toInt(); f.address = v.section('.', 1, 1).toInt(); }
            else { const int abs = v.toInt(); f.universe = (abs - 1) / 512 + 1; f.address = (abs - 1) % 512 + 1; }
            if (a0.attribute("break", "0") != "0") r.warnings << QString("Fixture %1 uses DMX break %2 - only break 0 imported").arg(f.fid).arg(a0.attribute("break"));
        }
        const Vec3 posSrc = world.translationPart();
        f.position = posSrc * opt.unitScale + opt.originOffset;
        f.rotation = world.eulerXYZDeg();
        // fixture type
        if (typeIdByFile.contains(spec)) f.typeId = typeIdByFile[spec];
        else if (opt.importGdtfTypes) {
            QString file = spec; if (!file.endsWith(".gdtf", Qt::CaseInsensitive)) file += ".gdtf";
            if (gdtfs.contains(file)) {
                ZipReader gz; gz.openData(gdtfs[file]);
                bool okx = false; const QByteArray desc = gz.read("description.xml", &okx);
                auto gr = okx ? gdtf::importDescription(desc, "mvr:" + file) : gdtf::ImportResult{{}, {}, "unreadable"};
                if (gr.ok()) { r.patch.types.add(gr.type); typeIdByFile[spec] = gr.type.id; f.typeId = gr.type.id; for (auto& w : gr.warnings) r.warnings << file + ": " + w; }
                else r.warnings << QString("GDTF '%1' could not be parsed: %2").arg(file, gr.error);
            } else r.warnings << QString("Fixture %1: GDTF '%2' not embedded in MVR - map it manually").arg(f.fid).arg(spec);
        }
        if (f.typeId.isEmpty()) f.typeId = "unresolved:" + spec;
        else if (const FixtureType* t = r.patch.types.find(f.typeId); t && !t->mode(f.modeName)) {
            r.warnings << QString("Fixture %1: mode '%2' not found in %3 - using first mode").arg(f.fid).arg(f.modeName, t->name);
            if (!t->modes.isEmpty()) f.modeName = t->modes.first().name;
        }
        r.patch.fixtures << f;
        walk(e.firstChildElement("ChildList"), world, layer, group, gdtfs, opt, r, typeIdByFile, nextFid);
    }
}

ImportResult importScene(const QByteArray& xml, const QMap<QString, QByteArray>& gdtfs, const ImportOptions& opt) {
    ImportResult r;
    QDomDocument doc;
    QString perrMsg; int perrLine = 0, perrCol = 0;
    const bool pr = doc.setContent(xml, &perrMsg, &perrLine, &perrCol);
    if (!pr) { r.error = QString("XML error line %1: %2").arg(perrLine).arg(perrMsg); return r; }
    const QDomElement root = doc.documentElement();
    if (root.tagName() != "GeneralSceneDescription") { r.error = "root element is not GeneralSceneDescription"; return r; }
    if (root.attribute("verMajor").toInt() != 1) r.warnings << "Untested MVR version " + root.attribute("verMajor") + "." + root.attribute("verMinor");
    QMap<QString, QString> typeIds;
    int nextFid = opt.firstFid;
    const QDomNodeList layers = root.firstChildElement("Scene").firstChildElement("Layers").elementsByTagName("Layer");
    for (int i = 0; i < layers.size(); ++i) {
        const QDomElement l = layers.at(i).toElement();
        Mat4 lm = l.firstChildElement("Matrix").isNull() ? Mat4::identity() : parseMatrix(l.firstChildElement("Matrix").text());
        walk(l.firstChildElement("ChildList"), lm, l.attribute("name"), QString(), gdtfs, opt, r, typeIds, nextFid);
    }
    if (r.patch.fixtures.isEmpty()) r.warnings << "No fixtures found in MVR scene";
    for (auto& issue : r.patch.validate()) r.warnings << issue.message;
    return r;
}

QByteArray exportScene(const Patch& patch) {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument("1.0");
    w.writeStartElement("GeneralSceneDescription");
    w.writeAttribute("verMajor", "1"); w.writeAttribute("verMinor", "6"); w.writeAttribute("provider", "LUMAFLOW"); w.writeAttribute("providerVersion", LUMAFLOW_VERSION);
    w.writeStartElement("UserData"); w.writeEndElement();
    w.writeStartElement("Scene");
    w.writeStartElement("AUXData"); w.writeEndElement();
    w.writeStartElement("Layers");
    QMap<QString, QVector<const FixtureInstance*>> byLayer;
    for (auto& f : patch.fixtures) byLayer[f.layer.isEmpty() ? "LUMAFLOW" : f.layer] << &f;
    for (auto it = byLayer.begin(); it != byLayer.end(); ++it) {
        w.writeStartElement("Layer"); w.writeAttribute("name", it.key()); w.writeAttribute("uuid", QUuid::createUuidV5(QUuid(), it.key()).toString(QUuid::WithoutBraces).toUpper());
        w.writeStartElement("ChildList");
        for (const FixtureInstance* f : *it) {
            const FixtureType* t = patch.typeOf(*f);
            w.writeStartElement("Fixture");
            w.writeAttribute("name", f->name);
            w.writeAttribute("uuid", (f->uuid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : f->uuid).toUpper());
            w.writeTextElement("Matrix", formatMatrix(f->position * 1000.0, f->rotation));
            w.writeTextElement("GDTFSpec", t ? (t->manufacturer + "@" + t->name + ".gdtf") : f->typeId);
            w.writeTextElement("GDTFMode", f->modeName);
            w.writeTextElement("FixtureID", QString::number(f->fid));
            w.writeTextElement("UnitNumber", QString::number(f->fid));
            w.writeStartElement("Addresses");
            w.writeStartElement("Address"); w.writeAttribute("break", "0"); w.writeCharacters(QString::number(absoluteSlot(f->universe, f->address) + 1)); w.writeEndElement();
            w.writeEndElement();
            w.writeEndElement();
        }
        w.writeEndElement(); w.writeEndElement();
    }
    w.writeEndElement(); w.writeEndElement(); w.writeEndElement();
    w.writeEndDocument();
    return out;
}

bool exportFile(const QString& path, const Patch& patch, QString* error) {
    ZipWriter z;
    z.add("GeneralSceneDescription.xml", exportScene(patch));
    QSet<QString> done;
    for (auto& f : patch.fixtures) {
        const FixtureType* t = patch.typeOf(f);
        if (!t || done.contains(t->id)) continue;
        done.insert(t->id);
        ZipWriter g; g.add("description.xml", gdtf::exportDescription(gdtf::specFromFixtureType(*t)));
        z.add(t->manufacturer + "@" + t->name + ".gdtf", g.finish(), false);
    }
    if (!z.save(path)) { if (error) *error = "cannot write " + path; return false; }
    return true;
}

} // namespace lf::mvr
