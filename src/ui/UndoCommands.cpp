#include "UndoCommands.h"
#include "app/Engine.h"
#include <QUndoStack>

namespace lf {

ParamCommand::ParamCommand(Engine* e, const QString& id, double b, double a) : m_e(e), m_param(id), m_before(b), m_after(a) {
    setText(QString("Change %1").arg(e->params().def(id).name));
}
void ParamCommand::undo() { m_e->params().set(m_param, m_before, "undo"); }
void ParamCommand::redo() { if (m_first) { m_first = false; return; } m_e->params().set(m_param, m_after, "undo"); }
bool ParamCommand::mergeWith(const QUndoCommand* o) {
    auto* c = static_cast<const ParamCommand*>(o);
    if (c->m_param != m_param) return false;
    m_after = c->m_after; return true;
}

SnapshotCommand::SnapshotCommand(Engine* e, const QString& t, const QJsonObject& b, const QJsonObject& a) : m_e(e), m_before(b), m_after(a) { setText(t); }
void SnapshotCommand::undo() { m_e->fromProjectJson(m_before); }
void SnapshotCommand::redo() { if (m_first) { m_first = false; return; } m_e->fromProjectJson(m_after); }

void undoable(Engine* e, QUndoStack* stack, const QString& text, const std::function<void()>& fn) {
    const QJsonObject before = e->toProjectJson();
    fn();
    if (stack) stack->push(new SnapshotCommand(e, text, before, e->toProjectJson()));
}

} // namespace lf
