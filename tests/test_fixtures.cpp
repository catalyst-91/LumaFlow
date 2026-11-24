#include <QtTest>
#include "fixtures/Fixture.h"
#include "fixtures/Gdtf.h"
#include "fixtures/Mvr.h"
#include "fixtures/ColorEngine.h"
#include "storage/Zip.h"
using namespace lf;

static const char* kMultiCellGdtf = R"(<?xml version="1.0"?>
<GDTF DataVersion="1.2"><FixtureType Name="Bar4" ShortName="B4" LongName="Bar 4" Manufacturer="Test" Description="" FixtureTypeID="11111111-2222-3333-4444-555555555555" Thumbnail="" RefFT="">
<AttributeDefinitions><Attributes><Attribute Name="Dimmer"/><Attribute Name="ColorAdd_R"/><Attribute Name="ColorAdd_G"/><Attribute Name="ColorAdd_B"/><Attribute Name="Pan"/></Attributes></AttributeDefinitions>
<Geometries><Geometry Name="Base" Position="{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}">
 <Geometry Name="Pixel" Position="{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}"/>
 <GeometryReference Name="P1" Geometry="Pixel" Position="{1,0,0,-300}{0,1,0,0}{0,0,1,0}{0,0,0,1}"><Break DMXOffset="2" DMXBreak="1"/></GeometryReference>
 <GeometryReference Name="P2" Geometry="Pixel" Position="{1,0,0,-100}{0,1,0,0}{0,0,1,0}{0,0,0,1}"><Break DMXOffset="5" DMXBreak="1"/></GeometryReference>
 <GeometryReference Name="P3" Geometry="Pixel" Position="{1,0,0,100}{0,1,0,0}{0,0,1,0}{0,0,0,1}"><Break DMXOffset="8" DMXBreak="1"/></GeometryReference>
 <GeometryReference Name="P4" Geometry="Pixel" Position="{1,0,0,300}{0,1,0,0}{0,0,1,0}{0,0,0,1}"><Break DMXOffset="11" DMXBreak="1"/></GeometryReference>
</Geometry></Geometries>
<DMXModes><DMXMode Name="13ch" Geometry="Base"><DMXChannels>
 <DMXChannel DMXBreak="1" Offset="1" Geometry="Base"><LogicalChannel Attribute="Dimmer"><ChannelFunction Attribute="Dimmer" Default="0/1"/></LogicalChannel></DMXChannel>
 <DMXChannel DMXBreak="1" Offset="1" Geometry="Pixel"><LogicalChannel Attribute="ColorAdd_R"><ChannelFunction Attribute="ColorAdd_R" Default="0/1"/></LogicalChannel></DMXChannel>
 <DMXChannel DMXBreak="1" Offset="2" Geometry="Pixel"><LogicalChannel Attribute="ColorAdd_G"><ChannelFunction Attribute="ColorAdd_G" Default="0/1"/></LogicalChannel></DMXChannel>
 <DMXChannel DMXBreak="1" Offset="3" Geometry="Pixel"><LogicalChannel Attribute="ColorAdd_B"><ChannelFunction Attribute="ColorAdd_B" Default="0/1"/></LogicalChannel></DMXChannel>
</DMXChannels></DMXMode>
<DMXMode Name="16bit" Geometry="Base"><DMXChannels>
 <DMXChannel DMXBreak="1" Offset="1,2" Geometry="Base"><LogicalChannel Attribute="Pan"><ChannelFunction Attribute="Pan" Default="32768/2"/></LogicalChannel></DMXChannel>
</DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>)";

