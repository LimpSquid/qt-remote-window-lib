#include "remotewindowserver.h"
#include "remotewindowsocket.h"
#include <QWindow>
#include <QWindow>
#include <QScreen>
#include <QBuffer>
#include <QTest>

const int RemoteWindowServer::WINDOW_UPDATE_TIME_INTERVAL = 125; // 8fps

RemoteWindowServer::RemoteWindowServer(QObject *parent, unsigned short port) :
    QTcpServer(parent)
{
    window_ = nullptr;
    screenShotFunction_ = nullptr;
    windowUpdateTimerId_ = -1;
    port_ = port;

}

RemoteWindowServer::RemoteWindowServer(QWindow *window, QObject *parent, unsigned short port) :
    RemoteWindowServer(parent, port)
{
    window_ = window;
}

RemoteWindowServer::~RemoteWindowServer()
{

}

bool RemoteWindowServer::start()
{
    if(isListening())
        return false;

    return listen(QHostAddress::Any, port_);
}

void RemoteWindowServer::stop()
{
    if(!isListening())
        return;

    close();
    qDeleteAll(sockets_);
}

QWindow *RemoteWindowServer::window() const
{
    return window_;
}

void RemoteWindowServer::setWindow(QWindow *value)
{
    window_ = value;
}

unsigned short RemoteWindowServer::port() const
{
    return port_;
}

void RemoteWindowServer::setPort(unsigned short port)
{
    port_ = port;
}

RemoteWindowServer::ScreenShotFunction RemoteWindowServer::screenShotFunction() const
{
    return screenShotFunction_;
}

void RemoteWindowServer::setScreenShotFunction(ScreenShotFunction value)
{
    screenShotFunction_ = value;
}

void RemoteWindowServer::incomingConnection(qintptr handle)
{
    RemoteWindowSocket *socket = new RemoteWindowSocket(handle, this);

    QObject::connect(socket, &RemoteWindowSocket::disconnected, this, &RemoteWindowServer::onSocketDisconnected);
    QObject::connect(socket, &RemoteWindowSocket::mouseMoveReceived, this, &RemoteWindowServer::onSocketMouseMoveReceived);
    QObject::connect(socket, &RemoteWindowSocket::mousePressReceived, this, &RemoteWindowServer::onSocketMousePressReceived);
    QObject::connect(socket, &RemoteWindowSocket::mouseReleaseReceived, this, &RemoteWindowServer::onSocketMouseReleaseReceived);
    QObject::connect(socket, &RemoteWindowSocket::mouseClickReceived, this, &RemoteWindowServer::onSocketMouseClickReceived);
    sockets_.append(socket);

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
    if(nullptr == window_)
        return;

    QByteArray data;
    QBuffer buffer(&data);

    if(!buffer.open(QBuffer::WriteOnly))
        return;

    QPixmap pixmap;

    if(nullptr == screenShotFunction_) {
        QScreen *screen = QGuiApplication::primaryScreen();
#ifdef Q_WS_WIN
        QPixmap pixmap = screen->grabWindow(0, window_->x(), window_->y(), window_->width(), window_->height());
#else
        pixmap = screen->grabWindow(window_->winId());
#endif
    } else
        pixmap = screenShotFunction_(window_);

    if(!pixmap.save(&buffer, "jpeg", 30))
        return;

    for(RemoteWindowSocket *socket : sockets_)
        socket->sendWindowCapture(data);
}

void RemoteWindowServer::onSocketDisconnected()
{
    RemoteWindowSocket *socket = static_cast<RemoteWindowSocket *>(QObject::sender());

    socket->deleteLater();
    sockets_.removeAll(socket);

    if(sockets_.isEmpty()) {
        killTimer(windowUpdateTimerId_);
        windowUpdateTimerId_ = -1;
    }
}

void RemoteWindowServer::onSocketMouseMoveReceived(const QPoint &position)
{
    if(nullptr == window_)
        return;

    QTest::mouseMove(window_, position);
}

void RemoteWindowServer::onSocketMousePressReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(nullptr == window_)
        return;

    QTest::mousePress(window_, button, modifiers, position);
}

void RemoteWindowServer::onSocketMouseReleaseReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(nullptr == window_)
        return;

    QTest::mouseRelease(window_, button, modifiers, position);
}

void RemoteWindowServer::onSocketMouseClickReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(nullptr == window_)
        return;

    QTest::mouseClick(window_, button, modifiers, position);
}
