#include "remotewindowsocket.h"
#include <QByteArray>

const QMap<RemoteWindowSocket::SocketCommand, RemoteWindowSocket::SocketState> RemoteWindowSocket::SOCKET_STATE_MAPPING =
{
    { RemoteWindowSocket::SC_JOIN_SESSION,      RemoteWindowSocket::SS_PROCESS_JOIN_SESSION     },
    { RemoteWindowSocket::SC_JOIN_SESSION_ACK,  RemoteWindowSocket::SS_PROCESS_JOIN_SESSION_ACK },
    { RemoteWindowSocket::SC_LEAVE_SESSION,     RemoteWindowSocket::SS_PROCESS_LEAVE_SESSION    },
    { RemoteWindowSocket::SC_WINDOW_CAPTURE,    RemoteWindowSocket::SS_PROCESS_WINDOW_CAPTURE   },
};

RemoteWindowSocket::RemoteWindowSocket(QObject *parent) :
    QTcpSocket(parent),
    writer_(this),
    reader_(this)
{
    socketState_ = SS_READ_JOIN;
    sessionState_ = SS_NO_SESSION;

    QObject::connect(this, &QTcpSocket::readyRead, this, &RemoteWindowSocket::process);
    QObject::connect(this, &QTcpSocket::connected, [&] {
        if(SS_NO_SESSION == sessionState_) {
            sessionState_ = SS_JOINING;
            sendJoinSession();
        }
    });
}

RemoteWindowSocket::RemoteWindowSocket(qintptr handle, QObject *parent) :
    RemoteWindowSocket(parent)
{
    setSocketDescriptor(handle);
}

RemoteWindowSocket::~RemoteWindowSocket()
{
    if(SS_JOINED == sessionState_) {
        sendLeaveSession();
        waitForBytesWritten();
    }
}

void RemoteWindowSocket::sendWindowCapture(const QByteArray &data)
{
    if(data.isEmpty())
        return;
    if(sessionState_ != SS_JOINED)
        return;

    writer_.startMap(1);
    writer_.append(static_cast<int>(SC_WINDOW_CAPTURE));
    writer_.append(data);
    writer_.endMap();
}

void RemoteWindowSocket::sendJoinSession()
{
    writer_.startArray();

    writer_.startMap(1);
    writer_.append(static_cast<int>(SC_JOIN_SESSION));
    writer_.append(nullptr); // No payload
    writer_.endMap();
}

void RemoteWindowSocket::sendJoinSessionAck()
{
    writer_.startArray();

    writer_.startMap(1);
    writer_.append(static_cast<int>(SC_JOIN_SESSION_ACK));
    writer_.append(nullptr); // No payload
    writer_.endMap();
}

void RemoteWindowSocket::sendLeaveSession()
{
    writer_.startMap(1);
    writer_.append(static_cast<int>(SC_LEAVE_SESSION));
    writer_.append(nullptr); // No payload
    writer_.endMap();

    writer_.endArray();
}

void RemoteWindowSocket::process()
{
    reader_.reparse();

    bool exit = false;
    while(reader_.lastError() == QCborError::NoError && !exit) {
        switch(socketState_) {
            case SS_READ_JOIN:
                if(reader_.isArray()) {
                    reader_.enterContainer(); // Start of our session
                    socketState_ = SS_READ_STREAM;
                } else
                    socketState_ = SS_ERROR;
                break;
            case SS_READ_STREAM:
                if(reader_.hasNext() && reader_.isMap()) {
                    // Clear any buffers here
                    byteArrayBuffer_.clear();

                    reader_.enterContainer();
                    socketState_ = SS_READ_COMMAND;
                } else
                    exit = true;
                break;
            case SS_READ_COMMAND:
                if(reader_.isInteger()) {
                    SocketCommand command = static_cast<SocketCommand>(reader_.toInteger());
                    if(SOCKET_STATE_MAPPING.contains(command)) {
                        reader_.next();
                        socketState_ = SOCKET_STATE_MAPPING.value(command);
                    }
                    else
                        socketState_ = SS_ERROR;
                } else
                    socketState_ = SS_ERROR;
                break;
            case SS_READ_COMMAND_DONE:
                reader_.leaveContainer();
                socketState_ = SS_READ_STREAM;
                break;

            case SS_PROCESS_JOIN_SESSION:
                reader_.next();
                if(SS_NO_SESSION == sessionState_) {
                    sessionState_ = SS_JOINED;
                    sendJoinSessionAck();
                }
                // @Todo: nack
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_JOIN_SESSION_ACK:
                reader_.next();
                if(SS_JOINING == sessionState_)
                    sessionState_ = SS_JOINED;
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_LEAVE_SESSION:
                socketState_ = SS_DONE;
                break;
            case SS_PROCESS_WINDOW_CAPTURE:
                if(reader_.isByteArray()) {
                    const auto &result = reader_.readByteArray();
                    byteArrayBuffer_.append(result.data);

                    if(QCborStreamReader::EndOfString == result.status) {
                        emit windowCaptureReceived(byteArrayBuffer_);
                        socketState_ = SS_READ_COMMAND_DONE;
                    }
                } else
                    socketState_ = SS_ERROR;
                break;

            case SS_ERROR:
            case SS_DONE:
                // Leave all containters and clear stream
                while(reader_.containerDepth() > 0)
                    reader_.leaveContainer();
                reader_.clear();
                socketState_ = SS_READ_JOIN;
                exit = true;
                break;
        }
    }
}
