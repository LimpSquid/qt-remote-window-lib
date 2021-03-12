#pragma once

#include <QTcpSocket>
#include <QMap>
#include <QQueue>

class RemoteWindowSocket : public QTcpSocket
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteWindowSocket)

public:
    enum SessionState
    {
        SS_NO_SESSION,
        SS_JOINING,
        SS_JOINED,
    };

    RemoteWindowSocket(QObject *parent = nullptr);
    RemoteWindowSocket(qintptr handle, QObject *parent = nullptr);
    virtual ~RemoteWindowSocket() override;

    SessionState sessionState() const;

    void sendWindowCapture(const QByteArray &compressed);
    void sendMouseMove(const QPoint &position);
    void sendMousePress(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());
    void sendMouseRelease(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());
    void sendMouseClick(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifier());
    void sendKeyPress(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifiers());
    void sendKeyRelease(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers = Qt::KeyboardModifiers());

private:
    enum SocketState
    {
        SS_READ_MESSAGE,
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
        SS_PROCESS_KEY_PRESS,
        SS_PROCESS_KEY_RELEASE,
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
        SC_KEY_PRESS,
        SC_KEY_RELEASE,
    };

    struct Message
    {
        SocketCommand command;
        QByteArray payload;
    };

    static const QMap<SocketCommand, SocketState> SOCKET_STATE_MAPPING;
    static const int BUFFER_MAX_SIZE;
    static const int QUEUE_MAX_SIZE;
    static const char MESSAGE_START_MARKER;
    static const char MESSAGE_END_MARKER;
    static const char MESSAGE_PAYLOAD_SIZE_MARKER;
    static const char MESSAGE_PAYLOAD_MARKER;

    bool sendMessage(const SocketCommand &command, const QByteArray &data = QByteArray());
    void readMessage();

    void sendJoinSession();
    void sendJoinSessionAck();
    void sendLeaveSession();
    void sendMouseEvent(const SocketCommand &command, const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void sendKeyEvent(const SocketCommand &command, const Qt::Key &key, const Qt::KeyboardModifiers &modifiers);

    void setSessionState(const SessionState &value);

    QQueue<Message> messageQueue_;
    SocketState socketState_;
    SessionState sessionState_;
    Message message_;
    QByteArray buffer_;

signals:
    void windowCaptureReceived(const QByteArray &data);
    void mouseMoveReceived(const QPoint &position);
    void mousePressReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void mouseReleaseReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void mouseClickReceived(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers);
    void keyPressReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers);
    void keyReleaseReceived(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers);
    void sessionStateChanged();

private slots:
    void process();
    void onStateChanged(const QAbstractSocket::SocketState &state);
};
