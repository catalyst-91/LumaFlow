#pragma once
#include "UiContext.h"
#include "nodes/NodeGraph.h"
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsItem>
class QComboBox; class QLabel; class QTreeWidget; class QPlainTextEdit; class QCheckBox; class QLineEdit;

namespace lf {

// ---------------------------------------------------------------- Timeline
class TimelineCanvas : public QWidget {
    Q_OBJECT
public:
    TimelineCanvas(UiContext* ctx, QWidget* parent = nullptr);
    int selectedTrack() const { return m_selTrack; }
    void setZoom(double pxPerSec) { m_pps = pxPerSec; update(); }
    bool snap = true;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    double timeAt(double x) const { return (x - 140 + m_scroll) / m_pps; }
    double xAt(double t) const { return 140 + t * m_pps - m_scroll; }
    UiContext* c; double m_pps = 20, m_scroll = 0; int m_selTrack = 0, m_dragKey = -1; bool m_scrub = false;
};

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    TimelinePanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    UiContext* c; TimelineCanvas* m_canvas; QLabel* m_pos; QComboBox* m_param;
};

// ---------------------------------------------------------------- Node editor
class NodeScene;
class NodeEditorPanel : public QWidget {
    Q_OBJECT
public:
    NodeEditorPanel(UiContext* ctx, QWidget* parent = nullptr);
    NodeGraph& graph() { return m_graph; }
    void rebuildScene();
    void compileAndApply(bool toProgram);
private:
    UiContext* c; NodeGraph m_graph; NodeScene* m_scene; QGraphicsView* m_view; QTreeWidget* m_palette;
    QPlainTextEdit* m_log; QLineEdit* m_name; QJsonObject m_clipboard; QCheckBox* m_live; quint64 m_lastHash = 0;
};

} // namespace lf
