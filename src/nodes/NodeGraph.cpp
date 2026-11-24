#include "NodeGraph.h"
#include <QJsonArray>
#include <QSet>
#include <QCryptographicHash>
#include <functional>
#include <QRegularExpression>
#include <cstring>

namespace lf {

QString portTypeName(PortType t) { return t == PortType::Float ? "float" : t == PortType::Vec2 ? "vec2" : "vec3"; }

static NodeTypeDef T(const QString& id, const QString& name, const QString& cat, QVector<PortDef> in, QVector<PortDef> out,
                     QStringList expr, QVector<NodeParamDef> params = {}, const QString& helper = {}, const QString& desc = {}) {
    return NodeTypeDef{id, name, cat, in, out, params, expr, helper, desc};
}
using P = PortType;

NodeRegistry::NodeRegistry() {
    auto& t = m_types;
    // ---- inputs / sources
    t << T("uv", "Coordinates", "Input", {}, {{"p", P::Vec2}, {"uv", P::Vec2}}, {"p", "uv"}, {}, {}, "Centred transformed position p and raw uv.");
    t << T("time", "Time", "Input", {}, {{"t", P::Float}}, {"t"});
    t << T("constant", "Constant", "Input", {}, {{"value", P::Float}}, {"float({param:value})"}, {{"value", 0.5, -10, 10}});
    t << T("color", "Colour", "Input", {}, {{"color", P::Color}}, {"vec3({param:r}, {param:g}, {param:b})"}, {{"r", 1, 0, 1}, {"g", 0.5, 0, 1}, {"b", 0, 0, 1}});
    t << T("vec2", "Vector", "Input", {{"x", P::Float}, {"y", P::Float}}, {{"v", P::Vec2}}, {"vec2({in:x}, {in:y})"});
    t << T("param", "Exposed Parameter", "Parameter modulation", {}, {{"value", P::Float}}, {"uP[{param:slot}]"}, {{"slot", 0, 0, 3}}, {}, "Reads layer parameter p1..p4 (slot 0..3). Mappable from console, MIDI, OSC, timeline.");
    t << T("audio", "Audio Band", "Audio input", {}, {{"value", P::Float}}, {"uAudio[{param:band}]"}, {{"band", 0, 0, 3}}, {}, "0=bass 1=mid 2=treble 3=level");
    t << T("beat", "Beat Pulse", "Audio input", {}, {{"value", P::Float}}, {"uBeat"});
    t << T("spectrum", "Spectrum", "Audio input", {{"x", P::Float}}, {{"value", P::Float}}, {"spectrumAt({in:x})"});
    t << T("midi", "MIDI Input", "MIDI input", {}, {{"value", P::Float}}, {"uExt[{param:slot}]"}, {{"slot", 0, 0, 7}}, {}, "External slot ext.1..8; map a MIDI control to ext.N in Console Mapping.");
    t << T("osc", "OSC Input", "OSC input", {}, {{"value", P::Float}}, {"uExt[{param:slot}]"}, {{"slot", 1, 0, 7}}, {}, "External slot ext.1..8; map an OSC address to ext.N.");
    t << T("dmx", "DMX Input", "DMX input", {}, {{"value", P::Float}}, {"uExt[{param:slot}]"}, {{"slot", 2, 0, 7}}, {}, "External slot ext.1..8; map a DMX channel to ext.N.");
    t << T("timecode", "Timecode", "Timecode", {}, {{"seconds", P::Float}}, {"uTimecode"});
    t << T("image", "Image / Video Input", "Video input", {{"uv", P::Vec2}}, {{"color", P::Color}, {"alpha", P::Float}}, {"texture(uTex, {in:uv}).rgb", "texture(uTex, {in:uv}).a"}, {}, {}, "Samples the layer's media texture.");
    t << T("feedback", "Previous Frame", "Video input", {{"uv", P::Vec2}}, {{"color", P::Color}}, {"texture(uPrev, {in:uv}).rgb"});
    // ---- generators
    t << T("noise", "Noise (fBm)", "Noise", {{"p", P::Vec2}, {"scale", P::Float, 3}}, {{"value", P::Float}}, {"fbm({in:p} * {in:scale})"});
    t << T("valuenoise", "Value Noise", "Noise", {{"p", P::Vec2}, {"scale", P::Float, 5}}, {{"value", P::Float}}, {"noise({in:p} * {in:scale})"});
    t << T("voronoi", "Voronoi", "Noise", {{"p", P::Vec2}, {"scale", P::Float, 6}, {"t", P::Float}}, {{"dist", P::Float}, {"cell", P::Float}},
           {"lf_voronoi({in:p} * {in:scale}, {in:t}).x", "lf_voronoi({in:p} * {in:scale}, {in:t}).y"}, {},
           "vec2 lf_voronoi(vec2 x, float t) { vec2 n = floor(x), f = fract(x); float md = 8.0, id = 0.0; for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) { vec2 g = vec2(i, j); vec2 o = hash22(n + g); o = 0.5 + 0.5 * sin(t + TAU * o); vec2 r = g + o - f; float d = dot(r, r); if (d < md) { md = d; id = hash21(n + g); } } return vec2(sqrt(md), id); }\n");
    t << T("circle", "Circle", "Generators", {{"p", P::Vec2}, {"radius", P::Float, 0.3}, {"feather", P::Float, 0.01}}, {{"mask", P::Float}}, {"smoothstep({in:radius} + {in:feather}, {in:radius}, length({in:p}))"});
    t << T("rings", "Rings", "Generators", {{"p", P::Vec2}, {"freq", P::Float, 10}, {"phase", P::Float}}, {{"value", P::Float}}, {"(0.5 + 0.5 * sin(length({in:p}) * {in:freq} - {in:phase}))"});
    t << T("stripes", "Stripes", "Generators", {{"p", P::Vec2}, {"freq", P::Float, 10}, {"angle", P::Float}, {"phase", P::Float}}, {{"value", P::Float}}, {"(0.5 + 0.5 * sin(dot({in:p}, vec2(cos({in:angle}), sin({in:angle}))) * {in:freq} - {in:phase}))"});
    t << T("checker", "Checker", "Generators", {{"p", P::Vec2}, {"size", P::Float, 8}}, {{"value", P::Float}}, {"mod(floor({in:p}.x * {in:size}) + floor({in:p}.y * {in:size}), 2.0)"});
    t << T("polygon", "Polygon", "Generators", {{"p", P::Vec2}, {"sides", P::Float, 6}, {"radius", P::Float, 0.3}}, {{"mask", P::Float}},
           {"smoothstep(0.01, 0.0, length({in:p}) * cos(mod(atan({in:p}.y, {in:p}.x), TAU / {in:sides}) - PI / {in:sides}) - {in:radius})"});
    t << T("plasma", "Plasma", "Generators", {{"p", P::Vec2}, {"t", P::Float}, {"freq", P::Float, 5}}, {{"value", P::Float}},
           {"(0.5 + 0.125 * (sin({in:p}.x * {in:freq} + {in:t}) + sin({in:p}.y * {in:freq} - {in:t}) + sin(({in:p}.x + {in:p}.y) * {in:freq} * 0.7 + {in:t}) + sin(length({in:p}) * {in:freq} * 1.3 - {in:t})))"});
    t << T("particles", "Particles", "Particles", {{"p", P::Vec2}, {"t", P::Float}, {"density", P::Float, 10}, {"size", P::Float, 0.15}}, {{"value", P::Float}},
           {"lf_particles({in:p}, {in:t}, {in:density}, {in:size})"}, {},
           "float lf_particles(vec2 p, float t, float dens, float size) { vec2 g = p * dens; vec2 id = floor(g), f = fract(g) - 0.5; float a = 0.0; for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { vec2 o = vec2(x, y); vec2 h = hash22(id + o); vec2 c = o + (h - 0.5) * 0.8 + 0.25 * vec2(sin(t + h.y * TAU), cos(t * 0.8 + h.x * TAU)); a += smoothstep(size, 0.0, length(f - c)); } return a; }\n");
    // ---- colour
    t << T("palette", "Palette", "Colour", {{"t", P::Float}}, {{"color", P::Color}}, {"palette({in:t})"}, {}, {}, "Blend between layer colours A and B.");
    t << T("hsv", "HSV to RGB", "Colour", {{"h", P::Float}, {"s", P::Float, 1}, {"v", P::Float, 1}}, {{"color", P::Color}}, {"hsv2rgb(vec3({in:h}, {in:s}, {in:v}))"});
    t << T("gradient", "Gradient Map", "Colour", {{"t", P::Float}}, {{"color", P::Color}}, {"mix(vec3({param:r1}, {param:g1}, {param:b1}), vec3({param:r2}, {param:g2}, {param:b2}), clamp({in:t}, 0.0, 1.0))"},
           {{"r1", 0, 0, 1}, {"g1", 0, 0, 1}, {"b1", 0.2, 0, 1}, {"r2", 0, 0, 1}, {"g2", 0.8, 0, 1}, {"b2", 1, 0, 1}});
    t << T("tint", "Tint", "Colour", {{"color", P::Color}, {"amount", P::Float, 1}}, {{"color", P::Color}}, {"{in:color} * {in:amount}"});
    t << T("huerot", "Hue Rotate", "Colour", {{"color", P::Color}, {"shift", P::Float}}, {{"color", P::Color}}, {"hsv2rgb(vec3(fract(rgb2hsv({in:color}).x + {in:shift}), rgb2hsv({in:color}).yz))"});
    // ---- transform
    t << T("rotate", "Rotate", "Transform", {{"p", P::Vec2}, {"angle", P::Float}}, {{"p", P::Vec2}}, {"(rot2({in:angle}) * {in:p})"});
    t << T("scale", "Scale", "Transform", {{"p", P::Vec2}, {"s", P::Float, 2}}, {{"p", P::Vec2}}, {"({in:p} * {in:s})"});
    t << T("translate", "Translate", "Transform", {{"p", P::Vec2}, {"x", P::Float}, {"y", P::Float}}, {{"p", P::Vec2}}, {"({in:p} + vec2({in:x}, {in:y}))"});
    t << T("polar", "To Polar", "Transform", {{"p", P::Vec2}}, {{"p", P::Vec2}, {"radius", P::Float}, {"angle", P::Float}}, {"vec2(length({in:p}), atan({in:p}.y, {in:p}.x) / TAU + 0.5)", "length({in:p})", "(atan({in:p}.y, {in:p}.x) / TAU + 0.5)"});
    t << T("kaleido", "Kaleidoscope", "Transform", {{"p", P::Vec2}, {"segments", P::Float, 6}}, {{"p", P::Vec2}}, {"lf_kaleido({in:p}, {in:segments})"}, {},
           "vec2 lf_kaleido(vec2 p, float n) { float r = length(p), a = atan(p.y, p.x); float s = TAU / max(n, 1.0); a = mod(a, s); a = abs(a - s * 0.5); return vec2(cos(a), sin(a)) * r; }\n");
    t << T("tile", "Tile", "Transform", {{"p", P::Vec2}, {"n", P::Float, 4}}, {{"p", P::Vec2}}, {"(fract({in:p} * {in:n}) - 0.5)"});
    // ---- distortion
    t << T("twirl", "Twirl", "Distortion", {{"p", P::Vec2}, {"amount", P::Float, 3}}, {{"p", P::Vec2}}, {"(rot2({in:amount} * length({in:p})) * {in:p})"});
    t << T("warp", "Noise Warp", "Distortion", {{"p", P::Vec2}, {"amount", P::Float, 0.3}, {"t", P::Float}}, {{"p", P::Vec2}}, {"({in:p} + {in:amount} * (vec2(fbm({in:p} * 2.0 + {in:t} * 0.2), fbm({in:p} * 2.0 + vec2(5.2, 1.3) - {in:t} * 0.2)) - 0.5))"});
    t << T("ripple", "Ripple", "Distortion", {{"p", P::Vec2}, {"freq", P::Float, 20}, {"amp", P::Float, 0.02}, {"t", P::Float}}, {{"p", P::Vec2}}, {"({in:p} + normalize({in:p} + 1e-5) * sin(length({in:p}) * {in:freq} - {in:t} * 4.0) * {in:amp})"});
    // ---- masks
    t << T("smoothstep", "Smooth Mask", "Masks", {{"x", P::Float}, {"edge0", P::Float, 0.4}, {"edge1", P::Float, 0.6}}, {{"mask", P::Float}}, {"smoothstep({in:edge0}, {in:edge1}, {in:x})"});
    t << T("threshold", "Threshold", "Masks", {{"x", P::Float}, {"level", P::Float, 0.5}}, {{"mask", P::Float}}, {"step({in:level}, {in:x})"});
    t << T("invert", "Invert", "Masks", {{"x", P::Float}}, {{"x", P::Float}}, {"(1.0 - {in:x})"});
    t << T("applymask", "Apply Mask", "Masks", {{"color", P::Color}, {"mask", P::Float, 1}}, {{"color", P::Color}}, {"({in:color} * clamp({in:mask}, 0.0, 1.0))"});
    // ---- compositing
    t << T("addc", "Add (Colour)", "Compositing", {{"a", P::Color}, {"b", P::Color}}, {{"color", P::Color}}, {"({in:a} + {in:b})"});
    t << T("mulc", "Multiply (Colour)", "Compositing", {{"a", P::Color}, {"b", P::Color, 1}}, {{"color", P::Color}}, {"({in:a} * {in:b})"});
    t << T("screen", "Screen", "Compositing", {{"a", P::Color}, {"b", P::Color}}, {{"color", P::Color}}, {"(1.0 - (1.0 - {in:a}) * (1.0 - {in:b}))"});
    t << T("mixc", "Mix (Colour)", "Compositing", {{"a", P::Color}, {"b", P::Color}, {"f", P::Float, 0.5}}, {{"color", P::Color}}, {"mix({in:a}, {in:b}, clamp({in:f}, 0.0, 1.0))"});
    t << T("maxc", "Lighten", "Compositing", {{"a", P::Color}, {"b", P::Color}}, {{"color", P::Color}}, {"max({in:a}, {in:b})"});
    // ---- maths
    auto bin = [&](const QString& id, const QString& name, const QString& op) { t << T(id, name, "Mathematics", {{"a", P::Float}, {"b", P::Float, id == "mul" || id == "div" ? 1.0 : 0.0}}, {{"value", P::Float}}, {op}); };
    bin("add", "Add", "({in:a} + {in:b})"); bin("sub", "Subtract", "({in:a} - {in:b})"); bin("mul", "Multiply", "({in:a} * {in:b})");
    bin("div", "Divide", "({in:a} / max(abs({in:b}), 1e-6) * sign({in:b} + 1e-9))"); bin("min", "Minimum", "min({in:a}, {in:b})"); bin("max", "Maximum", "max({in:a}, {in:b})");
    bin("pow", "Power", "pow(abs({in:a}), {in:b})"); bin("mod", "Modulo", "mod({in:a}, max({in:b}, 1e-6))");
    auto un = [&](const QString& id, const QString& name, const QString& op) { t << T(id, name, "Mathematics", {{"x", P::Float}}, {{"value", P::Float}}, {op}); };
    un("sin", "Sine", "sin({in:x})"); un("cos", "Cosine", "cos({in:x})"); un("abs", "Absolute", "abs({in:x})"); un("fract", "Fraction", "fract({in:x})");
    un("sqrt", "Square Root", "sqrt(abs({in:x}))"); un("sat", "Saturate", "clamp({in:x}, 0.0, 1.0)");
    t << T("remap", "Remap", "Mathematics", {{"x", P::Float}}, {{"value", P::Float}}, {"mix({param:outMin}, {param:outMax}, ({in:x} - {param:inMin}) / max({param:inMax} - {param:inMin}, 1e-6))"},
           {{"inMin", 0, -10, 10}, {"inMax", 1, -10, 10}, {"outMin", 0, -10, 10}, {"outMax", 1, -10, 10}});
    t << T("length", "Length", "Mathematics", {{"v", P::Vec2}}, {{"value", P::Float}}, {"length({in:v})"});
    t << T("split", "Split Vector", "Mathematics", {{"v", P::Vec2}}, {{"x", P::Float}, {"y", P::Float}}, {"{in:v}.x", "{in:v}.y"});
    t << T("luma", "Luminance", "Mathematics", {{"color", P::Color}}, {{"value", P::Float}}, {"dot({in:color}, vec3(0.2126, 0.7152, 0.0722))"});
    // ---- logic
    t << T("step", "Compare (a >= b)", "Logic", {{"a", P::Float}, {"b", P::Float, 0.5}}, {{"value", P::Float}}, {"step({in:b}, {in:a})"});
    t << T("select", "Select", "Logic", {{"cond", P::Float}, {"a", P::Color}, {"b", P::Color}}, {{"color", P::Color}}, {"({in:cond} > 0.5 ? {in:a} : {in:b})"});
    // ---- modulation
    t << T("lfo", "LFO", "Parameter modulation", {{"t", P::Float}, {"rate", P::Float, 1}}, {{"sine", P::Float}, {"saw", P::Float}, {"square", P::Float}},
           {"(0.5 + 0.5 * sin({in:t} * {in:rate} * TAU))", "fract({in:t} * {in:rate})", "step(0.5, fract({in:t} * {in:rate}))"});
    // ---- custom / structure
    t << T("glsl", "Custom GLSL", "Custom", {{"p", P::Vec2}, {"a", P::Float}, {"b", P::Float}, {"c", P::Color}}, {{"color", P::Color}}, {"{code}"}, {}, {},
           "Custom expression returning vec3. Available: p (vec2), a, b (float), c (vec3), t, uv and all prelude helpers.");
    t << T("subgraph", "Subgraph", "Custom", {{"p", P::Vec2}, {"a", P::Float}, {"b", P::Float}, {"c", P::Color}}, {{"color", P::Color}}, {"{subgraph}"});
    t << T("subin", "Subgraph Inputs", "Custom", {}, {{"p", P::Vec2}, {"a", P::Float}, {"b", P::Float}, {"c", P::Color}}, {"{ext:p}", "{ext:a}", "{ext:b}", "{ext:c}"});
    t << T("output", "Output", "Output", {{"color", P::Color}, {"alpha", P::Float, 1}}, {}, {});
}

const NodeRegistry& NodeRegistry::instance() { static NodeRegistry r; return r; }
const NodeTypeDef* NodeRegistry::find(const QString& id) const { for (auto& t : m_types) if (t.id == id) return &t; return nullptr; }
QStringList NodeRegistry::categories() const { QStringList c; for (auto& t : m_types) if (!c.contains(t.category)) c << t.category; return c; }

// ---------------------------------------------------------------- graph editing
QString NodeGraph::addNode(const QString& type, QPointF pos) {
    const NodeTypeDef* d = NodeRegistry::instance().find(type);
    if (!d) return {};
    for (auto& n : nodes) { bool ok; int k = n.id.mid(1).toInt(&ok); if (ok) m_next = std::max(m_next, k + 1); }
    Node n; n.id = QString("n%1").arg(m_next++); n.type = type; n.pos = pos; n.label = d->name;
    for (auto& p : d->params) n.params[p.name] = p.defaultValue;
    if (type == "glsl") n.code = "c * (0.5 + 0.5 * sin(length(p) * 20.0 - t * 3.0 + a * 6.0))";
    nodes << n;
    return n.id;
}

bool NodeGraph::removeNode(const QString& id) {
    const auto before = nodes.size();
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](auto& n) { return n.id == id; }), nodes.end());
    connections.erase(std::remove_if(connections.begin(), connections.end(), [&](auto& c) { return c.fromNode == id || c.toNode == id; }), connections.end());
    return nodes.size() != before;
}

