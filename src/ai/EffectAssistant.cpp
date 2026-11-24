#include "EffectAssistant.h"
#include "core/Log.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

namespace lf {

QJsonObject AiSuggestion::toJson() const {
    static const char* kinds[] = {"layer-preset", "node-effect", "shader-effect", "spatial"};
    QJsonObject o{{"kind", kinds[kind]}, {"title", title}, {"explanation", explanation}, {"effect", effectId}, {"values", layerValues},
                  {"colorA", QJsonArray{colorA[0], colorA[1], colorA[2]}}, {"colorB", QJsonArray{colorB[0], colorB[1], colorB[2]}},
                  {"mappings", QJsonArray::fromStringList(mappings)}};
    if (kind == NodeEffect) o["graph"] = graph.toJson();
    if (kind == ShaderEffect) o["glsl"] = glsl;
    if (kind == Spatial) o["spatial"] = spatial.toJson();
    return o;
}

EffectAssistant::EffectAssistant(QObject* parent) : QObject(parent) {}

RGB EffectAssistant::parseColour(const QString& w, bool* ok) {
    static const QMap<QString, RGB> c = {
        {"red", {1, 0, 0}}, {"green", {0, 1, 0}}, {"blue", {0, 0.25, 1}}, {"white", {1, 1, 1}}, {"black", {0, 0, 0}},
        {"yellow", {1, 0.9, 0}}, {"orange", {1, 0.45, 0}}, {"purple", {0.6, 0, 1}}, {"violet", {0.55, 0.1, 1}}, {"magenta", {1, 0, 0.8}},
        {"pink", {1, 0.3, 0.6}}, {"cyan", {0, 1, 1}}, {"teal", {0, 0.7, 0.6}}, {"amber", {1, 0.6, 0}}, {"gold", {1, 0.75, 0.2}},
        {"warm", {1, 0.6, 0.3}}, {"cold", {0.4, 0.7, 1}}, {"ice", {0.7, 0.9, 1}}, {"fire", {1, 0.35, 0}}, {"lime", {0.6, 1, 0}},
    };
    auto it = c.find(w.toLower());
    if (ok) *ok = it != c.end();
    return it == c.end() ? RGB{} : *it;
}

AiSuggestion EffectAssistant::generateLocal(const QString& promptIn, const QStringList& avail) const {
    const QString prompt = promptIn.toLower();
    const QStringList words = prompt.split(QRegularExpression("[^a-z0-9-]+"), Qt::SkipEmptyParts);
    AiSuggestion s;
    QVector<RGB> colours;
    for (const auto& w : words) { bool ok; RGB c = parseColour(w, &ok); if (ok) colours << c; }
    const bool audio = prompt.contains("audio") || prompt.contains("music") || prompt.contains("beat") || prompt.contains("bass") || prompt.contains("sound");
    const bool slow = prompt.contains("slow") || prompt.contains("gentle") || prompt.contains("calm");
    const bool fast = prompt.contains("fast") || prompt.contains("quick") || prompt.contains("energetic") || prompt.contains("aggressive");
    const double speed = slow ? 0.4 : fast ? 2.2 : 1.0;

    // ---------------- spatial / fixture effects
    const bool spatial = prompt.contains("chase") || prompt.contains("fixture") || prompt.contains("strip") || prompt.contains("wave across") ||
                         prompt.contains("centre-out") || prompt.contains("center-out") || prompt.contains("left to right") || prompt.contains("symmetr") ||
                         prompt.contains("position");
    if (spatial) {
        s.kind = AiSuggestion::Spatial;
        SpatialEffect& fx = s.spatial;
        fx.speed = 0.5 * speed;
        if (prompt.contains("centre-out") || prompt.contains("center-out") || prompt.contains("centre out") || prompt.contains("center out") || prompt.contains("outward")) fx.order = SpatialOrder::CentreOut;
        else if (prompt.contains("inward") || prompt.contains("outside in")) fx.order = SpatialOrder::OutsideIn;
        else if (prompt.contains("right to left")) fx.order = SpatialOrder::RightToLeft;
        else if (prompt.contains("front to back")) fx.order = SpatialOrder::FrontToBack;
        else if (prompt.contains("back to front")) fx.order = SpatialOrder::BackToFront;
        else if (prompt.contains("top to bottom") || prompt.contains("downward")) fx.order = SpatialOrder::TopToBottom;
        else if (prompt.contains("bottom to top") || prompt.contains("upward")) fx.order = SpatialOrder::BottomToTop;
        else if (prompt.contains("symmetr")) fx.order = SpatialOrder::Symmetrical;
        else if (prompt.contains("random")) fx.order = SpatialOrder::Random;
        else if (prompt.contains("radial") || prompt.contains("circular") || prompt.contains("clockwise")) fx.order = prompt.contains("anti") || prompt.contains("counter") ? SpatialOrder::RadialCCW : SpatialOrder::RadialCW;
        else fx.order = SpatialOrder::LeftToRight;
        if (prompt.contains("chase")) { fx.waveform = Waveform::Chase; fx.width = 0.3; }
        else if (prompt.contains("pulse")) fx.waveform = Waveform::Pulse;
        else if (prompt.contains("square") || prompt.contains("on/off") || prompt.contains("hard")) fx.waveform = Waveform::Square;
        else fx.waveform = Waveform::Sine;
        if (prompt.contains("rainbow") || prompt.contains("colour wave") || prompt.contains("color wave")) fx.colorSweep = true;
        fx.colorA = colours.value(0, RGB{1, 1, 1});
        fx.colorB = colours.size() > 1 ? colours[1] : RGB{0, 0, 0};
        if (colours.size() > 1 && fx.waveform == Waveform::Chase) fx.low = 1.0;   // two-colour chase keeps fixtures lit in colour B
        QRegularExpression wingRe("(\\d+)\\s*wings?");
        auto wm = wingRe.match(prompt); if (wm.hasMatch()) fx.wings = wm.captured(1).toInt();
        if (prompt.contains("mirror")) fx.wings = std::max(2, fx.wings);
        fx.name = promptIn.left(40);
        s.title = QString("%1 %2").arg(spatialOrderName(fx.order), waveformName(fx.waveform));
        s.explanation = QString("Spatial effect using fixture positions: order '%1', waveform %2, %3 cycles/s%4.")
            .arg(spatialOrderName(fx.order), waveformName(fx.waveform)).arg(fx.speed).arg(fx.colorSweep ? ", hue sweep" : "");
        if (audio) s.mappings << "audio beat -> spatial speed (set Audio reactivity on the master)";
        s.needsValidation = false;
        return s;
    }

    // ---------------- map keywords to effect families
    struct Rule { const char* key; const char* effect; };
    static const Rule rules[] = {
        {"vortex", "particle_vortex"}, {"tunnel", "particle_tunnel"}, {"explosion", "particle_explosion"}, {"firework", "particle_explosion"},
        {"storm", "particle_storm"}, {"rain", "particle_storm"}, {"trail", "particle_trails"}, {"particle", "particle_cloud"}, {"star", "particle_cloud"},
        {"plasma", "plasma"}, {"lightning", "lightning"}, {"arc", "electric_arcs"}, {"electric", "electric_arcs"}, {"ring", "energy_rings"},
        {"neon", "neon_lines"}, {"kaleido", "kaleidoscope"}, {"fractal", "fractal"}, {"mandala", "mandala"}, {"wireframe", "wireframe"},
        {"terrain", "wireframe"}, {"smoke", "smoke"}, {"fog", "smoke"}, {"fluid", "fluid"}, {"water", "fluid"}, {"ink", "ink"},
        {"flow", "flow_field"}, {"turbulen", "turbulence"}, {"blob", "organic_morph"}, {"glitch", "glitch"}, {"strobe", "strobe"},
        {"gradient", "gradient"}, {"spectrum", "spectrum"}, {"waveform", "waveform"}, {"feedback", "feedback"}, {"geometric", "geo_tunnel"},
        {"morph", "morph_geometry"}, {"pattern", "rotating_pattern"}, {"pulse", "pulse_field"},
    };
    QStringList matched;
    for (auto& r : rules) if (prompt.contains(r.key) && avail.contains(r.effect) && !matched.contains(r.effect)) matched << r.effect;

    if (matched.size() >= 2 || prompt.contains("combine") || prompt.contains(" with ") && matched.size() == 1 && audio) {
        // Build a node graph combining two generator families
        s.kind = AiSuggestion::NodeEffect;
        NodeGraph g; g.name = promptIn.left(40);
        const QString uv = g.addNode("uv", {0, 0}), tm = g.addNode("time", {0, 140});
        const QString warp = g.addNode("warp", {200, 0});
        g.connect(uv, "p", warp, "p"); g.connect(tm, "t", warp, "t");
        const QString genA = g.addNode(matched.join(' ').contains("particle") ? "particles" : "plasma", {420, 0});
        const QString genB = g.addNode(prompt.contains("ring") ? "rings" : "noise", {420, 160});
        g.connect(warp, "p", genA, "p"); if (g.node(genA)->type == "particles" || g.node(genA)->type == "plasma") g.connect(tm, "t", genA, "t");
        g.connect(warp, "p", genB, "p"); if (g.node(genB)->type == "rings") g.connect(tm, "t", genB, "phase");
        const QString add = g.addNode("add", {620, 60}); g.connect(genA, "value", add, "a"); g.connect(genB, "value", add, "b");
        QString drive = add;
        if (audio) { const QString au = g.addNode("audio", {620, 220}); const QString mul = g.addNode("mul", {800, 120}); g.node(mul)->params["b"] = 1; g.connect(add, "value", mul, "a"); const QString ad = g.addNode("add", {700, 260}); g.node(ad)->params["b"] = 0.5; g.connect(au, "value", ad, "a"); g.connect(ad, "value", mul, "b"); drive = mul; }
        const QString pal = g.addNode("palette", {980, 60}); g.connect(drive, "value", pal, "t");
        const QString out = g.addNode("output", {1160, 60}); g.connect(pal, "color", out, "color");
        s.graph = g;
        s.title = "Node effect: " + matched.join(" + ");
        s.explanation = "Generated a node graph that warps space and combines " + matched.join(" and ") + (audio ? ", modulated by audio bass." : ".");
    } else {
        s.kind = AiSuggestion::LayerPreset;
        s.effectId = matched.value(0, audio ? "audio_particles" : "plasma");
        s.title = s.effectId;
        s.explanation = QString("Layer preset based on '%1'").arg(s.effectId);
        s.needsValidation = false;
    }
    s.colorA = colours.isEmpty() ? std::array<double, 3>{0.2, 0.5, 1.0} : std::array<double, 3>{colours[0].r, colours[0].g, colours[0].b};
    s.colorB = colours.size() > 1 ? std::array<double, 3>{colours[1].r, colours[1].g, colours[1].b} : std::array<double, 3>{s.colorA[0] * 0.3, s.colorA[1] * 0.3, s.colorA[2] * 0.3 + 0.2};
    s.layerValues["speed"] = speed;
    s.layerValues["opacity"] = 1.0;
    if (audio) { s.layerValues["audio"] = 0.8; s.mappings << "audio level -> layer brightness/scale (layer Audio reactivity = 0.8)"; s.explanation += " Audio reactivity enabled."; }
    if (prompt.contains("big") || prompt.contains("large")) s.layerValues["scale"] = 1.6;
    if (prompt.contains("small") || prompt.contains("tiny")) s.layerValues["scale"] = 0.6;
    if (prompt.contains("spin") || prompt.contains("rotat")) s.layerValues["p2"] = 0.8;
    if (prompt.contains("dense") || prompt.contains("many")) s.layerValues["p1"] = 0.9;
    if (prompt.contains("sparse") || prompt.contains("few")) s.layerValues["p1"] = 0.2;
    return s;
}

bool EffectAssistant::requestExternal(const QString& prompt, const QString& previousError) {
    if (!m_cfg.externalEnabled) { emit externalError("External AI is disabled. Enable it in AI Assistant settings."); return false; }
    const QByteArray key = qgetenv(m_cfg.apiKeyEnv.toUtf8().constData());
    if (key.isEmpty()) { emit externalError(QString("No API key: set the %1 environment variable.").arg(m_cfg.apiKeyEnv)); return false; }
    if (!m_net) m_net = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(m_cfg.endpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", key);
    req.setRawHeader("anthropic-version", "2026-10-21");
    req.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");
    req.setTransferTimeout(120000);
    const QString system =
        "You write GLSL fragment code for the LUMAFLOW media server. Reply with ONLY one GLSL function, no markdown, no prose:\n"
        "vec4 effect(vec2 p, vec2 uv, float t)\n"
        "p is centred aspect-corrected coordinates (about -0.9..0.9 x -0.5..0.5), uv is 0..1, t is time in seconds.\n"
        "Available uniforms: uP (vec4 user params 0..1), uAudio (vec4 bass, mid, treble, level), uBeat (float), uColorA, uColorB (vec3).\n"
        "Available helpers: hash11, hash21, hash22, noise, fbm, hsv2rgb, rgb2hsv, rot2, palette(float), spectrumAt(float); constants PI, TAU.\n"
        "Return alpha 1.0. Keep loops bounded (<= 64 iterations). GLSL 3.30 core.";
    QString user = "Effect description: " + prompt;
    if (!previousError.isEmpty()) user += "\nThe previous attempt failed to compile with:\n" + previousError + "\nFix it.";
    const QJsonObject body{{"model", m_cfg.model}, {"max_tokens", 16000}, {"system", system},
                           {"output_config", QJsonObject{{"effort", "low"}}}, {"fallbacks", "default"},
                           {"messages", QJsonArray{QJsonObject{{"role", "user"}, {"content", user}}}}};
    QNetworkReply* r = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, prompt] {
        r->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
        if (r->error() != QNetworkReply::NoError) { emit externalError(r->errorString() + " " + o["error"].toObject()["message"].toString()); return; }
        if (o["stop_reason"].toString() == "refusal") { emit externalError("The model declined this request."); return; }
        QString text;
        for (auto c : o["content"].toArray()) if (c.toObject()["type"].toString() == "text") text += c.toObject()["text"].toString();
        text.remove(QRegularExpression("```(glsl)?")).replace("```", "");
        if (!text.contains("vec4 effect(")) { emit externalError("Response did not contain an effect() function."); return; }
        AiSuggestion s; s.kind = AiSuggestion::ShaderEffect; s.glsl = text.trimmed(); s.title = prompt.left(40);
        s.explanation = "Generated by external model " + m_cfg.model + ". Validate before activating.";
        emit externalResult(s);
    });
    return true;
}

QString EffectAssistant::repairGlsl(const QString& glsl, const QString& log) {
    QString g = glsl;
    // common model/user mistakes: GLSL ES precision qualifiers, gl_FragColor, missing float literals, texture2D
    g.remove(QRegularExpression("precision\\s+\\w+\\s+float\\s*;"));
    g.replace("texture2D(", "texture(");
    g.replace("gl_FragColor", "fragColor");
    g.replace("iTime", "t").replace("iResolution", "uResolution");
    if (log.contains("implicit") || log.contains("cannot convert") || log.contains("no matching")) {
        // turn integer literals in arithmetic into floats: " 2)" -> " 2.0)"
        g.replace(QRegularExpression("(?<![\\w.])(\\d+)(?=\\s*[)*/+\\-,;])(?![.\\d])"), "\\1.0");
        g.replace(QRegularExpression("for\\s*\\(\\s*int\\s+(\\w+)\\s*=\\s*(\\d+)\\.0"), "for (int \\1 = \\2");
        g.replace(QRegularExpression("(<|<=|>|>=)\\s*(\\d+)\\.0\\s*;"), "\\1 \\2;");
    }
    return g;
}

} // namespace lf
