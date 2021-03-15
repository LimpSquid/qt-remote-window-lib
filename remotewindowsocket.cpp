#include "remotewindowsocket.h"
#include <QByteArray>
#include <QDataStream>
#include <QPoint>

const QMap<RemoteWindowSocket::SocketCommand, RemoteWindowSocket::SocketState> RemoteWindowSocket::SOCKET_STATE_MAPPING =
{
    { RemoteWindowSocket::SC_JOIN_SESSION,      RemoteWindowSocket::SS_PROCESS_JOIN_SESSION     },
    { RemoteWindowSocket::SC_JOIN_SESSION_ACK,  RemoteWindowSocket::SS_PROCESS_JOIN_SESSION_ACK },
    { RemoteWindowSocket::SC_LEAVE_SESSION,     RemoteWindowSocket::SS_PROCESS_LEAVE_SESSION    },
    { RemoteWindowSocket::SC_WINDOW_CAPTURE,    RemoteWindowSocket::SS_PROCESS_WINDOW_CAPTURE   },
    { RemoteWindowSocket::SC_MOUSE_MOVE,        RemoteWindowSocket::SS_PROCESS_MOUSE_MOVE       },
    { RemoteWindowSocket::SC_MOUSE_PRESS,       RemoteWindowSocket::SS_PROCESS_MOUSE_PRESS      },
    { RemoteWindowSocket::SC_MOUSE_RELEASE,     RemoteWindowSocket::SS_PROCESS_MOUSE_RELEASE    },
    { RemoteWindowSocket::SC_MOUSE_CLICK,       RemoteWindowSocket::SS_PROCESS_MOUSE_CLICK      },
    { RemoteWindowSocket::SC_KEY_PRESS,         RemoteWindowSocket::SS_PROCESS_KEY_PRESS        },
    { RemoteWindowSocket::SC_KEY_RELEASE,       RemoteWindowSocket::SS_PROCESS_KEY_RELEASE      },
    { RemoteWindowSocket::SC_CHAT_MESSAGE,      RemoteWindowSocket::SS_PROCESS_CHAT_MESSAGE     },
};

const int RemoteWindowSocket::BUFFER_MAX_SIZE = 1024 * 1024 * 20;
const int RemoteWindowSocket::QUEUE_MAX_SIZE = 25;
const int RemoteWindowSocket::CHAT_MSG_MAX_SIZE = 1024;
const char RemoteWindowSocket::MESSAGE_START_MARKER = 0x01; // Start of heading
const char RemoteWindowSocket::MESSAGE_END_MARKER = 0x04; // End of transmission
const char RemoteWindowSocket::MESSAGE_PAYLOAD_SIZE_MARKER = 0x11; // Horizontal tab
const char RemoteWindowSocket::MESSAGE_PAYLOAD_MARKER = 0x09; // Vertical tab

