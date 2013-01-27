#include "pserver.h"
#include <QTcpSocket>
#include <QByteArray>
#include <QStringList>
#include <QHostAddress>
#include "pthreadpool.h"
#include "psocket.h"
#include "psaver.h"
#include "logger.h"
#include <QDateTime>

pServer::pServer(QObject *parent):
    QObject(parent)
{
    connect(&_server, SIGNAL(newConnection()), this, SLOT(onConnection()));
    _server.listen(Settings::host(), Settings::port());

    qDebug() << "\n\nServer started. Time:" << QDateTime::currentDateTime().toString() << '\n';
#ifdef FUNC_DEBUG
    qWarning() << "FuncDebug activated";
#endif
    _timeLeft = 0;
    startTimer(60000);
}

void pServer::onConnection()
{
    auto socket = _server.nextPendingConnection();

    auto addr = socket->localAddress();
    auto it = _limits.find(addr);
    if (it == _limits.end()) {
        it = _limits.insert(addr, 0);
    } else {
        if (it.value().loadAcquire() > pServer::LIMIT) {
            socket->disconnectFromHost();
            socket->deleteLater();
            return;
        }
    }
//    qDebug() << "Connected: " << socket->peerAddress().toString() << " with used limit " << it.value().loadAcquire() << " bytes";
    new pSocket(socket, pThreadPool::getNextThread(), it.value());
    Logger::log(socket->peerAddress().toString() + socket->peerName(), it.value().loadAcquire());
    // UDebug << "asdfads";
}

// invoked once per minute
void pServer::timerEvent(QTimerEvent *)
{
    _timeLeft++;
    if (_timeLeft > 8640) { // Ip limits clear once a day
        _limits.clear();
        _timeLeft = 0;
    }
}
