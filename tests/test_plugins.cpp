#include <QtTest>
#include "plugins/PluginHost.h"
#include "core/Log.h"
using namespace lf;

class TestPlugins : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Log::instance().setEcho(false); }
    void loadExampleOutOfProcess() {
        PluginHost host;
        host.setPluginDirs({LF_PLUGIN_DIR});
        host.setProbeExecutable(QString(LF_PLUGIN_DIR) + "/../../../src/lumaflow-cli");
        EffectLibrary lib;
        const auto recs = host.scanAndLoad(&lib, false);
        QCOMPARE(recs.size(), 1);
        QCOMPARE(recs[0].id, QString("io.lumaflow.example.starfield"));
        QCOMPARE(recs[0].status, QString("ok"));
        QVERIFY(recs[0].loaded);
        QCOMPARE(recs[0].effects, 2);
        QVERIFY(lib.contains("example.starfield"));
        QCOMPARE(lib.get("example.starfield").source, QString("plugin:io.lumaflow.example.starfield"));
    }
    void disabledAndSafeMode() {
        PluginHost host; host.setPluginDirs({LF_PLUGIN_DIR});
        host.setDisabled({"io.lumaflow.example.starfield"});
        EffectLibrary lib; auto recs = host.scanAndLoad(&lib, false);
        QCOMPARE(recs[0].status, QString("disabled")); QVERIFY(!lib.contains("example.starfield"));
        PluginHost h2; h2.setPluginDirs({LF_PLUGIN_DIR}); EffectLibrary l2;
        recs = h2.scanAndLoad(&l2, true); QVERIFY(recs[0].status.contains("safe mode")); QVERIFY(!l2.contains("example.starfield"));
    }
    void crashingPluginIsolated() {
        PluginHost host;
        host.setPluginDirs({QString(LF_PLUGIN_DIR) + "/../crashtest"});
        host.setProbeExecutable(QString(LF_PLUGIN_DIR) + "/../../../src/lumaflow-cli");
        EffectLibrary lib;
        const auto recs = host.scanAndLoad(&lib, false);       // must not crash this process
        QCOMPARE(recs.size(), 1);
        QVERIFY2(recs[0].status.contains("crash"), qPrintable(recs[0].status));
        QVERIFY(!recs[0].loaded);
    }
};
QTEST_GUILESS_MAIN(TestPlugins)
#include "test_plugins.moc"
