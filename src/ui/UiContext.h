#pragma once
#include <QObject>
#include <QUndoStack>

namespace lf {
class Engine;
// Shared UI state (selected layer, preview-edit mode) and the undo stack.
class UiContext : public QObject {
    Q_OBJECT
public:
    UiContext(Engine* e, QObject* parent = nullptr) : QObject(parent), engine(e) {}
    Engine* engine;
    QUndoStack undo;
    int layer() const { return m_layer; }
    void setLayer(int l) { if (l != m_layer) { m_layer = l; emit layerChanged(l); } }
    bool editPreview() const { return m_preview; }
    void setEditPreview(bool p) { m_preview = p; emit layerChanged(m_layer); }
signals:
    void layerChanged(int layer);
    void status(const QString& msg);
private:
    int m_layer = 0; bool m_preview = false;
};
}
