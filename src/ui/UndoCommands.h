#pragma once
// Undo/redo commands. Parameter edits merge per parameter while dragging;
// structural edits store project-state snapshots before/after.
#include <QUndoCommand>
#include <QJsonObject>
#include <functional>

namespace lf {
class Engine;

class ParamCommand : public QUndoCommand {
public:
    ParamCommand(Engine* e, const QString& id, double before, double after);
    void undo() override;
    void redo() override;
    int id() const override { return 1001; }
    bool mergeWith(const QUndoCommand* other) override;
private:
    Engine* m_e; QString m_param; double m_before, m_after; bool m_first = true;
};

class SnapshotCommand : public QUndoCommand {
public:
    SnapshotCommand(Engine* e, const QString& text, const QJsonObject& before, const QJsonObject& after);
    void undo() override;
    void redo() override;
private:
    Engine* m_e; QJsonObject m_before, m_after; bool m_first = true;
};

// Helper: runs `fn`, capturing project state before/after into one undo step.
void undoable(Engine* e, class QUndoStack* stack, const QString& text, const std::function<void()>& fn);

} // namespace lf