Node* NodeGraph::node(const QString& id) { for (auto& n : nodes) if (n.id == id) return &n; return nullptr; }
const Node* NodeGraph::node(const QString& id) const { for (auto& n : nodes) if (n.id == id) return &n; return nullptr; }

bool NodeGraph::wouldCreateCycle(const QString& from, const QString& to) const {
    // adding from->to creates a cycle if `from` is reachable from `to` downstream
    if (from == to) return true;
    QSet<QString> seen; QVector<QString> stack{to};
    while (!stack.isEmpty()) {
        const QString cur = stack.takeLast();
        if (cur == from) return true;
        if (seen.contains(cur)) continue;
        seen.insert(cur);
        for (auto& c : connections) if (c.fromNode == cur) stack << c.toNode;
    }
    return false;
}

static const PortDef* findPort(const QVector<PortDef>& ports, const QString& name) { for (auto& p : ports) if (p.name == name) return &p; return nullptr; }
static bool compatible(PortType from, PortType to) { return from == to || from == PortType::Float || (from == PortType::Color && to == PortType::Float); }

QString NodeGraph::connect(const QString& fn, const QString& fp, const QString& tn, const QString& tp) {
    const Node* a = node(fn); const Node* b = node(tn);
    if (!a || !b) return "unknown node";
    const NodeTypeDef* da = NodeRegistry::instance().find(a->type); const NodeTypeDef* db = NodeRegistry::instance().find(b->type);
    const PortDef* po = da ? findPort(da->outputs, fp) : nullptr; const PortDef* pi = db ? findPort(db->inputs, tp) : nullptr;
    if (!po) return QString("'%1' has no output '%2'").arg(a->label, fp);
    if (!pi) return QString("'%1' has no input '%2'").arg(b->label, tp);
    if (!compatible(po->type, pi->type)) return QString("type mismatch: %1 output '%2' cannot drive %3 input '%4'").arg(portTypeName(po->type), fp, portTypeName(pi->type), tp);
    if (wouldCreateCycle(fn, tn)) return "connection would create a cycle";
    disconnect(tn, tp);
    connections << Connection{fn, fp, tn, tp};
    return {};
}

