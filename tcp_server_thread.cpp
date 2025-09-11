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
        // 将接收到的所有数据追加到缓冲区
        m_buffer.append(socket->readAll());

        const quint8 HEADER_TOTAL_SIZE = 21;

        // 循环处理缓冲区中的数据，直到数据不足以解析一个完整的包
        while (m_buffer.size() >= HEADER_TOTAL_SIZE) {

            // 1. 解析数据帧头
            const DataHeader* header = reinterpret_cast<const DataHeader*>(m_buffer.constData());

            // 检查固定值 0x9EE9
            if (qFromLittleEndian(header->fixed_value) != 0x9EE9) {
                qDebug() << "无效的数据帧头，可能已失步，清空缓冲区。";
                m_buffer.clear();
                return; // 放弃此包，等待下一个同步点
            }

            quint16 dataLength = qFromLittleEndian(header->data_length);
            quint8 checksum = header->checksum;

            // 检查缓冲区中是否有完整的数据包
            if (m_buffer.size() < HEADER_TOTAL_SIZE + dataLength) {
                // 数据不完整，跳出循环，等待更多数据
                return;
            }

            // 2. 提取数据信息并校验
            QByteArray dataInfoPayload = m_buffer.mid(HEADER_TOTAL_SIZE, dataLength);
            quint8 calculatedChecksum = calculateChecksum(dataInfoPayload);

            if (calculatedChecksum != checksum) {
                qDebug() << "校验和不匹配！期望:" << checksum << "计算:" << calculatedChecksum;
                m_buffer.remove(0, HEADER_TOTAL_SIZE + dataLength); // 丢弃无效数据包
                continue; // 继续处理缓冲区中的下一个数据包
            }

            // 3. 解析数据信息
            const DataInfo* dataInfo = reinterpret_cast<const DataInfo*>(dataInfoPayload.constData());

            // 检查数据信息的固定帧头 0x55AA
            if (qFromLittleEndian(dataInfo->frame_header) != 0x55AA) {
                qDebug() << "无效的数据信息帧头，数据可能损坏。";
                m_buffer.remove(0, HEADER_TOTAL_SIZE + dataLength);
                continue;
            }

            // 4. 发出信号，通知主线程收到了数据，并传递关键参数
            emit receivedImageData(qFromLittleEndian(dataInfo->image_number),
                                   qFromLittleEndian(dataInfo->pixel_offset_x),
                                   qFromLittleEndian(dataInfo->pixel_offset_y));

            // 5. 打印解析出的关键信息 (保留用于日志)
            qDebug() << "收到完整数据包，大小:" << HEADER_TOTAL_SIZE + dataLength;
            qDebug() << "  - 图像编号:" << qFromLittleEndian(dataInfo->image_number);
            qDebug() << "  - 像素偏移 X:" << qFromLittleEndian(dataInfo->pixel_offset_x);
            qDebug() << "  - 像素偏移 Y:" << qFromLittleEndian(dataInfo->pixel_offset_y);
            qDebug() << "  - 指令类型:" << QString::number(dataInfo->command_type, 16);

            // 6. 移除已处理的数据包，继续处理缓冲区中的下一个
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
