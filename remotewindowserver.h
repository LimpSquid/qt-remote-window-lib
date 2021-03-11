#pragma once

#include <QTcpServer>
#include <QList>
#include <QTimer>
#include <functional>

class QWindow;
class QPixmap;
class RemoteWindowSocket;
class RemoteWindowServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteWindowServer)

public:
    using ScreenShotFunction = std::function<QPixmap(QWindow *)>;
    RemoteWindowServer(QObject *parent = nullptr, unsigned short port = 55555);
    RemoteWindowServer(QWindow *window, QObject *parent = nullptr, unsigned short port = 55555);
    virtual ~RemoteWindowServer() override;

    bool start();
    void stop();

    QWindow *window() const;
    void setWindow(QWindow *value);

    unsigned short port() const;
    void setPort(unsigned short value);

    ScreenShotFunction screenShotFunction() const;
    void setScreenShotFunction(ScreenShotFunction value);

    int windowUpdateDelay() const;
    void setWindowUpdateDelay(int value);

    double quality() const;
    void setQuality(double value);

    int clientCount() const;

private:
    static const double QUALITY_DEFAULT;
    static const int WINDOW_UPDATE_DELAY_MIN;
    static const int WINDOW_UPDATE_DELAY_DEFAULT;

    virtual void incomingConnection(qintptr handle) override;
    virtual void timerEvent(QTimerEvent *event) override;

    void appendSocket(RemoteWindowSocket *socket);
    void removeSocket(RemoteWindowSocket *socket);
    void handleWindowUpdate();

    QWindow *window_;
    QList<RemoteWindowSocket *> sockets_;
    ScreenShotFunction screenShotFunction_;
    double quality_;
    int windowUpdateDelayTimerId_;
    int windowUpdateDelay_;
    unsigned short port_;

signals:
    void windowChanged();
    void portChanged();
    void windowUpdateDelayChanged();
    void qualityChanged();
    void clientCountChanged();

private slots:
    void onSocketDisconnected();
    void onSocketMouseMoveReceived(const QPoint &position);
    void onSocketMousePressReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void onSocketMouseReleaseReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void onSocketMouseClickReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void onSocketKeyPressReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers);
    void onSocketKeyReleaseReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers);
};