RemoteWindowSocket::RemoteWindowSocket(QObject *parent) :
    QTcpSocket(parent)
{
    socketState_ = SS_READ_MESSAGE;
    sessionState_ = SS_NO_SESSION;

    QObject::connect(this, &QTcpSocket::stateChanged, this, &RemoteWindowSocket::onStateChanged);
    QObject::connect(this, &QTcpSocket::readyRead, this, &RemoteWindowSocket::process);
    QObject::connect(this, &QTcpSocket::connected, [&]() {
        if(SS_NO_SESSION == sessionState_) {
            setSessionState(SS_JOINING);
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

RemoteWindowSocket::SessionState RemoteWindowSocket::sessionState() const
{
    return sessionState_;
}

void RemoteWindowSocket::sendWindowCapture(const QByteArray &compressed)
{
    if(SS_JOINED != sessionState_)
        return;
    if(compressed.isEmpty())
        return;

    sendMessage(SC_WINDOW_CAPTURE, compressed);
}

void RemoteWindowSocket::sendMouseMove(const QPoint &position)
{
    if(SS_JOINED != sessionState_)
        return;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << position;

    sendMessage(SC_MOUSE_MOVE, data);
}

void RemoteWindowSocket::sendMousePress(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(SS_JOINED != sessionState_)
        return;

    sendMouseEvent(SC_MOUSE_PRESS, button, position, modifiers);
}

void RemoteWindowSocket::sendMouseRelease(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(SS_JOINED != sessionState_)
        return;

    sendMouseEvent(SC_MOUSE_RELEASE, button, position, modifiers);
}

void RemoteWindowSocket::sendMouseClick(const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    if(SS_JOINED != sessionState_)
        return;

    sendMouseEvent(SC_MOUSE_CLICK, button, position, modifiers);
}

void RemoteWindowSocket::sendKeyPress(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers)
{
    if(SS_JOINED != sessionState_)
        return;

    sendKeyEvent(SC_KEY_PRESS, key, modifiers);
}

void RemoteWindowSocket::sendKeyRelease(const Qt::Key &key, const Qt::KeyboardModifiers &modifiers)
{
    if(SS_JOINED != sessionState_)
        return;

    sendKeyEvent(SC_KEY_RELEASE, key, modifiers);
}

void RemoteWindowSocket::sendChatMessage(QString msg)
{
    if(SS_JOINED != sessionState_)
        return;
    if(msg.size() > CHAT_MSG_MAX_SIZE) {
        msg.chop(msg.size() - CHAT_MSG_MAX_SIZE);
        msg.append("...");
    }

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << msg;

    sendMessage(SC_CHAT_MESSAGE, data);
}

bool RemoteWindowSocket::sendMessage(const SocketCommand &command, const QByteArray &data)
{
    QByteArray message;

    message.append(MESSAGE_START_MARKER);
    message.append(QString::number(command).toUtf8().toBase64());
    message.append(MESSAGE_PAYLOAD_SIZE_MARKER);
    message.append(QString::number(data.size()).toUtf8().toBase64());
    message.append(MESSAGE_PAYLOAD_MARKER);
    message.append(data);
    message.append(MESSAGE_END_MARKER);
    return write(message) == message.size();
}

void RemoteWindowSocket::readMessage()
{
    // Yeah this kind of sucks, but it works...
    bool exit;

    do {
        exit = true;
        if(buffer_.size() > BUFFER_MAX_SIZE)
            buffer_.clear();
        buffer_.append(readAll());

        int indexOfStart = buffer_.indexOf(MESSAGE_START_MARKER);
        int indexOfPayloadSize = buffer_.indexOf(MESSAGE_PAYLOAD_SIZE_MARKER);
        int indexOfPayload = buffer_.indexOf(MESSAGE_PAYLOAD_MARKER);

        // Check if message header is present
        if(indexOfStart >= 0 && indexOfPayloadSize >= 0 && indexOfPayload >= 0) {
            bool ok = false;
            int payloadSize = QByteArray::fromBase64(buffer_.mid(indexOfPayloadSize + 1, indexOfPayload - indexOfPayloadSize - 1)).toInt(&ok);
            int indexOfEnd = indexOfPayload + payloadSize + 1;

            if(ok && indexOfEnd < buffer_.size()) {
                if(buffer_.at(indexOfEnd) == MESSAGE_END_MARKER) {
                    Message msg;
                    msg.command = static_cast<SocketCommand>(QByteArray::fromBase64(buffer_.mid(indexOfStart + 1, indexOfPayloadSize - indexOfStart - 1)).toInt());
                    msg.payload = buffer_.mid(indexOfPayload + 1, payloadSize);

                    if(messageQueue_.count() > QUEUE_MAX_SIZE)
                        messageQueue_.dequeue();
                    messageQueue_.enqueue(msg);

                    buffer_.remove(0, indexOfEnd + 1); // Remove valid message and possible garbage before message
                    exit = false;
                } else
                    buffer_.clear();
            }
        }
    } while(!exit);
}

void RemoteWindowSocket::sendJoinSession()
{
    sendMessage(SC_JOIN_SESSION);
}

void RemoteWindowSocket::sendJoinSessionAck()
{
    sendMessage(SC_JOIN_SESSION_ACK);
}

void RemoteWindowSocket::sendLeaveSession()
{
    sendMessage(SC_LEAVE_SESSION);
}

void RemoteWindowSocket::sendMouseEvent(const SocketCommand &command, const Qt::MouseButton &button, const QPoint &position, const Qt::KeyboardModifiers &modifiers)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << static_cast<int>(button) << position << static_cast<int>(modifiers);
    sendMessage(command, data);
}

void RemoteWindowSocket::sendKeyEvent(const SocketCommand &command, const Qt::Key &key, const Qt::KeyboardModifiers &modifiers)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << static_cast<int>(key) << static_cast<int>(modifiers);
    sendMessage(command, data);
}

void RemoteWindowSocket::setSessionState(const SessionState &value)
{
    if(sessionState_ != value) {
        sessionState_ = value;
        emit sessionStateChanged();
    }
}

void RemoteWindowSocket::process()
{
    readMessage();

    bool exit = false;
    while(!exit && state() == QAbstractSocket::ConnectedState) {
        switch(socketState_) {
            case SS_READ_MESSAGE:
                if(messageQueue_.isEmpty())
                    exit = true;
                else {
                    message_ = messageQueue_.dequeue();
                    socketState_ = SS_READ_COMMAND;
                }
                break;
            case SS_READ_COMMAND:
                if(SOCKET_STATE_MAPPING.contains(message_.command))
                    socketState_ = SOCKET_STATE_MAPPING.value(message_.command);
                else
                    socketState_ = SS_READ_MESSAGE;
                break;
            case SS_READ_COMMAND_DONE:
                socketState_ = SS_READ_MESSAGE;
                break;

            case SS_PROCESS_JOIN_SESSION:
                if(SS_NO_SESSION == sessionState_) {
                    setSessionState(SS_JOINED);
                    sendJoinSessionAck();
                }
                // @Todo: nack
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_JOIN_SESSION_ACK:
                if(SS_JOINING == sessionState_)
                    setSessionState(SS_JOINED);
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_LEAVE_SESSION:
                setSessionState(SS_NO_SESSION);
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_WINDOW_CAPTURE:
                emit windowCaptureReceived(qUncompress(message_.payload));
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            case SS_PROCESS_MOUSE_MOVE: {
                QPoint position;
                QDataStream stream(&message_.payload, QIODevice::ReadOnly);
                stream >> position;
                emit mouseMoveReceived(position);
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            }
            case SS_PROCESS_MOUSE_PRESS:
            case SS_PROCESS_MOUSE_RELEASE:
            case SS_PROCESS_MOUSE_CLICK: {
                int button;
                QPoint position;
                int modifiers;
                QDataStream stream(&message_.payload, QIODevice::ReadOnly);

                stream >> button >> position >> modifiers;
                if(SS_PROCESS_MOUSE_PRESS == socketState_)
                    emit mousePressReceived(static_cast<Qt::MouseButton>(button), position, static_cast<Qt::KeyboardModifiers>(modifiers));
                else if(SS_PROCESS_MOUSE_RELEASE == socketState_)
                    emit mouseReleaseReceived(static_cast<Qt::MouseButton>(button), position, static_cast<Qt::KeyboardModifiers>(modifiers));
                else if(SS_PROCESS_MOUSE_CLICK == socketState_)
                    emit mouseClickReceived(static_cast<Qt::MouseButton>(button), position, static_cast<Qt::KeyboardModifiers>(modifiers));
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            }
            case SS_PROCESS_KEY_PRESS:
            case SS_PROCESS_KEY_RELEASE: {
                int key;
                int modifiers;
                QDataStream stream(&message_.payload, QIODevice::ReadOnly);

                stream >> key >> modifiers;
                if(SS_PROCESS_KEY_PRESS == socketState_)
                    emit keyPressReceived(static_cast<Qt::Key>(key), static_cast<Qt::KeyboardModifiers>(modifiers));
                else if(SS_PROCESS_KEY_RELEASE == socketState_)
                    emit keyReleaseReceived(static_cast<Qt::Key>(key), static_cast<Qt::KeyboardModifiers>(modifiers));
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            }
            case SS_PROCESS_CHAT_MESSAGE: {
                QString msg;
                QDataStream stream(&message_.payload, QIODevice::ReadOnly);

                stream >> msg;
                emit chatMessageReceived(msg);
                socketState_ = SS_READ_COMMAND_DONE;
                break;
            }
        }
    }
}

void RemoteWindowSocket::onStateChanged(const QAbstractSocket::SocketState &state)
{
    switch(state) {
        default:
            break;
        case ClosingState:
        case UnconnectedState:
            // Session lost...
            buffer_.clear();
            setSessionState(SS_NO_SESSION);
            break;
    }
}