class TestFixtures : public QObject {
    Q_OBJECT
private slots:
    void capabilities() {
        const auto& lib = FixtureLibrary::builtin();
        auto caps = [&](const char* id) { return analyzeCapabilities(lib.find(id)->modes.first()); };
        QVERIFY(caps("Generic@RGB Par").needsVirtualDimmer());
        QCOMPARE(int(caps("Generic@RGBW Par").color), int(ColorModel::RGBW));
        QCOMPARE(int(caps("Generic@RGBWAUV Par").color), int(ColorModel::RGBWAUV));
        QVERIFY(caps("Generic@RGBW Par Dim").dimmer && !caps("Generic@RGBW Par Dim").needsVirtualDimmer());
        const Capabilities spot = caps("Generic@Moving Spot");
        QVERIFY(spot.pan && spot.tilt && spot.gobo && spot.prism && spot.zoom && spot.focus && spot.iris && spot.shutter);
        QCOMPARE(caps("Generic@Pixel Strip 170").cells, 170);
        QVERIFY(!caps("Generic@Dimmer").hasColor());
    }
    void dmx16bitAndDefaults() {
        const FixtureType* t = FixtureLibrary::builtin().find("Generic@Moving Spot");
        FixtureInstance f; f.universe = 1; f.address = 11;
        AttributeValues v; v.perAttribute[int(Attribute::Pan)] = 1.0; v.perAttribute[int(Attribute::Tilt)] = 0.5;
        QMap<int, DmxFrame> u; writeFixtureDmx(f, t->modes.first(), v, u);
        QCOMPARE(int(u[1][10]), 255); QCOMPARE(int(u[1][11]), 255);              // pan coarse/fine
        QCOMPARE(int(u[1][12]), 128); QCOMPARE(int(u[1][13]), 0);                // tilt 0.5 -> 32768
        QCOMPARE(int(u[1][16]), 255);                                           // shutter default open
    }
    void fixtureSpanningUniverse() {
        const FixtureType* t = FixtureLibrary::builtin().find("Generic@RGBW Par");
        FixtureInstance f; f.universe = 1; f.address = 511;
        AttributeValues v; v.perAttribute[int(Attribute::Red)] = 1; v.perAttribute[int(Attribute::White)] = 1;
        QMap<int, DmxFrame> u; writeFixtureDmx(f, t->modes.first(), v, u);
        QCOMPARE(int(u[1][510]), 255); QCOMPARE(int(u[2][1]), 255);             // white lands on universe 2 slot 2
    }
    void patchValidation() {
        Patch p;
        FixtureInstance a; a.fid = 1; a.typeId = "Generic@RGBW Par"; a.universe = 1; a.address = 1;
        FixtureInstance b = a; b.fid = 2; b.address = 3;                          // overlaps a (1..4)
        FixtureInstance c = a; c.fid = 3; c.address = 510;                        // crosses boundary
        FixtureInstance d = a; d.fid = 4; d.typeId = "nope";
        p.fixtures << a << b << c << d;
        const auto issues = p.validate();
        bool overlap = false, cross = false, missing = false;
        for (auto& i : issues) { overlap |= i.kind == PatchIssue::Overlap; cross |= i.kind == PatchIssue::CrossesUniverse; missing |= i.kind == PatchIssue::MissingType; }
        QVERIFY(overlap && cross && missing);
        p.fixtures.removeAt(1); p.fixtures.removeAt(1); p.fixtures.removeLast();
        QCOMPARE(p.nextFreeAddress(1, 4), 5);
        QVERIFY(p.validate().isEmpty());
    }
    void zipRoundTrip() {
        ZipWriter w; QByteArray big(100000, 'x'); w.add("a.txt", "hello"); w.add("dir/big.bin", big);
        ZipReader r; QVERIFY(r.openData(w.finish()));
        QCOMPARE(r.read("a.txt"), QByteArray("hello")); bool ok; QCOMPARE(r.read("dir/big.bin", &ok), big); QVERIFY(ok);
        QVERIFY(!r.openData("not a zip"));
    }
    void gdtfMultiInstanceImport() {
        const auto r = gdtf::importDescription(kMultiCellGdtf, "test");
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.type.modes.size(), 2);
        const FixtureMode& m = r.type.modes[0];
        QCOMPARE(m.cellCount, 4);
        QCOMPARE(m.footprint(), 13);
        const Capabilities c = analyzeCapabilities(m);
        QVERIFY(c.dimmer); QCOMPARE(int(c.color), int(ColorModel::RGB)); QCOMPARE(c.cells, 4);
        // cell 3 red at DMX offset 8 (1-based) -> 7 (0-based)
        bool found = false; for (auto& ch : m.channels) if (ch.cell == 2 && ch.attribute == Attribute::Red) { QCOMPARE(ch.offset, 7); found = true; }
        QVERIFY(found);
        QVERIFY(qAbs(m.cellOffsets[0].x + 0.3) < 1e-9);
        const FixtureMode& m2 = r.type.modes[1];
        QCOMPARE(m2.channels[0].fineOffset, 1); QCOMPARE(m2.channels[0].defaultValue, 128);
    }
    void gdtfExportRoundTrip() {
        const FixtureType* t = FixtureLibrary::builtin().find("Generic@Moving Wash");
        const auto spec = gdtf::specFromFixtureType(*t);
        const QByteArray xml = gdtf::exportDescription(spec);
        QVERIFY2(gdtf::validateDescription(xml).isEmpty(), qPrintable(gdtf::validateDescription(xml).join("; ")));
        const QString path = QDir::temp().filePath("lf_test.gdtf");
        QString err; QVERIFY2(gdtf::exportFile(path, spec, &err), qPrintable(err));
        const auto r = gdtf::importFile(path);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.type.modes.first().footprint(), t->modes.first().footprint());
        const Capabilities a = analyzeCapabilities(t->modes.first()), b = analyzeCapabilities(r.type.modes.first());
        QCOMPARE(a.describe(), b.describe());
    }
    void gdtfValidationCatchesErrors() {
        QByteArray bad = gdtf::exportDescription(gdtf::specFromFixtureType(*FixtureLibrary::builtin().find("Generic@RGB Par")));
        bad.replace("Offset=\"2\"", "Offset=\"1\"");
        QVERIFY(!gdtf::validateDescription(bad).isEmpty());
    }
    void mvrMatrix() {
        const Mat4 m = mvr::parseMatrix("{1,0,0}{0,1,0}{0,0,1}{1000,2000,3000}");
        QCOMPARE(m.translationPart().x, 1000.0); QCOMPARE(m.translationPart().z, 3000.0);
        const Vec3 rot(10, 20, 30);
        const Mat4 back = mvr::parseMatrix(mvr::formatMatrix(Vec3(1, 2, 3), rot));
        const Vec3 e = back.eulerXYZDeg();
        QVERIFY(qAbs(e.x - 10) < 1e-6 && qAbs(e.y - 20) < 1e-6 && qAbs(e.z - 30) < 1e-6);
    }
    void mvrRoundTrip() {
        Patch p;
        for (int i = 0; i < 5; ++i) { FixtureInstance f; f.fid = 100 + i; f.name = QString("F%1").arg(i); f.typeId = "Generic@RGBW Par Dim"; f.modeName = "6ch"; f.universe = 2; f.address = 1 + i * 6; f.position = Vec3(i * 1.5 - 3, 2, 5.5); f.rotation = Vec3(0, 0, i * 10); f.layer = "Truss"; p.fixtures << f; }
        const QString path = QDir::temp().filePath("lf_test.mvr");
        QString err; QVERIFY2(mvr::exportFile(path, p, &err), qPrintable(err));
        const auto r = mvr::importFile(path);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.patch.fixtures.size(), 5);
        for (int i = 0; i < 5; ++i) {
            const auto& f = r.patch.fixtures[i];
            QCOMPARE(f.fid, 100 + i); QCOMPARE(f.universe, 2); QCOMPARE(f.address, 1 + i * 6);
            QVERIFY(qAbs(f.position.x - (i * 1.5 - 3)) < 1e-6); QVERIFY(qAbs(f.position.z - 5.5) < 1e-6);
            QVERIFY(qAbs(f.rotation.z - i * 10) < 1e-6);
            QVERIFY(r.patch.modeOf(f) != nullptr);                              // type came from embedded GDTF
        }
    }
    void mvrMissingDataWarnings() {
        const QByteArray xml = R"(<GeneralSceneDescription verMajor="1" verMinor="6"><Scene><Layers><Layer name="L"><ChildList>
            <Fixture name="NoAddr" uuid="A"><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix><GDTFSpec>Missing.gdtf</GDTFSpec><GDTFMode>M</GDTFMode></Fixture>
            </ChildList></Layer></Layers></Scene></GeneralSceneDescription>)";
        const auto r = mvr::importScene(xml, {}, {});
        QVERIFY(r.ok());
        QCOMPARE(r.patch.fixtures.size(), 1);
        QVERIFY(r.warnings.join('\n').contains("no FixtureID"));
        QVERIFY(r.warnings.join('\n').contains("unpatched"));
        QVERIFY(r.warnings.join('\n').contains("not embedded"));
    }
    void colourConversion() {
        Capabilities rgbw; rgbw.color = ColorModel::RGBW;
        EmitterValues e = ColorEngine::convert({1, 1, 1}, 1.0, rgbw);
        QCOMPARE(e.w, 1.0); QCOMPARE(e.r, 0.0);                                // white extraction
        e = ColorEngine::convert({1, 0.5, 0.5}, 1.0, rgbw);
        QCOMPARE(e.w, 0.5); QCOMPARE(e.r, 0.5); QCOMPARE(e.g, 0.0);
        // virtual dimmer scales emitters
        Capabilities rgb; rgb.color = ColorModel::RGB;
        e = ColorEngine::convert({1, 0.5, 0}, 0.5, rgb);
        QCOMPARE(e.r, 0.5); QCOMPARE(e.g, 0.25); QCOMPARE(e.dimmer, 1.0);
        // physical dimmer: no double dimming
        Capabilities dim = rgb; dim.dimmer = true;
        e = ColorEngine::convert({1, 0.5, 0}, 0.5, dim);
        QCOMPARE(e.r, 1.0); QCOMPARE(e.g, 0.5); QCOMPARE(e.dimmer, 0.5);
        // intensity only
        Capabilities none; none.dimmer = true;
        e = ColorEngine::convert({1, 1, 1}, 0.7, none); QVERIFY(qAbs(e.dimmer - 0.7) < 1e-9);
        // limiter + gamma
        ColorCalibration cal; cal.maxBrightness = 0.5; cal.gamma = 2.0;
        e = ColorEngine::convert({1, 0, 0}, 1.0, rgb, cal); QVERIFY(qAbs(e.r - std::sqrt(0.5)) < 1e-9);
        const RGB warm = ColorEngine::fromKelvin(2700), cool = ColorEngine::fromKelvin(9000);
        QVERIFY(warm.r > warm.b && cool.b > warm.b);
    }
};
QTEST_GUILESS_MAIN(TestFixtures)
#include "test_fixtures.moc"
