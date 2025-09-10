#ifndef MESSAGE_TRANSFER_H
#define MESSAGE_TRANSFER_H

#include <QObject>
#include <QTcpSocket>

class MessageTransfer : public QObject
{
    Q_OBJECT
public:
    explicit MessageTransfer(QObject* parent = nullptr);
    ~MessageTransfer();

    void sendMessage(const QString& message, const QString& ipAddress, quint16 port);
    QTcpSocket* socket() const;

signals:
    void logMessage(const QString& message);
    void connected();
    void disconnected();
    void readyRead();
    void errorOccurred(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* m_socket;
};

#endif // MESSAGE_TRANSFER_H
