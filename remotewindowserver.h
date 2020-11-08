#pragma once

#include <QTcpServer>
#include <QList>
#include <QTimer>

class QWindow;
class QPixmap;
class RemoteWindowSocket;
class RemoteWindowServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteWindowServer)

public:
    RemoteWindowServer(QWindow *window, unsigned short port = 55555);
    virtual ~RemoteWindowServer() override;

    QWindow *window() const;
    void setWindow(QWindow *value);

private:
    static const int WINDOW_UPDATE_TIME_INTERVAL;

    virtual void incomingConnection(qintptr handle) override;
    virtual void timerEvent(QTimerEvent *event) override;

    void handleWindowUpdate();

    QWindow *window_;

    QMap<qintptr, RemoteWindowSocket *> sockets_;

    int windowUpdateTimerId_;

private slots:
    void onSocketDisconnected();
    void onSocketMouseMoveReceived(const QPoint &position);
    void onSocketMouseClickReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifier &modifiers);
};

