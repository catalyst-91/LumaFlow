#pragma once
// Network protocol engine. Owns all UDP sockets (Art-Net, sACN, OSC) and runs
// in its own thread so that network stalls can never block the renderer.
// The renderer/pixel-mapper publishes DMX frames with setOutput(); the timer
// in this thread transmits them at the configured rate, sending only changed
// universes plus a periodic keep-alive refresh.
#include "Dmx.h"
#include "Osc.h"
#include <QObject>
#include <QHostAddress>
#include <QMutex>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QElapsedTimer>
#include <QJsonObject>
#include <atomic>

class QUdpSocket;
class QTimer;
class QThread;

namespace lf {

struct NetworkConfig {
    bool artnetOut = false, artnetIn = false;
    bool sacnOut = false, sacnIn = false;
    bool oscIn = true;
    QHostAddress bindAddress = QHostAddress::AnyIPv4;
    QHostAddress artnetTarget = QHostAddress("2.255.255.255");   // broadcast or unicast node
    QList<QHostAddress> sacnUnicastTargets;                       // empty = multicast
    int artnetUniverseOffset = 1;     // LUMAFLOW universe U -> Art-Net port address U - offset
    int sacnPriority = 100;
    QString sourceName = "LUMAFLOW";
    double outputRateHz = 40.0;       // DMX refresh rate (max 44 for full 512-slot DMX)
    double keepAliveSec = 1.0;        // resend unchanged universes at this interval
    QSet<int> enabledOutputUniverses; // only these are transmitted (safety)
    QSet<int> inputUniverses;         // universes to listen to
    int oscListenPort = 8000;
    QList<QPair<QHostAddress, quint16>> oscFeedbackTargets;
    QJsonObject toJson() const;
    static NetworkConfig fromJson(const QJsonObject& o);
};

struct UniverseStats { quint64 packetsOut = 0, packetsIn = 0; double rateOut = 0, rateIn = 0; QString lastSource; qint64 lastInMs = 0; };

struct NetworkStats {
    quint64 artnetOut = 0, artnetIn = 0, sacnOut = 0, sacnIn = 0, oscIn = 0, oscOut = 0, errors = 0;
    QMap<int, UniverseStats> universes;
    QStringList warnings;
};

class NetworkIO : public QObject {
    Q_OBJECT
public:
    NetworkIO();
    ~NetworkIO() override;

    void start();                         // moves to own thread and opens sockets
    void stop();
    void applyConfig(const NetworkConfig& c);   // thread-safe
    NetworkConfig config() const;

    // Thread-safe publish of an output frame (universe numbers start at 1).
    void setOutput(int universe, const DmxFrame& frame);
    void clearOutputs();
    void blackout(bool on);
    bool isBlackout() const { return m_blackout; }

    bool inputFrame(int universe, DmxFrame& out) const;
    NetworkStats stats() const;
    QUuid cid() const { return m_cid; }

    // Thread-safe OSC transmit (queued to the network thread).
    void sendOsc(const osc::Message& m, const QHostAddress& host, quint16 port);
    void sendOscFeedback(const osc::Message& m);  // to all configured feedback targets

signals:
    void dmxInput(int universe);
    void oscReceived(const lf::osc::Message& m, const QString& sender);
    void networkError(const QString& message);

private slots:
    void openSockets();
    void closeSockets();
    void tick();
    void readArtnet();
    void readSacn();
    void readOsc();
    void doSendOsc(const QByteArray& data, const QString& host, quint16 port);

private:
    struct OutUniverse { DmxFrame frame{}; bool dirty = true; qint64 lastSentMs = -1000000; quint8 artSeq = 0, sacnSeq = 0; };
    struct InSource { DmxFrame frame{}; int priority = 0; qint64 lastMs = 0; };
    void storeInput(int universe, const QString& source, int priority, const QByteArray& data);

    QThread* m_thread = nullptr;
    QThread* m_owner = nullptr;
    QUdpSocket* m_artnet = nullptr;
    QUdpSocket* m_sacnTx = nullptr;
    QUdpSocket* m_sacnRx = nullptr;
    QUdpSocket* m_osc = nullptr;
    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    mutable QMutex m_mutex;
    NetworkConfig m_cfg;
    QMap<int, OutUniverse> m_out;
    QMap<int, QMap<QString, InSource>> m_in;
    NetworkStats m_stats;
    QMap<int, quint64> m_lastCountOut, m_lastCountIn;
    qint64 m_lastRateMs = 0;
    QUuid m_cid = QUuid::createUuid();
    std::atomic<bool> m_blackout{false};
    bool m_reopen = false;
};

} // namespace lf
