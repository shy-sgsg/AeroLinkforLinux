#ifndef TCPSERVERTHREAD_H
#define TCPSERVERTHREAD_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class TcpServerThread : public QObject
{
    Q_OBJECT

public:
    explicit TcpServerThread(QObject *parent = nullptr);
    ~TcpServerThread();

public slots:
    void startServer(const QString& ipAddress, quint16 port);
    void stopServer();

signals:
    void logMessage(const QString& message);
    void serverStarted();
    void serverStopped();
    void receivedImageData(quint16 image_num, qint16 pixel_x, qint16 pixel_y);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpServer* m_tcpServer;
    QByteArray m_buffer; // 新增：用于存储不完整数据包的缓冲区
};

#endif // TCPSERVERTHREAD_H
