#pragma once
// Node-based visual effect graphs. Graphs are validated (types, cycles,
// dangling connections) and compiled to a GLSL effect function so they run on
// the same GPU path as built-in effects. Compilation produces a cached
// execution plan (topologically ordered GLSL); parameter changes are uniforms
// and never trigger recompilation.
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QPointF>

namespace lf {

enum class PortType { Float, Vec2, Color };
QString portTypeName(PortType t);

struct PortDef { QString name; PortType type; double defaultValue = 0.0; };
struct NodeParamDef { QString name; double defaultValue = 0.0; double min = 0.0, max = 1.0; };

struct NodeTypeDef {
    QString id;
    QString name;
    QString category;
    QVector<PortDef> inputs;
    QVector<PortDef> outputs;
    QVector<NodeParamDef> params;
    QStringList outputExpr;      // GLSL expression per output; {in:name}, {param:name}, {id}
    QString helper;              // optional helper function emitted once
    QString description;
};

struct Node {
    QString id;                  // "n1", "n2", ...
    QString type;
    QPointF pos;
    QMap<QString, double> params;
    QString code;                // for Custom GLSL nodes
    QString label;
    QString group;               // editor grouping
    QJsonObject subgraph;        // for Subgraph nodes
};

struct Connection {
    QString fromNode; QString fromPort;
    QString toNode; QString toPort;
    bool operator==(const Connection& o) const { return fromNode == o.fromNode && fromPort == o.fromPort && toNode == o.toNode && toPort == o.toPort; }
};

struct CompileResult {
    bool ok = false;
    QString glsl;                // defines vec4 effect(vec2 p, vec2 uv, float t)
    QStringList errors;
    QStringList warnings;
    QStringList exposedParams;   // names for uP.x..w
    int nodeCount = 0;
};

class NodeRegistry {
public:
    static const NodeRegistry& instance();
    const NodeTypeDef* find(const QString& id) const;
    QVector<NodeTypeDef> all() const { return m_types; }
    QStringList categories() const;
private:
    NodeRegistry();
    QVector<NodeTypeDef> m_types;
};

class NodeGraph {
public:
    QString name = "Custom Effect";
    QVector<Node> nodes;
    QVector<Connection> connections;

    QString addNode(const QString& type, QPointF pos = {});
    bool removeNode(const QString& id);
    // Connects, replacing any existing connection into the same input. Returns error text or empty.
    QString connect(const QString& fromNode, const QString& fromPort, const QString& toNode, const QString& toPort);
    void disconnect(const QString& toNode, const QString& toPort);
    Node* node(const QString& id);
    const Node* node(const QString& id) const;
    bool wouldCreateCycle(const QString& from, const QString& to) const;
    QStringList validate() const;
    CompileResult compile() const;
    quint64 structureHash() const;   // changes only when topology/types/code change
    QJsonObject toJson() const;
    static NodeGraph fromJson(const QJsonObject& o);
    // Copy/paste helpers: returns JSON of selected nodes + internal connections; paste renames ids.
    QJsonObject copy(const QStringList& ids) const;
    QStringList paste(const QJsonObject& clip, QPointF offset);
    // Collapses selected nodes into a reusable subgraph node (inputs/outputs inferred).
    QString makeSubgraph(const QStringList& ids, const QString& name);
    static NodeGraph defaultGraph();
private:
    int m_next = 1;
    QString compileNode(const QString& id, const QString& prefix, QMap<QString, QStringList>& done, QStringList& code,
                        QSet<QString>& helpers, QStringList& errors, QSet<QString>& visiting, const QMap<QString, QString>& externalInputs) const;
};

} // namespace lf
