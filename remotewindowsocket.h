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
    };

    static const QMap<SocketCommand, SocketState> SOCKET_STATE_MAPPING;

    void sendJoinSession();
    void sendJoinSessionAck();
    void sendLeaveSession();

    QCborStreamWriter writer_;
    QCborStreamReader reader_;
    SocketState socketState_;
    SessionState sessionState_;
    QByteArray byteArrayBuffer_;

signals:
    void windowCaptureReceived(const QByteArray &data);

private slots:
    void process();
};
