#include "message_transfer.h"
#include <QDebug>

MessageTransfer::MessageTransfer(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MessageTransfer::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MessageTransfer::disconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MessageTransfer::readyRead);
//    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
//            this, &MessageTransfer::errorOccurred);
}

MessageTransfer::~MessageTransfer() {}

void MessageTransfer::sendMessage(const QString& message, const QString& ipAddress, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(ipAddress, port);
        if (!m_socket->waitForConnected(3000)) {
            emit logMessage("Unable to connect to server: " + m_socket->errorString());
            return;
        }
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(message.toUtf8());
        m_socket->flush();
        emit logMessage("Message sent: " + message);
    } else {
        emit logMessage("Failed to send message: not connected.");
    }
}

QTcpSocket* MessageTransfer::socket() const {
    return m_socket;
}
