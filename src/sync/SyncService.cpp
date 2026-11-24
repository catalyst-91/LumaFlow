#include "SyncService.h"
#include "core/Log.h"
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include <QJsonDocument>
#include <QNetworkDatagram>

namespace lf {

QString syncRoleName(SyncRole r) { return r == SyncRole::Primary ? "primary" : r == SyncRole::Secondary ? "secondary" : "off"; }

SyncService::SyncService() : m_owner(QThread::currentThread()) { m_clock.start(); }
SyncService::~SyncService() { stop(); }

bool SyncService::start(SyncRole role, quint16 port, const QHostAddress& group, const QList<QHostAddress>& unicast) {
    stop();
    if (role == SyncRole::Off) return true;
    m_group = group; m_unicast = unicast; m_port = port;
    { QMutexLocker l(&m_mutex); m_status = SyncStatus{}; m_status.role = role; }
    m_thread = new QThread; m_thread->setObjectName("lf-sync");
    moveToThread(m_thread);
    bool ok = false;
    m_thread->start();
    QMetaObject::invokeMethod(this, [&] {
        m_sock = new QUdpSocket(this);
        ok = m_sock->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        if (ok && m_unicast.isEmpty()) { m_sock->joinMulticastGroup(m_group); m_sock->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1); }
        connect(m_sock, &QUdpSocket::readyRead, this, &SyncService::onRead);
        m_timer = new QTimer(this); connect(m_timer, &QTimer::timeout, this, &SyncService::onTick); m_timer->start(100);
    }, Qt::BlockingQueuedConnection);
    if (!ok) logWarn("sync", QString("cannot bind sync port %1").arg(port));
    else logInfo("sync", QString("Sync started as %1 on port %2").arg(syncRoleName(role)).arg(port));
    if (role == SyncRole::Secondary) send({{"type", "snapshotReq"}});
    return ok;
}

void SyncService::stop() {
    if (!m_thread) return;
    QMetaObject::invokeMethod(this, [this] { if (m_timer) m_timer->stop(); delete m_sock; m_sock = nullptr; delete m_timer; m_timer = nullptr; moveToThread(m_owner); }, Qt::BlockingQueuedConnection);
    m_thread->quit(); m_thread->wait(2000); delete m_thread; m_thread = nullptr;
    QMutexLocker l(&m_mutex); m_status.role = SyncRole::Off; m_status.connected = false;
}

void SyncService::sendRaw(const QByteArray& data) {
    if (!m_sock) return;
    if (m_unicast.isEmpty()) m_sock->writeDatagram(data, m_group, m_port);
    else for (auto& h : m_unicast) m_sock->writeDatagram(data, h, m_port);
    QMutexLocker l(&m_mutex); ++m_status.packetsOut;
}

void SyncService::send(QJsonObject o) {
    o["from"] = m_id.toString(QUuid::WithoutBraces);
    { QMutexLocker l(&m_mutex); o["role"] = syncRoleName(m_status.role); }
    o["seq"] = qint64(++m_seq);
    const QByteArray data = QJsonDocument(o).toJson(QJsonDocument::Compact);
    if (QThread::currentThread() == thread()) sendRaw(data);
    else QMetaObject::invokeMethod(this, "sendRaw", Qt::QueuedConnection, Q_ARG(QByteArray, data));
}

void SyncService::publishParam(const QString& id, double v) {
    if (!m_thread) return;
    { QMutexLocker l(&m_mutex); if (m_status.role != SyncRole::Primary) return; }
    send({{"type", "param"}, {"id", id}, {"v", v}});
}

void SyncService::publishEvent(const QJsonObject& ev) {
    if (!m_thread) return;
    { QMutexLocker l(&m_mutex); if (m_status.role != SyncRole::Primary) return; }
    send({{"type", "event"}, {"event", ev}});
}

SyncStatus SyncService::status() const { QMutexLocker l(&m_mutex); return m_status; }

double SyncService::primaryTime() const {
    QMutexLocker l(&m_mutex);
    return m_primaryTimeAtRx + (m_clock.elapsed() - m_rxLocalMs) / 1000.0;
}

