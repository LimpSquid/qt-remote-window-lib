#include "remotewindowserver.h"
#include "remotewindowsocket.h"
#include <QWindow>
#include <QWindow>
#include <QScreen>
#include <QBuffer>
#include <QTest>

const double RemoteWindowServer::QUALITY_DEFAULT = 0.3; // between 0.0 and 1.0
const int RemoteWindowServer::WINDOW_UPDATE_DELAY_MIN = 5; // In ms
const int RemoteWindowServer::WINDOW_UPDATE_DELAY_DEFAULT = 25; // In ms

RemoteWindowServer::RemoteWindowServer(QObject *parent, unsigned short port) :
    QTcpServer(parent)
{
    window_ = nullptr;
    screenShotFunction_ = nullptr;
    quality_ = QUALITY_DEFAULT;
    windowUpdateDelayTimerId_ = -1;
    windowUpdateDelay_ = WINDOW_UPDATE_DELAY_DEFAULT;
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
    if(window_ != value) {
        window_ = value;
        emit windowChanged();
    }
}

unsigned short RemoteWindowServer::port() const
{
    return port_;
}

void RemoteWindowServer::setPort(unsigned short value)
{
    if(port_ != value) {
        port_ = value;
        emit portChanged();
    }
}

RemoteWindowServer::ScreenShotFunction RemoteWindowServer::screenShotFunction() const
{
    return screenShotFunction_;
}

void RemoteWindowServer::setScreenShotFunction(ScreenShotFunction value)
{
    screenShotFunction_ = value;
}

int RemoteWindowServer::windowUpdateDelay() const
{
    return windowUpdateDelay_;
}

void RemoteWindowServer::setWindowUpdateDelay(int value)
{
    value = qMax(value, WINDOW_UPDATE_DELAY_MIN);

    if(windowUpdateDelay_ != value) {
        windowUpdateDelay_ = value;
        emit windowUpdateDelayChanged();
    }
}

double RemoteWindowServer::quality() const
{
    return quality_;
}

void RemoteWindowServer::setQuality(double value)
{
    value = qBound(0.0, value, 1.0);

    if(quality_ != value) {
        quality_ = value;
        emit qualityChanged();
    }
}

int RemoteWindowServer::clientCount() const
{
    return sockets_.count();
}

void RemoteWindowServer::incomingConnection(qintptr handle)
{
    RemoteWindowSocket *socket = new RemoteWindowSocket(handle, this);

    QObject::connect(socket, &RemoteWindowSocket::disconnected, this, &RemoteWindowServer::onSocketDisconnected);
    QObject::connect(socket, &RemoteWindowSocket::mouseMoveReceived, this, &RemoteWindowServer::onSocketMouseMoveReceived);
    QObject::connect(socket, &RemoteWindowSocket::mousePressReceived, this, &RemoteWindowServer::onSocketMousePressReceived);
    QObject::connect(socket, &RemoteWindowSocket::mouseReleaseReceived, this, &RemoteWindowServer::onSocketMouseReleaseReceived);
    QObject::connect(socket, &RemoteWindowSocket::mouseClickReceived, this, &RemoteWindowServer::onSocketMouseClickReceived);
    QObject::connect(socket, &RemoteWindowSocket::keyPressReceived, this, &RemoteWindowServer::onSocketKeyPressReceived);
    QObject::connect(socket, &RemoteWindowSocket::keyReleaseReceived, this, &RemoteWindowServer::onSocketKeyReleaseReceived);
    appendSocket(socket);

    sendChatMessage(QString("%1: joined the chat").arg(socket->localAddress().toString()));
    if(-1 == windowUpdateDelayTimerId_)
        windowUpdateDelayTimerId_ = startTimer(windowUpdateDelay_);
}

void RemoteWindowServer::timerEvent(QTimerEvent *event)
{
    if(event->timerId() == windowUpdateDelayTimerId_) {
        killTimer(windowUpdateDelayTimerId_);
        handleWindowUpdate();
        windowUpdateDelayTimerId_ = startTimer(windowUpdateDelay_);
    }
}

void RemoteWindowServer::appendSocket(RemoteWindowSocket *socket)
{
    if(nullptr == socket)
        return;

    sockets_.append(socket);
    emit clientCountChanged();
}

void RemoteWindowServer::removeSocket(RemoteWindowSocket *socket)
{
    if(sockets_.contains(socket)) {
        sockets_.removeAll(socket);
        emit clientCountChanged();
    }
}

void RemoteWindowServer::sendChatMessage(QString msg)
{
    for(RemoteWindowSocket *socket : sockets_)
        socket->sendChatMessage(msg);
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
#ifdef Q_OS_WIN
        pixmap = screen->grabWindow(0, window_->x(), window_->y(), window_->width(), window_->height());
#else
        pixmap = screen->grabWindow(window_->winId());
#endif
    } else
        pixmap = screenShotFunction_(window_);

    if(!pixmap.save(&buffer, "jpeg", quality_ * 100))
        return;

    QByteArray compressed = qCompress(data);
    for(RemoteWindowSocket *socket : sockets_)
        socket->sendWindowCapture(compressed);
}

void RemoteWindowServer::onSocketDisconnected()
{
    RemoteWindowSocket *socket = static_cast<RemoteWindowSocket *>(QObject::sender());

    removeSocket(socket);
    sendChatMessage(QString("%1: left the chat").arg(socket->localAddress().toString()));
    socket->deleteLater();

    if(sockets_.isEmpty()) {
        killTimer(windowUpdateDelayTimerId_);
        windowUpdateDelayTimerId_ = -1;
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

void RemoteWindowServer::onSocketKeyPressReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers)
{
    if(nullptr == window_)
        return;

    QTest::keyPress(window_, key, modifiers);
}

void RemoteWindowServer::onSocketKeyReleaseReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers)
{
    if(nullptr == window_)
        return;

    QTest::keyRelease(window_, key, modifiers);
}
