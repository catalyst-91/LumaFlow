#include "RemoteServer.h"
#include "core/Log.h"
#include <QTcpSocket>
#include <QThread>
#include <QFile>
#include <QUrlQuery>
#include <QUrl>
#include <QRandomGenerator>
#include <QJsonDocument>

namespace lf {

QString remoteRoleName(RemoteRole r) { switch (r) { case RemoteRole::Viewer: return "viewer"; case RemoteRole::Operator: return "operator"; case RemoteRole::Admin: return "admin"; default: return "none"; } }

RemoteServer::RemoteServer() {}
RemoteServer::~RemoteServer() { stop(); }

QString RemoteServer::generateToken() {
    QByteArray b(16, 0);
    for (auto& c : b) c = char(QRandomGenerator::system()->bounded(256));
    return b.toHex();
}

void RemoteServer::setUsers(const QHash<QString, QPair<QString, RemoteRole>>& u) { QMutexLocker l(&m_mutex); m_users = u; }
QHash<QString, QPair<QString, RemoteRole>> RemoteServer::users() const { QMutexLocker l(&m_mutex); return m_users; }
void RemoteServer::setApiHandler(ApiHandler h) { QMutexLocker l(&m_mutex); m_api = std::move(h); }
QVector<RemoteClientInfo> RemoteServer::clients() const { QMutexLocker l(&m_mutex); return m_clients.values().toVector(); }

RemoteRole RemoteServer::requiredRole(const QString& method, const QString& path) {
    if (path == "/" || path == "/index.html" || path == "/api/ping") return RemoteRole::None;
    if (method == "GET") return RemoteRole::Viewer;
    if (path.startsWith("/api/blackout") || path.startsWith("/api/config") || path.startsWith("/api/output")) return RemoteRole::Admin;
    return RemoteRole::Operator;
}

bool RemoteServer::start(quint16 port) {
    if (m_running) stop();
    m_thread = new QThread; m_thread->setObjectName("lf-remote");
    m_server = new QTcpServer;
    m_server->moveToThread(m_thread);
    bool ok = false;
    m_thread->start();
    QMetaObject::invokeMethod(m_server, [&] {
        ok = m_server->listen(QHostAddress::AnyIPv4, port);
        if (ok) QObject::connect(m_server, &QTcpServer::newConnection, m_server, [this] { onNewConnection(); });
    }, Qt::BlockingQueuedConnection);
    if (!ok) { logWarn("remote", QString("cannot listen on port %1").arg(port)); stop(); return false; }
    m_port = m_server->serverPort(); m_running = true;
    logInfo("remote", QString("Remote control server listening on port %1").arg(m_port));
    return true;
}

void RemoteServer::stop() {
    if (!m_thread) return;
    QMetaObject::invokeMethod(m_server, [this] { m_server->close(); delete m_server; m_server = nullptr; }, Qt::BlockingQueuedConnection);
    m_thread->quit(); m_thread->wait(2000); delete m_thread; m_thread = nullptr;
    m_running = false;
}

void RemoteServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* s = m_server->nextPendingConnection();
        auto buf = std::make_shared<QByteArray>();
        QObject::connect(s, &QTcpSocket::readyRead, s, [this, s, buf] {
            buf->append(s->readAll());
            if (buf->size() > 1024 * 1024) { s->abort(); return; }               // bounded request size
            const int he = buf->indexOf("\r\n\r\n");
            if (he < 0) return;
            const QByteArray head = buf->left(he);
            int len = 0;
            for (const auto& line : head.split('\n')) if (line.toLower().startsWith("content-length:")) len = line.mid(15).trimmed().toInt();
            if (buf->size() < he + 4 + len) return;
            s->setProperty("req", *buf);
            handle(s);
            buf->clear();
        });
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    }
}

static QByteArray statusText(int s) { switch (s) { case 200: return "OK"; case 400: return "Bad Request"; case 401: return "Unauthorized"; case 403: return "Forbidden"; case 404: return "Not Found"; case 503: return "Service Unavailable"; default: return "Error"; } }

void RemoteServer::handle(QTcpSocket* s) {
    const QByteArray raw = s->property("req").toByteArray();
    const int he = raw.indexOf("\r\n\r\n");
    const QList<QByteArray> lines = raw.left(he).split('\n');
    HttpRequest req;
    const QList<QByteArray> rl = lines.value(0).trimmed().split(' ');
    req.method = QString::fromLatin1(rl.value(0));
    const QUrl url(QString::fromUtf8(rl.value(1)));
    req.path = url.path();
    for (auto& q : QUrlQuery(url).queryItems(QUrl::FullyDecoded)) req.query[q.first] = q.second;
    for (int i = 1; i < lines.size(); ++i) { const int c = lines[i].indexOf(':'); if (c > 0) req.headers[QString::fromLatin1(lines[i].left(c)).toLower()] = QString::fromUtf8(lines[i].mid(c + 1)).trimmed(); }
    req.body = raw.mid(he + 4);
    req.peer = s->peerAddress().toString();
    // authenticate
    QString token = req.query.value("token");
    const QString auth = req.headers.value("authorization");
    if (auth.startsWith("Bearer ", Qt::CaseInsensitive)) token = auth.mid(7).trimmed();
    ApiHandler api;
    {
        QMutexLocker l(&m_mutex);
        for (auto it = m_users.begin(); it != m_users.end(); ++it) if (!token.isEmpty() && it->first == token) { req.role = it->second; req.user = it.key(); }
        auto& ci = m_clients[req.peer]; ci.peer = req.peer; ci.user = req.user; ci.lastSeen = QDateTime::currentDateTime(); ++ci.requests;
        api = m_api;
    }
    HttpResponse resp;
    const RemoteRole need = requiredRole(req.method, req.path);
    if (req.role < need) {
        resp.status = req.role == RemoteRole::None ? 401 : 403;
        resp.body = QJsonDocument(QJsonObject{{"error", req.role == RemoteRole::None ? "authentication required" : "insufficient role"}, {"required", remoteRoleName(need)}}).toJson(QJsonDocument::Compact);
    } else if (req.path == "/" || req.path == "/index.html") {
        QFile f(":/lumaflow/remote/index.html");
        if (f.open(QIODevice::ReadOnly)) { resp.contentType = "text/html; charset=utf-8"; resp.body = f.readAll(); } else resp.status = 404;
    } else if (req.path == "/api/ping") {
        resp.body = R"({"ok":true,"app":"LUMAFLOW"})";
    } else if (api) {
        resp = api(req);
    } else { resp.status = 503; resp.body = R"({"error":"engine unavailable"})"; }
    QByteArray out = "HTTP/1.1 " + QByteArray::number(resp.status) + " " + statusText(resp.status) + "\r\n";
    out += "Content-Type: " + resp.contentType + "\r\nContent-Length: " + QByteArray::number(resp.body.size()) + "\r\n";
    out += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: keep-alive\r\n\r\n" + resp.body;
    s->write(out);
}

} // namespace lf
