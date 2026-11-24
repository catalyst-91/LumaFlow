#include <QtTest>
#include "console/Personality.h"
#include "console/Mapping.h"
#include "layers/Composition.h"
using namespace lf;

class TestConsole : public QObject {
    Q_OBJECT
    ParameterRegistry reg;
    QStringList effects{"Plasma", "Vortex", "Rings"};
private slots:
    void initTestCase() { Composition::defineParams(reg, 4, 3, 2); }
    void modesArePrefixes() {
        const auto b = PersonalityGenerator::generate(reg, PersonalityMode::Basic16, effects, {"A"});
        const auto s = PersonalityGenerator::generate(reg, PersonalityMode::Standard32, effects, {"A"});
        const auto x = PersonalityGenerator::generate(reg, PersonalityMode::Extended64, effects, {"A"});
        const auto a = PersonalityGenerator::generate(reg, PersonalityMode::Advanced128, effects, {"A"});
        QVERIFY(b.footprint() <= 16 && b.footprint() >= 15);
        QVERIFY(s.footprint() <= 32 && s.footprint() >= 30);
        QVERIFY(x.footprint() <= 64 && x.footprint() >= 60);
        QVERIFY(a.footprint() <= 128 && a.footprint() > 64);
        for (int i = 0; i < b.channels.size(); ++i) { QCOMPARE(s.channels[i].paramId, b.channels[i].paramId); QCOMPARE(s.channels[i].offset, b.channels[i].offset); }
        for (int i = 0; i < x.channels.size(); ++i) QCOMPARE(a.channels[i].offset, x.channels[i].offset);
        QCOMPARE(b.channels[0].paramId, QString("master.intensity")); QCOMPARE(b.channels[0].fineOffset, 2);
        QCOMPARE(b.channelFor("layer1.effect")->indexNames, effects);
        // no channel overlaps
        QSet<int> used; for (auto& c : a.channels) { QVERIFY(!used.contains(c.offset)); used.insert(c.offset); if (c.fineOffset) { QVERIFY(!used.contains(c.fineOffset)); used.insert(c.fineOffset); } }
    }
    void customMode() {
        const auto p = PersonalityGenerator::generate(reg, PersonalityMode::Custom, effects, {}, {"layer2.speed", "master.intensity"});
        QCOMPARE(p.channels.size(), 2); QCOMPARE(p.channels[0].paramId, QString("layer2.speed")); QCOMPARE(p.footprint(), 3);
    }
    void gdtfExportValid() {
        QVector<PersonalityProfile> ms;
        for (auto m : {PersonalityMode::Basic16, PersonalityMode::Standard32, PersonalityMode::Extended64, PersonalityMode::Advanced128}) ms << PersonalityGenerator::generate(reg, m, effects, {"A"});
        const QByteArray xml = gdtf::exportDescription(PersonalityGenerator::toGdtfSpec(ms));
        QVERIFY2(gdtf::validateDescription(xml).isEmpty(), qPrintable(gdtf::validateDescription(xml).join("; ")));
        const auto back = gdtf::importDescription(xml, "t");
        QVERIFY(back.ok()); QCOMPARE(back.type.modes.size(), 4);
        for (int i = 0; i < 4; ++i) QCOMPARE(back.type.modes[i].footprint(), ms[i].footprint());
    }
    void diffAndApproval() {
        ProfileManager pm;
        auto v1 = PersonalityGenerator::generate(reg, PersonalityMode::Basic16, effects, {"A"});
        pm.propose(v1); QVERIFY(pm.approve(PersonalityMode::Basic16, false));
        QCOMPARE(pm.active(PersonalityMode::Basic16).version, 1);
        // identical regeneration: nothing pending
        QVERIFY(pm.propose(v1).empty()); QVERIFY(!pm.hasPending(PersonalityMode::Basic16));
        // rename only -> non breaking, still needs approval
        auto v2 = v1; v2.channels[3].name = "Renamed";
        auto d = pm.propose(v2); QVERIFY(!d.empty()); QVERIFY(!d.breaking());
        QCOMPARE(pm.active(PersonalityMode::Basic16).channels[3].name, v1.channels[3].name);   // not silently applied
        QVERIFY(pm.approve(PersonalityMode::Basic16, false)); QCOMPARE(pm.active(PersonalityMode::Basic16).version, 2);
        // adding an effect changes index ranges -> breaking -> refused without confirmation
        auto v3 = PersonalityGenerator::generate(reg, PersonalityMode::Basic16, effects + QStringList{"New"}, {"A"});
        d = pm.propose(v3); QVERIFY(d.breaking());
        QString why; QVERIFY(!pm.approve(PersonalityMode::Basic16, false, &why)); QVERIFY(why.contains("breaking"));
        QVERIFY(pm.approve(PersonalityMode::Basic16, true)); QCOMPARE(pm.active(PersonalityMode::Basic16).version, 3);
        QVERIFY(pm.rollback(PersonalityMode::Basic16)); QCOMPARE(pm.active(PersonalityMode::Basic16).version, 2);
        // persistence
        ProfileManager pm2; pm2.fromJson(pm.toJson()); QCOMPARE(pm2.active(PersonalityMode::Basic16).version, 2); QCOMPARE(pm2.history(PersonalityMode::Basic16).size(), 1);
    }
    void moveIsBreaking() {
        auto a = PersonalityGenerator::generate(reg, PersonalityMode::Basic16, effects, {"A"});
        auto b = a; std::swap(b.channels[3].offset, b.channels[4].offset);
        QVERIFY(PersonalityGenerator::compare(a, b).breaking());
        auto c = a; c.channels.removeLast(); QVERIFY(PersonalityGenerator::compare(a, c).breaking());
        auto d2 = PersonalityGenerator::generate(reg, PersonalityMode::Standard32, effects, {"A"});
        QVERIFY(!PersonalityGenerator::compare(a, d2).breaking());     // appended channels beyond footprint are safe
    }
    void decodeDmx() {
        auto p = PersonalityGenerator::generate(reg, PersonalityMode::Basic16, effects, {"A"});
        quint8 dmx[16] = {0};
        dmx[0] = 255; dmx[1] = 255; dmx[2] = 2 * 8 + 3;   // intensity full, effect index 2
        auto v = PersonalityGenerator::decodeDmx(p, dmx, 16);
        QCOMPARE(v[0], 1.0); QCOMPARE(v[1], 2.0);
    }
    void mappingScalingInvertDeadband() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r);
        ControlMapping c; c.source = ControlSource::Osc; c.address = "/fader"; c.target = "layer1.speed"; c.inMin = 0; c.inMax = 100; c.outMin = 0; c.outMax = 0.5;
        m.add(c);
        QCOMPARE(m.handleOsc({"/fader", {50}}), 1);
        QVERIFY(qAbs(r.value("layer1.speed") - 4 * 0.25) < 1e-9);    // 50% of 0..0.5 normalised -> 0.25*range(0..4)=1.0
        ControlMapping inv = c; inv.id.clear(); inv.address = "/inv"; inv.target = "layer1.opacity"; inv.invert = true; inv.outMax = 1; inv.deadband = 0.05;
        m.add(inv);
        m.handleOsc({"/inv", {25}}); QVERIFY(qAbs(r.value("layer1.opacity") - 0.75) < 1e-9);
        QCOMPARE(m.handleOsc({"/inv", {27}}), 0);                    // within deadband
        QCOMPARE(m.handleOsc({"/inv", {40}}), 1);
    }
    void mappingSmoothingAndSoftTakeover() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r);
        ControlMapping c; c.source = ControlSource::Midi; c.address = "cc:0:1"; c.target = "layer1.p1"; c.softTakeover = true;
        m.add(c);
        r.set("layer1.p1", 0.5);
        midi::Message mm; mm.type = midi::Type::ControlChange; mm.data1 = 1;
        mm.data2 = 10; QCOMPARE(m.handleMidi(mm), 0);                // far from 0.5: not picked up
        mm.data2 = 30; QCOMPARE(m.handleMidi(mm), 0);
        QCOMPARE(r.value("layer1.p1"), 0.5);
        mm.data2 = 70; QCOMPARE(m.handleMidi(mm), 1);                // crossed 0.5 -> pickup
        QVERIFY(qAbs(r.value("layer1.p1") - 70 / 127.0) < 1e-9);
    }
    void edgeTriggers() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r);
        int fired = 0; m.setActionHandler([&](const QString& a, double, ControlSource) { if (a == "cue.go") ++fired; });
        ControlMapping c; c.source = ControlSource::Dmx; c.address = "1/10"; c.target = "action:cue.go"; c.edge = TriggerEdge::Rising; c.threshold = 0.5;
        m.add(c);
        quint8 dmx[512] = {0};
        m.handleDmx(1, dmx, 512); dmx[9] = 200; m.handleDmx(1, dmx, 512); m.handleDmx(1, dmx, 512);
        QCOMPARE(fired, 1);
        dmx[9] = 0; m.handleDmx(1, dmx, 512); dmx[9] = 255; m.handleDmx(1, dmx, 512);
        QCOMPARE(fired, 2);
        m.handleDmx(2, dmx, 512); QCOMPARE(fired, 2);                // other universe ignored
    }
    void ownershipPolicies() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r); m.setPriorityHoldMs(10000);
        m.setPolicy("layer1.p1", OwnershipPolicy::ConsolePriority);
        QVERIFY(m.write("layer1.p1", 0.2, ControlSource::Dmx));
        QVERIFY(!m.write("layer1.p1", 0.9, ControlSource::Touch));    // console holds it
        QCOMPARE(r.value("layer1.p1"), 0.2);
        m.setPolicy("layer1.p2", OwnershipPolicy::TouchPriority);
        QVERIFY(m.write("layer1.p2", 0.3, ControlSource::Touch));
        QVERIFY(!m.write("layer1.p2", 0.8, ControlSource::Osc));
        m.setPolicy("layer1.p3", OwnershipPolicy::Locked, ControlSource::Remote);
        QVERIFY(!m.write("layer1.p3", 0.1, ControlSource::Touch)); QVERIFY(m.write("layer1.p3", 0.1, ControlSource::Remote));
        QVERIFY(m.write("layer1.p4", 0.1, ControlSource::Touch)); QVERIFY(m.write("layer1.p4", 0.2, ControlSource::Osc));   // last change wins
        // policies persist
        MappingEngine m2(&r); m2.fromJson(m.toJson()); QCOMPARE(int(m2.policy("layer1.p3")), int(OwnershipPolicy::Locked));
    }
    void feedbackLoopPrevention() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r);
        int sent = 0; m.setFeedbackSender([&](const ControlMapping&, double) { ++sent; });
        r.addListener([&](const QString& id, double v, const QString& src) { m.onParameterChanged(id, v, src); });
        ControlMapping c; c.source = ControlSource::Osc; c.address = "/f"; c.target = "layer1.p1"; m.add(c);
        m.handleOsc({"/f", {0.3}});
        QCOMPARE(sent, 0);                                            // value came from this mapping: no echo
        r.set("layer1.p1", 0.8, "touch");
        QCOMPARE(sent, 1);                                            // changed elsewhere: feedback sent
        r.set("layer1.p1", 0.8, "touch"); QCOMPARE(sent, 1);
    }
    void learn() {
        ParameterRegistry r; Composition::defineParams(r, 1, 3, 1);
        MappingEngine m(&r);
        m.startLearn("layer1.scale", "grandMA3"); QVERIFY(m.learning());
        midi::Message mm; mm.type = midi::Type::ControlChange; mm.channel = 2; mm.data1 = 21; mm.data2 = 64;
        m.handleMidi(mm); QVERIFY(!m.learning());
        QCOMPARE(m.mappings().size(), 1); QCOMPARE(m.mappings()[0].address, QString("cc:2:21")); QCOMPARE(m.mappings()[0].console, QString("grandMA3"));
        mm.data2 = 127; m.handleMidi(mm); QCOMPARE(r.value("layer1.scale"), 4.0);
    }
};
QTEST_GUILESS_MAIN(TestConsole)
#include "test_console.moc"
