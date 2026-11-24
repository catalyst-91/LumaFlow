#pragma once
// Optional local-network remote control: a minimal HTTP/1.1 server serving a
// touch-friendly web UI and a JSON REST API, with token authentication and
// role-based permissions. Runs in its own thread; the renderer never depends
// on it and keeps running when clients disconnect.
#include <QObject>
#include <QTcpServer>
#include <QJsonObject>
#include <QHash>
#include <QMutex>
#include <QDateTime>
#include <functional>

class QThread;

namespace lf {

enum class RemoteRole { None = 0, Viewer = 1, Operator = 2, Admin = 3 };
QString remoteRoleName(RemoteRole r);

struct HttpRequest { QString method; QString path; QHash<QString, QString> query; QHash<QString, QString> headers; QByteArray body; QString peer; RemoteRole role = RemoteRole::None; QString user; };
struct HttpResponse { int status = 200; QByteArray contentType = "application/json"; QByteArray body; };

struct RemoteClientInfo { QString peer; QString user; QDateTime lastSeen; quint64 requests = 0; };

class RemoteServer : public QObject {
    Q_OBJECT
public:
    using ApiHandler = std::function<HttpResponse(const HttpRequest&)>;
    RemoteServer();
    ~RemoteServer() override;
    // Users: name -> (token, role). The admin token is generated on first start.
    void setUsers(const QHash<QString, QPair<QString, RemoteRole>>& users);
    QHash<QString, QPair<QString, RemoteRole>> users() const;
    void setApiHandler(ApiHandler h);            // called on the server thread
    bool start(quint16 port);
    void stop();
    bool running() const { return m_running; }
    quint16 port() const { return m_port; }
    QVector<RemoteClientInfo> clients() const;
    static QString generateToken();
    static RemoteRole requiredRole(const QString& method, const QString& path);
private slots:
    void onNewConnection();
private:
    void handle(class QTcpSocket* s);
    QThread* m_thread = nullptr;
    QTcpServer* m_server = nullptr;
    mutable QMutex m_mutex;
    QHash<QString, QPair<QString, RemoteRole>> m_users;
    QHash<QString, RemoteClientInfo> m_clients;
    ApiHandler m_api;
    bool m_running = false;
    quint16 m_port = 0;
};

} // namespace lf
