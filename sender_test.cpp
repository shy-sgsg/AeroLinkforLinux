// sender_test.cpp
#include <QCoreApplication>
#include <QTcpSocket>
#include <QDebug>
#include <QHostAddress>
#include <QtEndian>
#include <QTimer>
#include "radar_protocol.h" // 包含您新定义的协议头文件

// 此类用于打包和发送数据
class SenderClient : public QObject
{
    Q_OBJECT

public:
    explicit SenderClient(QObject* parent = nullptr) : QObject(parent), m_socket(new QTcpSocket(this)) {
        connect(m_socket, &QTcpSocket::connected, this, &SenderClient::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &SenderClient::onDisconnected);
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &SenderClient::onError);
    }

    void start(const QString& ip, quint16 port) {
        qDebug() << "尝试连接到服务器:" << ip << ":" << port;
        m_socket->connectToHost(ip, port);
    }

private slots:
    void onConnected() {
        qDebug() << "已成功连接到服务器。";

        // 示例：创建一个用于发送的 DataInfo 结构体
        DataInfo dataInfo = {};
        dataInfo.frame_header = qToLittleEndian(static_cast<quint16>(0x55AA));
        dataInfo.data_length = qToLittleEndian(static_cast<quint32>(18)); // DataInfo 除去帧头和自身长度字段，共18字节
        dataInfo.message_count = qToLittleEndian(static_cast<quint16>(1));
        dataInfo.source_address = qToLittleEndian(static_cast<quint16>(100));
        dataInfo.destination_address = qToLittleEndian(static_cast<quint16>(200));
        dataInfo.command_type = 0x01;
        dataInfo.reserved = 0;
        dataInfo.image_number = qToLittleEndian(static_cast<quint16>(12345));
        dataInfo.pixel_offset_x = qToLittleEndian(static_cast<qint16>(50));
        dataInfo.pixel_offset_y = qToLittleEndian(static_cast<qint16>(100));

        QByteArray dataInfoPayload(reinterpret_cast<const char*>(&dataInfo), sizeof(DataInfo));

        // 计算校验和
        quint8 checksum = calculateChecksum(dataInfoPayload);

        // 示例：创建一个用于发送的 DataHeader 结构体
        DataHeader dataHeader = {};
        dataHeader.fixed_value = qToLittleEndian(static_cast<quint16>(0x9EE9));
        dataHeader.data_length = qToLittleEndian(static_cast<quint16>(dataInfoPayload.size()));
        dataHeader.checksum = checksum;

        QByteArray fullPacket;
        fullPacket.append(reinterpret_cast<const char*>(&dataHeader), sizeof(DataHeader));
        fullPacket.append(dataInfoPayload);

        qDebug() << "准备发送数据包，总大小:" << fullPacket.size();
        qDebug() << "  - 校验和:" << QString::number(checksum, 16);
        qDebug() << "  - 图像编号:" << qFromLittleEndian(dataInfo.image_number);

        m_socket->write(fullPacket);
        m_socket->flush(); // 立即发送数据

        qDebug() << "数据已发送，断开连接。";
        m_socket->disconnectFromHost();
    }

    void onDisconnected() {
        qDebug() << "已与服务器断开连接。";
        QCoreApplication::quit();
    }

    void onError(QAbstractSocket::SocketError socketError) {
        qDebug() << "发生错误:" << m_socket->errorString();
        QCoreApplication::quit();
    }

private:
    QTcpSocket* m_socket;
};


