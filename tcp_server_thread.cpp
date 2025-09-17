#include "tcp_server_thread.h"
#include "radar_protocol.h"
#include <QDebug>
#include <QHostAddress>
#include <QtEndian>

TcpServerThread::TcpServerThread(QObject *parent)
    : QObject(parent), m_tcpServer(new QTcpServer(this))
{
    // 将服务器的新连接信号连接到槽
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServerThread::onNewConnection);
}

TcpServerThread::~TcpServerThread()
{
    // 析构时关闭服务器
    m_tcpServer->close();
}

void TcpServerThread::startServer(const QString& ipAddress, quint16 port)
{
    if (!m_tcpServer->listen(QHostAddress(ipAddress), port)) {
        qDebug() << tr("服务器无法启动！错误：%1。").arg(m_tcpServer->errorString());
    } else {
        qDebug() << tr("TCP服务器已成功启动。正在监听 %1:%2。").arg(ipAddress).arg(port);
        // 成功启动后发出信号，通知主线程更新UI
        emit serverStarted();
    }
}

void TcpServerThread::stopServer()
{
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
        qDebug() << "TCP服务器已停止。";
        // 成功停止后发出信号，通知主线程更新UI
        emit serverStopped();
    }
}

void TcpServerThread::onNewConnection()
{
    // 获取新的连接
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    qDebug() << tr("来自 %1:%2 的新连接。").arg(socket->peerAddress().toString()).arg(socket->peerPort());

    // 连接新套接字的信号
    connect(socket, &QTcpSocket::readyRead, this, &TcpServerThread::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &TcpServerThread::onSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &TcpServerThread::onSocketError);
}

void TcpServerThread::onSocketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        m_buffer.append(socket->readAll());
        const quint8 HEADER_TOTAL_SIZE = 21;

        while (m_buffer.size() >= HEADER_TOTAL_SIZE) {
            const DataHeader* header = reinterpret_cast<const DataHeader*>(m_buffer.constData());
            if (qFromLittleEndian(header->fixed_value) != 0x9EE9) {
                qDebug() << "无效的数据帧头，可能已失步，清空缓冲区。";
                m_buffer.clear();
                return;
            }

            quint16 dataLength = qFromLittleEndian(header->data_length);
            quint8 checksum = header->checksum;

            if (m_buffer.size() < HEADER_TOTAL_SIZE + dataLength) {
                return;
            }

            QByteArray dataInfoPayload = m_buffer.mid(HEADER_TOTAL_SIZE, dataLength);
            quint8 calculatedChecksum = calculateChecksum(dataInfoPayload);

            if (calculatedChecksum != checksum) {
                qDebug() << "校验和不匹配！期望:" << checksum << "计算:" << calculatedChecksum;
                m_buffer.remove(0, HEADER_TOTAL_SIZE + dataLength);
                continue;
            }

            const DataInfo* dataInfo = reinterpret_cast<const DataInfo*>(dataInfoPayload.constData());
            if (qFromLittleEndian(dataInfo->frame_header) != 0x55AA) {
                qDebug() << "无效的数据信息帧头，数据可能损坏。";
                m_buffer.remove(0, HEADER_TOTAL_SIZE + dataLength);
                continue;
            }

            quint16 imageNumber = qFromLittleEndian(dataInfo->image_number);
            quint16 offsetX = qFromLittleEndian(dataInfo->pixel_offset_x);
            quint16 offsetY = qFromLittleEndian(dataInfo->pixel_offset_y);
            quint8 commandType = dataInfo->command_type;

            // 根据指令类型决定下一步操作
            if (commandType == 0x01) { // 假设0x01是ISAR成像请求的指令类型
                // 发出信号，通知主线程处理ISAR请求
                emit receivedIsarRequest(imageNumber, offsetX, offsetY);
                qDebug() << "收到ISAR成像请求，正在生成XML文件...";
            } else {
                // 处理其他类型的命令，例如您已有的receivedImageData
                // emit receivedImageData(imageNumber, offsetX, offsetY);
            }

            qDebug() << "收到完整数据包，大小:" << HEADER_TOTAL_SIZE + dataLength;
            qDebug() << "  - 图像编号:" << imageNumber;
                       qDebug() << "  - 像素偏移 X:" << offsetX;
                       qDebug() << "  - 像素偏移 Y:" << offsetY;
                       qDebug() << "  - 指令类型:" << QString::number(commandType, 16);
                        m_buffer.remove(0, HEADER_TOTAL_SIZE + dataLength);
        }
    }
}
void TcpServerThread::onSocketDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        qDebug() << tr("客户端已断开连接，来自 %1:%2。").arg(socket->peerAddress().toString()).arg(socket->peerPort());
        socket->deleteLater();
    }
}

void TcpServerThread::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError) // 避免编译警告
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        // qDebug() << tr("套接字错误: %1").arg(socket->errorString());
    }
}
