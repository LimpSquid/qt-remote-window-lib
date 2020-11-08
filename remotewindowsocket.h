#pragma once

#include <QTcpSocket>
#include <QCborStreamWriter>
#include <QCborStreamReader>
#include <QMap>

class RemoteWindowSocket : public QTcpSocket
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteWindowSocket)

public:
    RemoteWindowSocket(QObject *parent = nullptr);
    RemoteWindowSocket(qintptr handle, QObject *parent = nullptr);
    virtual ~RemoteWindowSocket() override;

    void sendWindowCapture(const QByteArray &data);
    void sendMouseMove(const QPoint &position);
    void sendMousePress(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());
    void sendMouseRelease(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());
    void sendMouseClick(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());

private:
    enum SessionState
    {
        SS_NO_SESSION,
        SS_JOINING,
        SS_JOINED,
        SS_LEAVING,
    };

    enum SocketState
    {
        SS_READ_JOIN = 0,
        SS_READ_STREAM,
        SS_READ_COMMAND,
        SS_READ_COMMAND_DONE,

        SS_PROCESS_JOIN_SESSION,
        SS_PROCESS_JOIN_SESSION_ACK,
        SS_PROCESS_LEAVE_SESSION,
        SS_PROCESS_WINDOW_CAPTURE,
        SS_PROCESS_MOUSE_MOVE,
        SS_PROCESS_MOUSE_PRESS,
        SS_PROCESS_MOUSE_RELEASE,
        SS_PROCESS_MOUSE_CLICK,

        SS_ERROR,
        SS_DONE,
    };

    enum SocketCommand
    {
        SC_UNKNOWN = 0,
        SC_JOIN_SESSION,
        SC_JOIN_SESSION_ACK,
        SC_LEAVE_SESSION,
        SC_WINDOW_CAPTURE,
        SC_MOUSE_MOVE,
        SC_MOUSE_PRESS,
        SC_MOUSE_RELEASE,
        SC_MOUSE_CLICK,
    };

    static const QMap<SocketCommand, SocketState> SOCKET_STATE_MAPPING;

    void sendJoinSession();
    void sendJoinSessionAck();
    void sendLeaveSession();
    void sendMouseEvent(const SocketCommand &command, const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);

    QCborStreamWriter writer_;
    QCborStreamReader reader_;
    SocketState socketState_;
    SessionState sessionState_;
    QByteArray byteArrayBuffer_;

signals:
    void windowCaptureReceived(const QByteArray &data);
    void mouseMoveReceived(const QPoint &position);
    void mousePressReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void mouseReleaseReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void mouseClickReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);

private slots:
    void process();
};