void NodeGraph::disconnect(const QString& tn, const QString& tp) {
    connections.erase(std::remove_if(connections.begin(), connections.end(), [&](auto& c) { return c.toNode == tn && c.toPort == tp; }), connections.end());
}

QStringList NodeGraph::validate() const {
    QStringList e;
    int outputs = 0;
    QSet<QString> ids;
    for (auto& n : nodes) {
        if (ids.contains(n.id)) e << "duplicate node id " + n.id;
        ids.insert(n.id);
        if (!NodeRegistry::instance().find(n.type)) e << QString("node %1: unknown type '%2'").arg(n.id, n.type);
        if (n.type == "output") ++outputs;
    }
    if (outputs == 0) e << "graph has no Output node";
    if (outputs > 1) e << "graph has more than one Output node";
    QSet<QString> inputs;
    for (auto& c : connections) {
        const Node* a = node(c.fromNode); const Node* b = node(c.toNode);
        if (!a || !b) { e << "dangling connection " + c.fromNode + " -> " + c.toNode; continue; }
        const NodeTypeDef* da = NodeRegistry::instance().find(a->type); const NodeTypeDef* db = NodeRegistry::instance().find(b->type);
        const PortDef* po = da ? findPort(da->outputs, c.fromPort) : nullptr; const PortDef* pi = db ? findPort(db->inputs, c.toPort) : nullptr;
        if (!po || !pi) { e << QString("invalid port in connection %1.%2 -> %3.%4").arg(c.fromNode, c.fromPort, c.toNode, c.toPort); continue; }
        if (!compatible(po->type, pi->type)) e << QString("type mismatch %1.%2 (%3) -> %4.%5 (%6)").arg(c.fromNode, c.fromPort, portTypeName(po->type), c.toNode, c.toPort, portTypeName(pi->type));
        const QString key = c.toNode + "." + c.toPort;
        if (inputs.contains(key)) e << "multiple connections into " + key;
        inputs.insert(key);
    }
    // cycle detection (DFS colouring)
    QMap<QString, int> colour;
    std::function<bool(const QString&)> dfs = [&](const QString& id) {
        colour[id] = 1;
        for (auto& c : connections) if (c.fromNode == id) {
            if (colour.value(c.toNode) == 1) return true;
            if (colour.value(c.toNode) == 0 && dfs(c.toNode)) return true;
        }
        colour[id] = 2; return false;
    };
    for (auto& n : nodes) if (colour.value(n.id) == 0 && dfs(n.id)) { e << "graph contains a cycle"; break; }
    return e;
}

