#pragma once
// Multi-instance synchronisation over UDP (multicast or unicast).
// Primary instances broadcast heartbeats (show clock, timecode, scene) and
// parameter deltas with sequence numbers; secondaries apply them, detect gaps
// and request full snapshots, estimate clock offset from ping/pong round trips
// and recover automatically after disconnects. If two primaries are detected
// the one with the lower instance id stays primary (deterministic resolution).
// Timing: typical LAN alignment is a few milliseconds; this is NOT a
// frame-accurate genlock (see documentation).
#include <QObject>
#include <QHostAddress>
#include <QUuid>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QMutex>
#include <functional>

class QUdpSocket; class QTimer; class QThread;

namespace lf {

enum class SyncRole { Off, Primary, Secondary };
QString syncRoleName(SyncRole r);

struct SyncStatus {
    SyncRole role = SyncRole::Off;
    bool connected = false;
    QString peer;
    double clockOffsetMs = 0, rttMs = 0;
    bool rttValid = false;
    quint64 packetsIn = 0, packetsOut = 0, gaps = 0, snapshots = 0;
    int peers = 0;
    QString lastEvent;
};

class SyncService : public QObject {
    Q_OBJECT
public:
    struct Callbacks {
        std::function<QJsonObject()> snapshot;                 // primary: full state
        std::function<void(const QJsonObject&)> applySnapshot; // secondary
        std::function<void(const QString&, double)> applyParam;
        std::function<void(const QJsonObject&)> applyEvent;    // scene recall, cue go, ...
        std::function<double()> showTime;                      // primary clock (s)
    };
    SyncService();
    ~SyncService() override;
    void setCallbacks(Callbacks cb) { QMutexLocker l(&m_mutex); m_cb = std::move(cb); }
    bool start(SyncRole role, quint16 port = 7700, const QHostAddress& group = QHostAddress("239.255.77.77"), const QList<QHostAddress>& unicast = {}, quint16 peerPort = 0);
    void stop();
    // primary -> secondaries (thread-safe)
    void publishParam(const QString& id, double value);
    void publishEvent(const QJsonObject& ev);
    SyncStatus status() const;
    QUuid instanceId() const { return m_id; }
    double primaryTime() const;   // secondary: estimated primary show time
signals:
    void roleChanged(int role);
private slots:
    void onRead();
    void onTick();
    void sendRaw(const QByteArray& data);
private:
    void send(QJsonObject o);
    QThread* m_thread = nullptr;
    QThread* m_owner = nullptr;
    QUdpSocket* m_sock = nullptr;
    QTimer* m_timer = nullptr;
    QUuid m_id = QUuid::createUuid();
    QHostAddress m_group; QList<QHostAddress> m_unicast; quint16 m_port = 7700, m_peerPort = 7700;
    mutable QMutex m_mutex;
    Callbacks m_cb;
    SyncStatus m_status;
    quint64 m_seq = 0, m_lastRemoteSeq = 0;
    QElapsedTimer m_clock;
    qint64 m_lastPeerMs = -100000, m_lastPingMs = 0;
    double m_primaryTimeAtRx = 0; qint64 m_rxLocalMs = 0;
};

} // namespace lf
