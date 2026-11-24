#include <QtTest>
#include "pixelmapping/PixelMap.h"
#include "spatial/Spatial.h"
using namespace lf;

static Patch linePatch(const char* type, const char* mode, int n, int fp) {
    Patch p;
    for (int i = 0; i < n; ++i) { FixtureInstance f; f.fid = i + 1; f.typeId = type; f.modeName = mode; f.universe = 1; f.address = 1 + i * fp; f.position = Vec3(i, 0, 3); p.fixtures << f; }
    return p;
}

class TestMappingSpatial : public QObject {
    Q_OBJECT
private slots:
    void stripPixelAlignedUniverseSplit() {
        // 200 RGB pixels from universe 1 address 1: 170 fit (510 ch), pixel 170 starts universe 2 slot 0
        PixelMapElement e = PixelMap::makeStrip("s", {0, 0.5}, {1, 0.5}, 200, 1, 1);
        auto s169 = PixelMap::channelSlots(e, 169), s170 = PixelMap::channelSlots(e, 170);
        QCOMPARE(s169[2].first, 1); QCOMPARE(s169[2].second, 509);
        QCOMPARE(s170[0].first, 2); QCOMPARE(s170[0].second, 0);
        e.split = UniverseSplit::Continuous;
        auto c170 = PixelMap::channelSlots(e, 170);
        QCOMPARE(c170[0].first, 1); QCOMPARE(c170[0].second, 510); QCOMPARE(c170[2].first, 2); QCOMPARE(c170[2].second, 0);
    }
    void stripStartOffset() {
        PixelMapElement e = PixelMap::makeStrip("s", {0, 0.5}, {1, 0.5}, 10, 3, 508, PixelOrder::RGB);
        auto a = PixelMap::channelSlots(e, 0), b = PixelMap::channelSlots(e, 1);
        QCOMPARE(a[0].first, 3); QCOMPARE(a[0].second, 507);
        QCOMPARE(b[0].first, 4); QCOMPARE(b[0].second, 0);                 // never splits a pixel across universes
    }
    void matrixZigzag() {
        PixelMapElement e = PixelMap::makeMatrix("m", QRectF(0, 0, 1, 1), 4, 2, 1, 1, PixelOrder::RGB, true);
        QCOMPARE(e.points.size(), 8);
        QVERIFY(e.points[3].x() > e.points[0].x());                           // row 0 left -> right
        QVERIFY(qAbs(e.points[4].x() - e.points[3].x()) < 1e-9);              // row 1 starts on the right
        QVERIFY(e.points[4].y() > e.points[3].y());
        PixelMapElement br = PixelMap::makeMatrix("m", QRectF(0, 0, 1, 1), 4, 2, 1, 1, PixelOrder::RGB, false, PixelMap::StartCorner::BottomRight);
        QVERIFY(br.points[0].x() > 0.8 && br.points[0].y() > 0.5);
    }
    void renderStripColoursAndOrder() {
        QImage img(100, 10, QImage::Format_RGB32);
        for (int x = 0; x < 100; ++x) for (int y = 0; y < 10; ++y) img.setPixel(x, y, x < 50 ? qRgb(255, 0, 0) : qRgb(0, 0, 255));
        PixelMap pm; pm.globalGamma = 1.0; pm.sampling = Sampling::Nearest;
        pm.elements << PixelMap::makeStrip("s", {0.1, 0.5}, {0.9, 0.5}, 2, 1, 1, PixelOrder::GRB);
        QMap<int, DmxFrame> out; pm.render(img, Patch{}, out);
        QCOMPARE(int(out[1][0]), 0); QCOMPARE(int(out[1][1]), 255); QCOMPARE(int(out[1][2]), 0);   // pixel0 red in GRB = 0,255,0
        QCOMPARE(int(out[1][5]), 255);                                                            // pixel1 blue
        pm.masterBrightness = 0.5; out.clear(); pm.render(img, Patch{}, out);
        QCOMPARE(int(out[1][1]), 128);                                                            // brightness limiter
        pm.masterBrightness = 1.0; pm.globalGamma = 2.2; out.clear();
        QImage grey(10, 10, QImage::Format_RGB32); grey.fill(qRgb(128, 128, 128)); pm.render(grey, Patch{}, out);
        QVERIFY(int(out[1][0]) < 64);                                                             // gamma correction
    }
    void fixturePixelMapVirtualDimmer() {
        Patch p = linePatch("Generic@RGB Par", "3ch", 1, 3);
        PixelMap pm; pm.elements = PixelMap::makeFromPatch(p, PixelMap::Projection::Front);
        QCOMPARE(pm.elements.size(), 1);
        QImage img(10, 10, QImage::Format_RGB32); img.fill(qRgb(128, 64, 0));
        QMap<int, DmxFrame> out; pm.render(img, p, out);
        QCOMPARE(int(out[1][0]), 128); QCOMPARE(int(out[1][1]), 64); QCOMPARE(int(out[1][2]), 0);
    }
    void fixturePixelMapPhysicalDimmer() {
        Patch p = linePatch("Generic@RGBW Par Dim", "6ch", 1, 6);
        PixelMap pm; pm.elements = PixelMap::makeFromPatch(p, PixelMap::Projection::Front);
        QImage img(10, 10, QImage::Format_RGB32); img.fill(qRgb(128, 0, 0));
        QMap<int, DmxFrame> out; pm.render(img, p, out);
        QCOMPARE(int(out[1][0]), 128);   // dimmer carries intensity
        QCOMPARE(int(out[1][1]), 255);   // red at full: no double dimming
    }
    void conflictDetection() {
        PixelMap pm;
        pm.elements << PixelMap::makeStrip("a", {0, 0}, {1, 0}, 10, 1, 1) << PixelMap::makeStrip("b", {0, 1}, {1, 1}, 10, 1, 25);
        auto st = pm.analyze(Patch{});
        QCOMPARE(st.conflicts.size(), 6);                         // a uses 1..30, b uses 25..54 -> 25..30 overlap
        QCOMPARE(st.pixels, 20);
        pm.elements[1].address = 31;
        QVERIFY(pm.analyze(Patch{}).conflicts.isEmpty());
    }
    void testPatterns() {
        PixelMap pm; pm.elements << PixelMap::makeStrip("a", {0, 0}, {1, 0}, 3, 1, 1);
        QMap<int, DmxFrame> out; pm.renderTestPattern(1, 0, Patch{}, out);
        for (int i = 0; i < 9; ++i) QCOMPARE(int(out[1][i]), 255);
        QVERIFY(!pm.universesUsed(Patch{}).contains(2));
    }
    void spatialOrdering() {
        Patch p = linePatch("Generic@RGB Par", "3ch", 5, 3);
        SpatialEffect fx; fx.order = SpatialOrder::LeftToRight;
        auto ph = SpatialEngine::computePhases(p, fx);
        QCOMPARE(ph.size(), 5);
        for (int i = 1; i < 5; ++i) QVERIFY(ph[i].phase > ph[i - 1].phase);
        fx.order = SpatialOrder::RightToLeft; ph = SpatialEngine::computePhases(p, fx);
        QVERIFY(ph[0].phase > ph[4].phase);
        fx.order = SpatialOrder::CentreOut; ph = SpatialEngine::computePhases(p, fx);
        QCOMPARE(ph[0].phase, ph[4].phase); QCOMPARE(ph[1].phase, ph[3].phase); QVERIFY(ph[2].phase < ph[1].phase);
        fx.order = SpatialOrder::OutsideIn; ph = SpatialEngine::computePhases(p, fx);
        QVERIFY(ph[2].phase > ph[0].phase);
    }
    void spatialRandomReproducible() {
        Patch p = linePatch("Generic@RGB Par", "3ch", 10, 3);
        SpatialEffect fx; fx.order = SpatialOrder::Random; fx.seed = 42;
        auto a = SpatialEngine::computePhases(p, fx), b = SpatialEngine::computePhases(p, fx);
        for (int i = 0; i < a.size(); ++i) QCOMPARE(a[i].phase, b[i].phase);
        fx.seed = 43; auto c = SpatialEngine::computePhases(p, fx);
        bool differs = false; for (int i = 0; i < a.size(); ++i) differs |= a[i].phase != c[i].phase;
        QVERIFY(differs);
    }
    void spatialWingsBlocksExclusion() {
        Patch p = linePatch("Generic@RGB Par", "3ch", 8, 3);
        SpatialEffect fx; fx.wings = 2;
        auto ph = SpatialEngine::computePhases(p, fx);
        QCOMPARE(ph[0].phase, ph[7].phase); QCOMPARE(ph[3].phase, ph[4].phase);   // mirrored wings
        fx.wings = 1; fx.blocks = 2; ph = SpatialEngine::computePhases(p, fx);
        QCOMPARE(ph[0].phase, ph[1].phase); QVERIFY(ph[2].phase > ph[1].phase);
        fx.blocks = 1; fx.excluded = {3, 4}; QCOMPARE(SpatialEngine::computePhases(p, fx).size(), 6);
    }
    void spatialMixedCapabilities() {
        Patch p;
        FixtureInstance a; a.fid = 1; a.typeId = "Generic@RGB Par"; a.universe = 1; a.address = 1; a.position = Vec3(0, 0, 0);
        FixtureInstance b; b.fid = 2; b.typeId = "Generic@Dimmer"; b.universe = 1; b.address = 10; b.position = Vec3(1, 0, 0);
        FixtureInstance c; c.fid = 3; c.typeId = "Generic@RGBW Par Dim"; c.modeName = "6ch"; c.universe = 1; c.address = 20; c.position = Vec3(2, 0, 0);
        p.fixtures << a << b << c;
        SpatialEffect fx; fx.waveform = Waveform::Square; fx.width = 1.0; fx.low = 0.5; fx.high = 0.5; fx.colorA = {1, 0, 0}; fx.colorB = {1, 0, 0};
        QMap<int, DmxFrame> out; SpatialEngine::render(p, fx, 0.0, out);
        QCOMPARE(int(out[1][0]), 128);                // RGB par: virtual dimmer -> red at 50%
        QCOMPARE(int(out[1][9]), 128);                // dimmer-only: intensity effect
        QCOMPARE(int(out[1][19]), 128);               // dimmer channel at 50%
        QCOMPARE(int(out[1][20]), 255);               // red full (no double dimming)
    }
    void waveforms() {
        QCOMPARE(SpatialEngine::waveform(Waveform::Square, 0.2, 0.5), 1.0);
        QCOMPARE(SpatialEngine::waveform(Waveform::Square, 0.7, 0.5), 0.0);
        QVERIFY(qAbs(SpatialEngine::waveform(Waveform::Sine, 0.5, 0.5) - 1.0) < 1e-9);
        QCOMPARE(SpatialEngine::waveform(Waveform::Ramp, 1.25, 0.5), 0.25);
    }
};
QTEST_GUILESS_MAIN(TestMappingSpatial)
#include "test_mapping_spatial.moc"