static QString convert(const QString& expr, PortType from, PortType to) {
    if (from == to) return expr;
    if (from == PortType::Float && to == PortType::Color) return "vec3(" + expr + ")";
    if (from == PortType::Float && to == PortType::Vec2) return "vec2(" + expr + ")";
    if (from == PortType::Color && to == PortType::Float) return "dot(" + expr + ", vec3(0.2126, 0.7152, 0.0722))";
    return expr;
}

static QString num(double v) { QString s = QString::number(v, 'g', 9); if (!s.contains('.') && !s.contains('e')) s += ".0"; return s; }

QString NodeGraph::compileNode(const QString& id, const QString& prefix, QMap<QString, QStringList>& done, QStringList& code,
                               QSet<QString>& helpers, QStringList& errors, QSet<QString>& visiting, const QMap<QString, QString>& ext) const {
    if (done.contains(id)) return {};
    if (visiting.contains(id)) { errors << "cycle at node " + id; return {}; }
    visiting.insert(id);
    const Node* n = node(id);
    const NodeTypeDef* d = n ? NodeRegistry::instance().find(n->type) : nullptr;
    if (!d) { errors << "unknown node " + id; return {}; }
    QMap<QString, QString> in;
    for (const auto& port : d->inputs) {
        QString expr;
        for (auto& c : connections) if (c.toNode == id && c.toPort == port.name) {
            compileNode(c.fromNode, prefix, done, code, helpers, errors, visiting, ext);
            const Node* src = node(c.fromNode);
            const NodeTypeDef* sd = src ? NodeRegistry::instance().find(src->type) : nullptr;
            if (!sd) break;
            for (int k = 0; k < sd->outputs.size(); ++k) if (sd->outputs[k].name == c.fromPort && done.contains(c.fromNode) && k < done[c.fromNode].size())
                expr = convert(done[c.fromNode][k], sd->outputs[k].type, port.type);
        }
        if (expr.isEmpty()) {
            // unconnected: per-node override (params[name]) or port default; p/uv/t fall back to the effect's arguments
            if (port.type == PortType::Vec2) expr = port.name == "uv" ? "uv" : "p";
            else if (port.name == "t" && !n->params.contains("t")) expr = "t";
            else if (port.type == PortType::Color) expr = "vec3(" + num(n->params.value(port.name, port.defaultValue)) + ")";
            else expr = num(n->params.value(port.name, port.defaultValue));
        }
        in[port.name] = expr;
    }
    if (!d->helper.isEmpty()) helpers.insert(d->helper);
    QStringList outs;
    for (int k = 0; k < d->outputs.size(); ++k) {
        QString e = d->outputExpr.value(k);
        for (auto it = in.begin(); it != in.end(); ++it) e.replace("{in:" + it.key() + "}", it.value());
        for (auto it = n->params.begin(); it != n->params.end(); ++it) e.replace("{param:" + it.key() + "}", it.key() == "slot" || it.key() == "band" ? QString::number(int(it.value())) : num(it.value()));
        for (auto& pd : d->params) e.replace("{param:" + pd.name + "}", pd.name == "slot" || pd.name == "band" ? QString::number(int(pd.defaultValue)) : num(pd.defaultValue));
        for (auto it = ext.begin(); it != ext.end(); ++it) e.replace("{ext:" + it.key() + "}", it.value());
        e.replace(QRegularExpression("\\{ext:p\\}"), "p"); e.replace(QRegularExpression("\\{ext:[abc]\\}"), "0.0");
        if (n->type == "glsl") {
            QString user = n->code.trimmed();
            if (user.contains(';') || user.contains('{') || user.contains("discard")) { errors << QString("node %1: custom GLSL must be a single expression (no ';', '{' or discard)").arg(id); user = "vec3(0.0)"; }
            helpers.insert(QString("vec3 %1%2_fn(vec2 p, vec2 uv, float t, float a, float b, vec3 c) { return vec3(%3); }\n").arg(prefix, id, user));
            e = QString("%1%2_fn(%3, uv, t, %4, %5, %6)").arg(prefix, id, in["p"], in["a"], in["b"], in["c"]);
        }
        if (n->type == "subgraph") {
            NodeGraph sub = NodeGraph::fromJson(n->subgraph);
            QMap<QString, QStringList> sdone; QSet<QString> svis;
            QMap<QString, QString> sext{{"p", in["p"]}, {"a", in["a"]}, {"b", in["b"]}, {"c", in["c"]}};
            const Node* sout = nullptr; for (auto& sn : sub.nodes) if (sn.type == "output") sout = &sn;
            if (!sout) { errors << "subgraph " + id + " has no output"; e = "vec3(0.0)"; }
            else {
                QStringList scode;
                for (auto& c : sub.connections) if (c.toNode == sout->id) sub.compileNode(c.fromNode, prefix + id + "_", sdone, scode, helpers, errors, svis, sext);
                code << scode;
                QString colorExpr = "vec3(0.0)";
                for (auto& c : sub.connections) if (c.toNode == sout->id && c.toPort == "color") {
                    const Node* src = sub.node(c.fromNode); const NodeTypeDef* sd = src ? NodeRegistry::instance().find(src->type) : nullptr;
                    if (sd) for (int k2 = 0; k2 < sd->outputs.size(); ++k2) if (sd->outputs[k2].name == c.fromPort && sdone.contains(c.fromNode)) colorExpr = convert(sdone[c.fromNode][k2], sd->outputs[k2].type, PortType::Color);
                }
                e = colorExpr;
            }
        }
        const QString var = QString("%1%2_%3").arg(prefix, id, d->outputs[k].name);
        code << QString("    %1 %2 = %3;").arg(portTypeName(d->outputs[k].type), var, e);
        outs << var;
    }
    visiting.remove(id);
    done[id] = outs;
    return {};
}

