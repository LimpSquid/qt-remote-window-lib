#include "remotewindowserver.h"
#include "remotewindowsocket.h"
#include <QWindow>
#include <QWindow>
#include <QScreen>
#include <QBuffer>

const int RemoteWindowServer::WINDOW_UPDATE_TIME_INTERVAL = 50; // 20fps

RemoteWindowServer::RemoteWindowServer(QWindow *window, unsigned short port) :
    QTcpServer(window)
{
    window_ = window;
    windowUpdateTimerId_ = -1;

    listen(QHostAddress::Any, port);
}

RemoteWindowServer::~RemoteWindowServer()
{

}

void RemoteWindowServer::incomingConnection(qintptr handle)
{
    if(sockets_.contains(handle)) {
        sockets_[handle]->deleteLater();
        sockets_.remove(handle);
    }

    RemoteWindowSocket *socket = new RemoteWindowSocket(handle, this);
    QObject::connect(socket, &RemoteWindowSocket::disconnected, this, &RemoteWindowServer::socketDisconnected);

    sockets_.insert(handle, socket);

    if(-1 == windowUpdateTimerId_)
        windowUpdateTimerId_ = startTimer(WINDOW_UPDATE_TIME_INTERVAL);
}

void RemoteWindowServer::timerEvent(QTimerEvent *event)
{
    if(event->timerId() == windowUpdateTimerId_)
        handleWindowUpdate();
}

void RemoteWindowServer::handleWindowUpdate()
{
    QScreen *screen = window_->screen();
    WId windowId = window_->winId();
    QByteArray data;
    QBuffer buffer(&data);

    if(!buffer.open(QBuffer::WriteOnly))
        return;

    QPixmap pixmap = screen->grabWindow(windowId);
    if(!pixmap.save(&buffer, "jpg"))
        return;

    for(RemoteWindowSocket *socket : sockets_)
        socket->sendWindowCapture(data);
}

void RemoteWindowServer::socketDisconnected()
{
    RemoteWindowSocket *socket = static_cast<RemoteWindowSocket *>(QObject::sender());
    qintptr handle = socket->socketDescriptor();

    socket->deleteLater();
    sockets_.remove(handle);

    if(sockets_.isEmpty()) {
        killTimer(windowUpdateTimerId_);
        windowUpdateTimerId_ = -1;
    }
}
