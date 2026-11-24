#include "NetworkIO.h"
#include "ArtNet.h"
#include "Sacn.h"
#include "core/Log.h"
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include <QNetworkDatagram>
#include <QJsonArray>
#include <QNetworkInterface>

namespace lf {

QJsonObject NetworkConfig::toJson() const {
    QJsonArray outU, inU, sacnT, oscT;
    for (int u : enabledOutputUniverses) outU << u;
    for (int u : inputUniverses) inU << u;
    for (auto& h : sacnUnicastTargets) sacnT << h.toString();
    for (auto& t : oscFeedbackTargets) oscT << QString("%1:%2").arg(t.first.toString()).arg(t.second);
    return {{"artnetOut", artnetOut}, {"artnetIn", artnetIn}, {"sacnOut", sacnOut}, {"sacnIn", sacnIn},
            {"oscIn", oscIn}, {"bind", bindAddress.toString()}, {"artnetTarget", artnetTarget.toString()},
            {"sacnUnicast", sacnT}, {"artnetUniverseOffset", artnetUniverseOffset}, {"sacnPriority", sacnPriority},
            {"sourceName", sourceName}, {"outputRateHz", outputRateHz}, {"keepAliveSec", keepAliveSec},
            {"outputUniverses", outU}, {"inputUniverses", inU}, {"oscListenPort", oscListenPort}, {"oscFeedback", oscT}};
}

NetworkConfig NetworkConfig::fromJson(const QJsonObject& o) {
    NetworkConfig c;
    c.artnetOut = o["artnetOut"].toBool(); c.artnetIn = o["artnetIn"].toBool();
    c.sacnOut = o["sacnOut"].toBool(); c.sacnIn = o["sacnIn"].toBool(); c.oscIn = o["oscIn"].toBool(true);
    if (o.contains("bind")) c.bindAddress = QHostAddress(o["bind"].toString());
    if (o.contains("artnetTarget")) c.artnetTarget = QHostAddress(o["artnetTarget"].toString());
    for (auto v : o["sacnUnicast"].toArray()) c.sacnUnicastTargets << QHostAddress(v.toString());
    c.artnetUniverseOffset = o["artnetUniverseOffset"].toInt(1);
    c.sacnPriority = o["sacnPriority"].toInt(100); c.sourceName = o["sourceName"].toString("LUMAFLOW");
    c.outputRateHz = o["outputRateHz"].toDouble(40); c.keepAliveSec = o["keepAliveSec"].toDouble(1.0);
    for (auto v : o["outputUniverses"].toArray()) c.enabledOutputUniverses << v.toInt();
    for (auto v : o["inputUniverses"].toArray()) c.inputUniverses << v.toInt();
    c.oscListenPort = o["oscListenPort"].toInt(8000);
    for (auto v : o["oscFeedback"].toArray()) {
        const QString s = v.toString(); const int i = s.lastIndexOf(':');
        if (i > 0) c.oscFeedbackTargets << qMakePair(QHostAddress(s.left(i)), quint16(s.mid(i + 1).toUInt()));
    }
    return c;
}

NetworkIO::NetworkIO() : m_owner(QThread::currentThread()) { m_clock.start(); }

NetworkIO::~NetworkIO() { stop(); }

void NetworkIO::start() {
    if (m_thread) return;
    m_thread = new QThread;
    m_thread->setObjectName("lf-network");
    moveToThread(m_thread);
    connect(m_thread, &QThread::started, this, &NetworkIO::openSockets);
    m_thread->start();
}

void NetworkIO::stop() {
    if (!m_thread) return;
    QMetaObject::invokeMethod(this, [this] { closeSockets(); delete m_timer; m_timer = nullptr; moveToThread(m_owner); }, Qt::BlockingQueuedConnection);
    QThread* t = m_thread; m_thread = nullptr;
    t->quit(); t->wait(3000); delete t;
}

void NetworkIO::applyConfig(const NetworkConfig& c) {
    { QMutexLocker l(&m_mutex); m_cfg = c; for (auto& u : m_out) u.dirty = true; }
    if (m_thread) QMetaObject::invokeMethod(this, [this] { closeSockets(); openSockets(); }, Qt::QueuedConnection);
}

NetworkConfig NetworkIO::config() const { QMutexLocker l(&m_mutex); return m_cfg; }

void NetworkIO::openSockets() {
    NetworkConfig c = config();
    auto bindUdp = [&](quint16 port, const char* what) -> QUdpSocket* {
        auto* s = new QUdpSocket(this);
        if (!s->bind(c.bindAddress, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            const QString msg = QString("%1: cannot bind UDP port %2 (%3)").arg(what).arg(port).arg(s->errorString());
            logWarn("network", msg); emit networkError(msg);
            QMutexLocker l(&m_mutex); m_stats.warnings << msg; ++m_stats.errors;
        }
        return s;
    };
    if (c.artnetIn || c.artnetOut) {
        m_artnet = c.artnetIn ? bindUdp(artnet::kPort, "Art-Net") : new QUdpSocket(this);
        if (!c.artnetIn) m_artnet->bind(c.bindAddress, 0);
        connect(m_artnet, &QUdpSocket::readyRead, this, &NetworkIO::readArtnet);
    }
    if (c.sacnOut) { m_sacnTx = new QUdpSocket(this); m_sacnTx->bind(c.bindAddress, 0); m_sacnTx->setSocketOption(QAbstractSocket::MulticastTtlOption, 8); }
    if (c.sacnIn) {
        m_sacnRx = bindUdp(sacn::kPort, "sACN");
        for (int u : c.inputUniverses) if (u >= 1 && u <= 63999) m_sacnRx->joinMulticastGroup(sacn::multicastAddress(quint16(u)));
        connect(m_sacnRx, &QUdpSocket::readyRead, this, &NetworkIO::readSacn);
    }
    if (c.oscIn) { m_osc = bindUdp(quint16(c.oscListenPort), "OSC"); connect(m_osc, &QUdpSocket::readyRead, this, &NetworkIO::readOsc); }
    else { m_osc = new QUdpSocket(this); m_osc->bind(c.bindAddress, 0); }
    if (!m_timer) { m_timer = new QTimer(this); m_timer->setTimerType(Qt::PreciseTimer); connect(m_timer, &QTimer::timeout, this, &NetworkIO::tick); }
    m_timer->start(std::max(1, int(1000.0 / std::clamp(c.outputRateHz, 1.0, 120.0))));
    logInfo("network", QString("Network I/O started (Art-Net out=%1 in=%2, sACN out=%3 in=%4, OSC port=%5)")
        .arg(c.artnetOut).arg(c.artnetIn).arg(c.sacnOut).arg(c.sacnIn).arg(c.oscIn ? c.oscListenPort : 0));
}

void NetworkIO::closeSockets() {
    if (m_timer) m_timer->stop();
    for (QUdpSocket** s : {&m_artnet, &m_sacnTx, &m_sacnRx, &m_osc}) { if (*s) { (*s)->close(); (*s)->deleteLater(); *s = nullptr; } }
}

void NetworkIO::setOutput(int universe, const DmxFrame& frame) {
    QMutexLocker l(&m_mutex);
    auto& u = m_out[universe];
    if (u.frame != frame) { u.frame = frame; u.dirty = true; }
}

void NetworkIO::clearOutputs() { QMutexLocker l(&m_mutex); for (auto& u : m_out) { u.frame.fill(0); u.dirty = true; } }

void NetworkIO::blackout(bool on) { m_blackout = on; QMutexLocker l(&m_mutex); for (auto& u : m_out) u.dirty = true; }

void NetworkIO::tick() {
    const qint64 now = m_clock.elapsed();
    struct Pending { int universe; DmxFrame frame; quint8 artSeq, sacnSeq; };
    QVector<Pending> send;
    NetworkConfig c;
    {
        QMutexLocker l(&m_mutex);
        c = m_cfg;
        for (auto it = m_out.begin(); it != m_out.end(); ++it) {
            if (!c.enabledOutputUniverses.contains(it.key())) continue;          // never send to unconfigured universes
            auto& u = it.value();
            if (!u.dirty && now - u.lastSentMs < qint64(c.keepAliveSec * 1000)) continue;
            u.dirty = false; u.lastSentMs = now;
            u.artSeq = quint8(u.artSeq % 255 + 1); u.sacnSeq = quint8(u.sacnSeq + 1);
            Pending p{it.key(), u.frame, u.artSeq, u.sacnSeq};
            if (m_blackout) p.frame.fill(0);
            send << p;
        }
    }
    for (const auto& p : send) {
        if (c.artnetOut && m_artnet) {
            const int pa = p.universe - c.artnetUniverseOffset;
            if (pa >= 0 && pa < 32768) {
                artnet::DmxPacket pkt; pkt.portAddress = quint16(pa); pkt.sequence = p.artSeq; pkt.data = dmxToBytes(p.frame);
                if (m_artnet->writeDatagram(artnet::encodeDmx(pkt), c.artnetTarget, artnet::kPort) < 0) { QMutexLocker l(&m_mutex); ++m_stats.errors; }
                else { QMutexLocker l(&m_mutex); ++m_stats.artnetOut; ++m_stats.universes[p.universe].packetsOut; }
            }
        }
        if (c.sacnOut && m_sacnTx && p.universe >= 1 && p.universe <= 63999) {
            sacn::DataPacket pkt; pkt.cid = m_cid; pkt.sourceName = c.sourceName; pkt.priority = quint8(c.sacnPriority);
            pkt.sequence = p.sacnSeq; pkt.universe = quint16(p.universe); pkt.data = dmxToBytes(p.frame);
            const QByteArray bytes = sacn::encode(pkt);
            bool ok = true;
            if (c.sacnUnicastTargets.isEmpty()) ok = m_sacnTx->writeDatagram(bytes, sacn::multicastAddress(pkt.universe), sacn::kPort) >= 0;
            else for (auto& h : c.sacnUnicastTargets) ok &= m_sacnTx->writeDatagram(bytes, h, sacn::kPort) >= 0;
            QMutexLocker l(&m_mutex);
            if (ok) { ++m_stats.sacnOut; ++m_stats.universes[p.universe].packetsOut; } else ++m_stats.errors;
        }
    }
    if (now - m_lastRateMs >= 1000) {
        QMutexLocker l(&m_mutex);
        const double dt = (now - m_lastRateMs) / 1000.0;
        for (auto it = m_stats.universes.begin(); it != m_stats.universes.end(); ++it) {
            it->rateOut = (it->packetsOut - m_lastCountOut.value(it.key())) / dt;
            it->rateIn = (it->packetsIn - m_lastCountIn.value(it.key())) / dt;
            m_lastCountOut[it.key()] = it->packetsOut; m_lastCountIn[it.key()] = it->packetsIn;
        }
        m_lastRateMs = now;
    }
}

void NetworkIO::storeInput(int universe, const QString& source, int priority, const QByteArray& data) {
    const qint64 now = m_clock.elapsed();
    {
        QMutexLocker l(&m_mutex);
        auto& src = m_in[universe][source];
        bytesToDmx(data, src.frame); src.priority = priority; src.lastMs = now;
        auto& st = m_stats.universes[universe]; ++st.packetsIn; st.lastSource = source; st.lastInMs = now;
    }
    emit dmxInput(universe);
}

bool NetworkIO::inputFrame(int universe, DmxFrame& out) const {
    QMutexLocker l(&m_mutex);
    auto it = m_in.find(universe);
    if (it == m_in.end() || it->isEmpty()) return false;
    const qint64 now = m_clock.elapsed();
    // sACN-style merge: highest priority among live sources (2.5 s timeout); HTP among equal priority.
    int best = -1;
    for (auto& s : *it) if (now - s.lastMs < 2500) best = std::max(best, s.priority);
    if (best < 0) return false;
    out.fill(0);
    for (auto& s : *it) {
        if (now - s.lastMs >= 2500 || s.priority != best) continue;
        for (int i = 0; i < kDmxSlots; ++i) out[i] = std::max(out[i], s.frame[i]);
    }
    return true;
}

NetworkStats NetworkIO::stats() const { QMutexLocker l(&m_mutex); return m_stats; }

void NetworkIO::readArtnet() {
    const NetworkConfig c = config();
    while (m_artnet && m_artnet->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_artnet->receiveDatagram();
        const QByteArray pkt = d.data();
        const quint16 op = artnet::opcode(pkt);
        { QMutexLocker l(&m_mutex); ++m_stats.artnetIn; }
        if (op == artnet::kOpDmx && c.artnetIn) {
            artnet::DmxPacket p;
            if (!artnet::decodeDmx(pkt, p)) continue;
            const int u = p.portAddress + c.artnetUniverseOffset;
            if (!c.inputUniverses.contains(u)) continue;
            // loop prevention: ignore our own transmission of the same universe
            if (c.artnetOut && c.enabledOutputUniverses.contains(u) && d.senderPort() == m_artnet->localPort()) continue;
            storeInput(u, "artnet:" + d.senderAddress().toString(), 100, p.data);
        } else if (op == artnet::kOpPoll) {
            artnet::PollReply r; r.shortName = "LUMAFLOW"; r.longName = "LUMAFLOW media server " LUMAFLOW_VERSION;
            for (auto& a : QNetworkInterface::allAddresses()) if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()) { r.ip = a; break; }
            r.numPorts = std::min<int>(4, int(c.enabledOutputUniverses.size()));
            m_artnet->writeDatagram(artnet::encodePollReply(r), d.senderAddress(), artnet::kPort);
        }
    }
}

void NetworkIO::readSacn() {
    const NetworkConfig c = config();
    while (m_sacnRx && m_sacnRx->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_sacnRx->receiveDatagram();
        sacn::DataPacket p;
        { QMutexLocker l(&m_mutex); ++m_stats.sacnIn; }
        if (!sacn::decode(d.data(), p) || p.startCode != 0) continue;
        if (p.cid == m_cid) continue;                                        // our own packets
        if (!c.inputUniverses.contains(p.universe) || (p.options & sacn::kOptionPreview)) continue;
        if (p.options & sacn::kOptionStreamTerminated) { QMutexLocker l(&m_mutex); m_in[p.universe].remove("sacn:" + p.cid.toString()); continue; }
        storeInput(p.universe, "sacn:" + p.cid.toString(), p.priority, p.data);
    }
}

void NetworkIO::readOsc() {
    while (m_osc && m_osc->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_osc->receiveDatagram();
        QVector<osc::Message> msgs;
        if (!osc::decode(d.data(), msgs)) { QMutexLocker l(&m_mutex); ++m_stats.errors; continue; }
        { QMutexLocker l(&m_mutex); m_stats.oscIn += msgs.size(); }
        const QString sender = d.senderAddress().toString() + ":" + QString::number(d.senderPort());
        for (const auto& m : msgs) emit oscReceived(m, sender);
    }
}

void NetworkIO::sendOsc(const osc::Message& m, const QHostAddress& host, quint16 port) {
    const QByteArray data = osc::encode(m);
    QMetaObject::invokeMethod(this, "doSendOsc", Qt::QueuedConnection, Q_ARG(QByteArray, data), Q_ARG(QString, host.toString()), Q_ARG(quint16, port));
}

void NetworkIO::sendOscFeedback(const osc::Message& m) {
    for (auto& t : config().oscFeedbackTargets) sendOsc(m, t.first, t.second);
}

void NetworkIO::doSendOsc(const QByteArray& data, const QString& host, quint16 port) {
    if (!m_osc) { m_osc = new QUdpSocket(this); m_osc->bind(QHostAddress::AnyIPv4, 0); }
    if (m_osc->writeDatagram(data, QHostAddress(host), port) >= 0) { QMutexLocker l(&m_mutex); ++m_stats.oscOut; }
    else { QMutexLocker l(&m_mutex); ++m_stats.errors; }
}

} // namespace lf