CompileResult NodeGraph::compile() const {
    CompileResult r;
    r.errors = validate();
    if (!r.errors.isEmpty()) return r;
    const Node* out = nullptr;
    for (auto& n : nodes) if (n.type == "output") out = &n;
    QMap<QString, QStringList> done; QStringList code; QSet<QString> helpers; QSet<QString> visiting;
    QString colorExpr = "vec3(0.0)", alphaExpr = "1.0";
    for (auto& c : connections) if (c.toNode == out->id) {
        compileNode(c.fromNode, "", done, code, helpers, r.errors, visiting, {});
        const Node* src = node(c.fromNode); const NodeTypeDef* sd = NodeRegistry::instance().find(src->type);
        for (int k = 0; k < sd->outputs.size(); ++k) if (sd->outputs[k].name == c.fromPort && done.contains(c.fromNode)) {
            if (c.toPort == "color") colorExpr = convert(done[c.fromNode][k], sd->outputs[k].type, PortType::Color);
            else alphaExpr = convert(done[c.fromNode][k], sd->outputs[k].type, PortType::Float);
        }
    }
    if (!r.errors.isEmpty()) return r;
    const int reachable = int(done.size());
    if (reachable + 1 < nodes.size()) r.warnings << QString("%1 node(s) are not connected to the output and are ignored").arg(nodes.size() - reachable - 1);
    // exposed parameter names
    r.exposedParams = QStringList{"-", "-", "-", "-"};
    for (auto& n : nodes) if (n.type == "param") { const int s = std::clamp(int(n.params.value("slot")), 0, 3); r.exposedParams[s] = n.label.isEmpty() ? QString("Param %1").arg(s + 1) : n.label; }
    QString glsl = "// generated by LUMAFLOW node compiler: " + name + "\n";
    for (auto& h : helpers) glsl += h;
    glsl += "vec4 effect(vec2 p, vec2 uv, float t) {\n" + code.join('\n') + QString("\n    return vec4(%1, %2);\n}\n").arg(colorExpr, alphaExpr);
    r.glsl = glsl; r.ok = true; r.nodeCount = reachable;
    return r;
}