void SyncService::onTick() {
    const qint64 now = m_clock.elapsed();
    SyncRole role; Callbacks cb;
    { QMutexLocker l(&m_mutex); role = m_status.role; cb = m_cb; }
    if (role == SyncRole::Primary) send({{"type", "hb"}, {"time", cb.showTime ? cb.showTime() : now / 1000.0}});
    if (role == SyncRole::Secondary && now - m_lastPingMs > 1000) { m_lastPingMs = now; send({{"type", "ping"}, {"t0", double(now)}}); }
    QMutexLocker l(&m_mutex);
    const bool was = m_status.connected;
    m_status.connected = now - m_lastPeerMs < 2000;
    if (was && !m_status.connected) { m_status.lastEvent = "peer lost - holding last state"; logWarn("sync", m_status.lastEvent); }
    if (!was && m_status.connected && role == SyncRole::Secondary && m_status.snapshots > 0) {
        m_status.lastEvent = "reconnected - requesting snapshot";
        l.unlock(); send({{"type", "snapshotReq"}}); return;
    }
}

void SyncService::onRead() {
    while (m_sock && m_sock->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_sock->receiveDatagram();
        const QJsonObject o = QJsonDocument::fromJson(d.data()).object();
        const QString from = o["from"].toString();
        if (from.isEmpty() || from == m_id.toString(QUuid::WithoutBraces)) continue;   // own multicast loopback
        const QString type = o["type"].toString();
        const qint64 now = m_clock.elapsed();
        SyncRole role; Callbacks cb;
        { QMutexLocker l(&m_mutex); role = m_status.role; cb = m_cb; ++m_status.packetsIn; }
        const QString remoteRole = o["role"].toString();
        if (remoteRole == "primary" && role == SyncRole::Primary) {
            // two primaries: lower id wins
            if (QUuid(from) < m_id) {
                { QMutexLocker l(&m_mutex); m_status.role = SyncRole::Secondary; m_status.lastEvent = "conflict: another primary with lower id - demoted to secondary"; }
                logWarn("sync", "primary conflict - demoted to secondary");
                emit roleChanged(int(SyncRole::Secondary));
                send({{"type", "snapshotReq"}});
            }
            continue;
        }
        if (role == SyncRole::Primary) {
            if (type == "snapshotReq" && cb.snapshot) send({{"type", "snapshot"}, {"state", cb.snapshot()}});
            else if (type == "ping") send({{"type", "pong"}, {"t0", o["t0"]}, {"time", cb.showTime ? cb.showTime() : 0.0}, {"to", from}});
            QMutexLocker l(&m_mutex); m_lastPeerMs = now; m_status.peer = d.senderAddress().toString(); m_status.peers = 1;
            continue;
        }
        if (role != SyncRole::Secondary || remoteRole != "primary") continue;
        const quint64 seq = quint64(o["seq"].toInteger());
        {
            QMutexLocker l(&m_mutex);
            m_lastPeerMs = now; m_status.peer = d.senderAddress().toString(); m_status.peers = 1;
            if (m_lastRemoteSeq && seq > m_lastRemoteSeq + 1 && type == "param") ++m_status.gaps;
        }
        const bool gap = m_lastRemoteSeq && seq > m_lastRemoteSeq + 1;
        m_lastRemoteSeq = std::max(m_lastRemoteSeq, seq);
        if (type == "hb") { QMutexLocker l(&m_mutex); m_primaryTimeAtRx = o["time"].toDouble() + m_status.rttMs / 2000.0; m_rxLocalMs = now; }
        else if (type == "param" && cb.applyParam) { cb.applyParam(o["id"].toString(), o["v"].toDouble()); if (gap) send({{"type", "snapshotReq"}}); }
        else if (type == "event" && cb.applyEvent) cb.applyEvent(o["event"].toObject());
        else if (type == "snapshot" && cb.applySnapshot) { cb.applySnapshot(o["state"].toObject()); QMutexLocker l(&m_mutex); ++m_status.snapshots; m_status.lastEvent = "snapshot applied"; }
        else if (type == "pong" && o["to"].toString() == m_id.toString(QUuid::WithoutBraces)) {
            const double rtt = now - o["t0"].toDouble();
            QMutexLocker l(&m_mutex);
            m_status.rttMs = m_status.rttMs > 0 ? m_status.rttMs * 0.8 + rtt * 0.2 : rtt;
            const double primaryNow = o["time"].toDouble() + rtt / 2000.0;
            m_status.clockOffsetMs = (primaryNow - now / 1000.0) * 1000.0;
        }
    }
}

} // namespace lf
