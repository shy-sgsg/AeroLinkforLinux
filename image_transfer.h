#ifndef IMAGETRANSFER_H
#define IMAGETRANSFER_H

#pragma once

// Qt基础类型
#include <QObject>
#include <QString>
#include <QQueue>
#include <QMap>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QFile>
#include <QTimer>
#include <QDebug>

// 业务通用类型
#include "package_sar_data.h"

// ===================== 业务通用类型 =====================
// 文件状态（主窗口和传输模块共用）
enum FileStatus {
    Pending,
    Success,
    Failure
};

// ===================== 单文件处理接口 =====================
struct ImageTransferResult {
    bool success;
    QString message;
};

// 单文件处理（TIF转JPG、AUX打包、TCP发送）
ImageTransferResult processAndTransferImage(const QString &filePath, const QString &ipAddress, quint16 port, uint16_t image_num);

ImageTransferResult processAndTransferManualImage(const QString &tifFilePath, const QString &auxFilePath, const QString &ipAddress, quint16 port, uint16_t image_num);


// ===================== 高级批量传输类 =====================
// 支持信号/槽的批量传输工具
class SarPacketTransferManager : public QObject {
    Q_OBJECT

public:
    explicit SarPacketTransferManager(QObject* parent = nullptr);
    void startTransfer(const QString& binFilePath, const QString& ipAddress, quint16 port);

signals:
    void finished(bool success);
    void disconnected(); // 新增信号：当套接字断开连接时发出

private slots:
    void onConnected();
    void onBytesWritten(qint64 bytes);
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onReadyRead(); // 新增槽函数：处理接收端发来的数据
    void onTimeout();   // 新增槽函数：处理超时

private:
    void sendNextPacket();

private:
    QTcpSocket* m_socket;
    SarPacketizer* m_packetizer = nullptr;
    QTimer* m_timeoutTimer; // 超时定时器
    bool m_transferSuccessful = false;
};

#endif // IMAGETRANSFER_H