quint64 NodeGraph::structureHash() const {
    QByteArray b;
    for (auto& n : nodes) { b += n.id.toUtf8() + n.type.toUtf8() + n.code.toUtf8(); for (auto it = n.params.begin(); it != n.params.end(); ++it) b += it.key().toUtf8() + QByteArray::number(it.value()); }
    for (auto& c : connections) b += (c.fromNode + c.fromPort + c.toNode + c.toPort).toUtf8();
    const QByteArray h = QCryptographicHash::hash(b, QCryptographicHash::Md5);
    quint64 v; memcpy(&v, h.constData(), 8); return v;
}

QJsonObject NodeGraph::toJson() const {
    QJsonArray ns;
    for (auto& n : nodes) {
        QJsonObject p; for (auto it = n.params.begin(); it != n.params.end(); ++it) p[it.key()] = it.value();
        QJsonObject o{{"id", n.id}, {"type", n.type}, {"x", n.pos.x()}, {"y", n.pos.y()}, {"params", p}, {"label", n.label}};
        if (!n.code.isEmpty()) o["code"] = n.code;
        if (!n.group.isEmpty()) o["group"] = n.group;
        if (!n.subgraph.isEmpty()) o["subgraph"] = n.subgraph;
        ns << o;
    }
    QJsonArray cs; for (auto& c : connections) cs << QJsonArray{c.fromNode, c.fromPort, c.toNode, c.toPort};
    return {{"name", name}, {"nodes", ns}, {"connections", cs}, {"format", "lfgraph"}, {"version", 1}};
}

