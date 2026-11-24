#pragma once
#include <QMainWindow>
#include <QHash>
#include <QThread>
#include <QImage>
#include <functional>

class QLabel; class QDockWidget; class QAction;

namespace lf {
class Engine; class UiContext; class LibraryPanel; class PreviewPanel; class LayerPanel; class ParameterPanel; class ShowPanel;
class TimelinePanel; class NodeEditorPanel; class FixturePanel; class PixelMapperPanel; class StagePanel; class SpatialPanel; class ConsolePanel;
class OutputPanel; class MediaPanel; class AudioPanel; class CalibrationPanel; class RemoteSyncPanel; class PluginPanel; class AiPanel; class PerformancePanel; class RecoveryPanel;

// Renders animated effect thumbnails on a background thread with its own GL context.
class ThumbnailWorker : public QThread {
    Q_OBJECT
public:
    ThumbnailWorker(Engine* e, const QString& cacheDir) : m_e(e), m_dir(cacheDir) {}
signals:
    void ready(const QHash<QString, QVector<QImage>>& thumbs);
protected:
    void run() override;
private:
    Engine* m_e; QString m_dir;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Engine* e, QWidget* parent = nullptr);
    ~MainWindow() override;
    void applyWorkspace(const QString& name);
    QDockWidget* dock(const QString& name) const { return m_docks.value(name); }
    UiContext* context() const { return m_ctx; }
    QStringList dockNames() const { return m_docks.keys(); }
    bool openProject(const QString& path);
    void checkCrashRecovery();
    void setAutomation(bool on) { m_automation = on; }
protected:
    void closeEvent(QCloseEvent*) override;
private:
    QDockWidget* addPanel(const QString& name, QWidget* w, Qt::DockWidgetArea area);
    void buildMenus();
    void updateStatus();
    bool maybeSave();
    void saveAs();
    void updateTitle();
    Engine* m_e; UiContext* m_ctx;
    QHash<QString, QDockWidget*> m_docks;
    LibraryPanel* m_library = nullptr; PreviewPanel* m_preview = nullptr;
    QLabel *m_stFps, *m_stNet, *m_stTc, *m_stFaults, *m_stMsg;
    ThumbnailWorker* m_thumbs = nullptr;
    QStringList m_recent;
    bool m_automation = false;
};

} // namespace lf