NodeGraph NodeGraph::fromJson(const QJsonObject& o) {
    NodeGraph g; g.name = o["name"].toString("Custom Effect");
    for (auto v : o["nodes"].toArray()) {
        const QJsonObject x = v.toObject(); Node n;
        n.id = x["id"].toString(); n.type = x["type"].toString(); n.pos = QPointF(x["x"].toDouble(), x["y"].toDouble());
        const QJsonObject p = x["params"].toObject(); for (auto it = p.begin(); it != p.end(); ++it) n.params[it.key()] = it.value().toDouble();
        n.code = x["code"].toString(); n.label = x["label"].toString(); n.group = x["group"].toString(); n.subgraph = x["subgraph"].toObject();
        g.nodes << n;
    }
    for (auto v : o["connections"].toArray()) { auto a = v.toArray(); g.connections << Connection{a[0].toString(), a[1].toString(), a[2].toString(), a[3].toString()}; }
    return g;
}

QJsonObject NodeGraph::copy(const QStringList& ids) const {
    NodeGraph g;
    for (auto& n : nodes) if (ids.contains(n.id)) g.nodes << n;
    for (auto& c : connections) if (ids.contains(c.fromNode) && ids.contains(c.toNode)) g.connections << c;
    return g.toJson();
}

QStringList NodeGraph::paste(const QJsonObject& clip, QPointF offset) {
    const NodeGraph g = fromJson(clip);
    QMap<QString, QString> rename; QStringList created;
    for (auto n : g.nodes) {
        if (n.type == "output" && std::any_of(nodes.begin(), nodes.end(), [](auto& x) { return x.type == "output"; })) continue;
        const QString nid = addNode(n.type, n.pos + offset);
        Node* nn = node(nid); nn->params = n.params; nn->code = n.code; nn->label = n.label; nn->subgraph = n.subgraph; nn->group = n.group;
        rename[n.id] = nid; created << nid;
    }
    for (auto& c : g.connections) if (rename.contains(c.fromNode) && rename.contains(c.toNode)) connections << Connection{rename[c.fromNode], c.fromPort, rename[c.toNode], c.toPort};
    return created;
}

QString NodeGraph::makeSubgraph(const QStringList& ids, const QString& sname) {
    // The selection must have exactly one Colour output leaving it; Vec2 'p' inputs are rewired to the subgraph input node.
    NodeGraph sub; sub.name = sname;
    for (auto& n : nodes) if (ids.contains(n.id) && n.type != "output") sub.nodes << n;
    for (auto& c : connections) if (ids.contains(c.fromNode) && ids.contains(c.toNode)) sub.connections << c;
    QVector<Connection> leaving;
    for (auto& c : connections) if (ids.contains(c.fromNode) && !ids.contains(c.toNode)) leaving << c;
    if (leaving.isEmpty()) return {};
    const QString inId = sub.addNode("subin", QPointF(-200, 0));
    const QString outId = sub.addNode("output", QPointF(400, 0));
    sub.connections << Connection{leaving.first().fromNode, leaving.first().fromPort, outId, "color"};
    for (auto& c : connections) if (!ids.contains(c.fromNode) && ids.contains(c.toNode)) {
        const Node* src = node(c.fromNode);
        if (src && src->type == "uv") sub.connections << Connection{inId, "p", c.toNode, c.toPort};
    }
    QPointF centre; for (auto& n : sub.nodes) centre += n.pos; centre /= std::max<qsizetype>(1, sub.nodes.size());
    for (auto& id : ids) if (node(id) && node(id)->type != "output") removeNode(id);
    const QString nid = addNode("subgraph", centre);
    node(nid)->subgraph = sub.toJson(); node(nid)->label = sname;
    for (auto& c : leaving) if (node(c.toNode)) connections << Connection{nid, "color", c.toNode, c.toPort};
    return nid;
}

NodeGraph NodeGraph::defaultGraph() {
    NodeGraph g; g.name = "Node Effect";
    const QString uv = g.addNode("uv", {0, 0}), tm = g.addNode("time", {0, 120});
    const QString warp = g.addNode("warp", {200, 0}), noise = g.addNode("noise", {400, 0});
    const QString rings = g.addNode("rings", {400, 140}), mul = g.addNode("mul", {600, 60});
    const QString pal = g.addNode("palette", {780, 60}), prm = g.addNode("param", {400, 260}), out = g.addNode("output", {980, 60});
    g.node(prm)->label = "Warp"; 
    g.connect(uv, "p", warp, "p"); g.connect(tm, "t", warp, "t"); g.connect(prm, "value", warp, "amount");
    g.connect(warp, "p", noise, "p"); g.connect(warp, "p", rings, "p"); g.connect(tm, "t", rings, "phase");
    g.connect(noise, "value", mul, "a"); g.connect(rings, "value", mul, "b");
    g.connect(mul, "value", pal, "t"); g.connect(pal, "color", out, "color");
    return g;
}

} // namespace lf
