#include <QTimer>
#include <QFileDialog>
#include <QElapsedTimer>
#include "image_utils.h"
#include "image_transfer.h"
#include "package_sar_data.h"
#include <QFileInfo>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QBuffer>
#include <QCoreApplication>
#include <QThread>

/**
 * @brief Processes and transfers GMTI data.
 *
 * This function takes a GMTI-related .bin file and orchestrates its processing and transfer.
 * It finds corresponding .txt (for coordinates) and .png (for image) files,
 * packages them into a new structured .bin file, and then transfers it over TCP.
 *
 * @param filePath Path to the input .bin file.
 * @param ipAddress The destination IP address for the transfer.
 * @param port The destination port.
 * @param image_num A sequential number for the image packet.
 * @return An ImageTransferResult indicating the success or failure of the operation.
 */
ImageTransferResult processAndTransferGMTI(const QString &filePath, const QString &ipAddress, quint16 port, uint16_t image_num)
{
    ImageTransferResult result;
    result.success = false;

    // 1.  derive file paths and verify existence
    QFileInfo imageFileInfo(filePath);
    if (imageFileInfo.suffix().toLower() != "png" && imageFileInfo.suffix().toLower() != "jpg" && imageFileInfo.suffix().toLower() != "tif") {
        result.message = QString("Input file %1 is not a image file, skip.").arg(filePath);
        qWarning() << result.message;
        return result;
    }

    QString baseName = imageFileInfo.baseName();
    QString dirPath = imageFileInfo.path();
    QString txtPath = dirPath + QDir::separator() + baseName + ".txt";
    QString binPath = dirPath + QDir::separator() + baseName + ".bin";

    // 新增：为 .txt 和 .png 文件添加等待和重试机制
    const int MAX_RETRIES = 10;
    const int RETRY_DELAY_MS = 500;
    int retries = 0;

    // 等待 .txt 文件
    while (!QFileInfo::exists(txtPath) && retries < MAX_RETRIES) {
        // qDebug() << QString("Required .txt file not found: %1. Retrying... (%2/%3)").arg(txtPath).arg(retries + 1).arg(MAX_RETRIES);
        QThread::msleep(RETRY_DELAY_MS);
        retries++;
    }
    if (!QFileInfo::exists(txtPath)) {
        result.message = QString("Required .txt file not found after %1 retries: %2").arg(MAX_RETRIES).arg(txtPath);
        qWarning() << result.message;
        return result;
    }

    // 重置重试计数器并等待 .bin 文件
    retries = 0;
    while (!QFileInfo::exists(binPath) && retries < MAX_RETRIES) {
        // qDebug() << QString("Required .bin file not found: %1. Retrying... (%2/%3)").arg(binPath).arg(retries + 1).arg(MAX_RETRIES);
        QThread::msleep(RETRY_DELAY_MS);
        retries++;
    }
    if (!QFileInfo::exists(binPath)) {
        result.message = QString("Required .bin file not found after %1 retries: %2").arg(MAX_RETRIES).arg(binPath);
        qWarning() << result.message;
        return result;
    }

    // 2. Parse the TXT file for coordinates
    QMap<QString, double> coords;
    QFile txtFile(txtPath);
    if (!txtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = QString("Failed to open .txt file: %1").arg(txtPath);
        qWarning() << result.message;
        return result;
    }

    QTextStream in(&txtFile);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split('=');
        if (parts.size() == 2) {
            bool ok;
            QString key = parts[0].trimmed();
            double value = parts[1].trimmed().toDouble(&ok);
            if (ok) {
                coords[key] = value;
            } else {
                qWarning() << "Could not parse value for key" << key << "in file" << txtPath;
            }
        }
    }
    txtFile.close();

    // Verify all needed coordinates were parsed
    QStringList requiredKeys = {"B0", "L0", "B1", "L1", "B2", "L2", "B3", "L3"};
    for (const QString& key : requiredKeys) {
        if (!coords.contains(key)) {
            result.message = QString("Missing required key '%1' in file %2").arg(key).arg(txtPath);
            qWarning() << result.message;
            return result;
        }
    }

    // 3. Read BIN and PNG data into buffers
    QFile originalBinFile(binPath);
    if (!originalBinFile.open(QIODevice::ReadOnly)) {
        result.message = "Failed to open original BIN file: " + filePath;
        qWarning() << result.message;
        return result;
    }
    QByteArray originalBinData = originalBinFile.readAll();
    originalBinFile.close();

    QFile pngFile(filePath);
    if (!pngFile.open(QIODevice::ReadOnly)) {
        result.message = "Failed to open PNG file: " + binPath;
        qWarning() << result.message;
        return result;
    }
    QByteArray pngData = pngFile.readAll();
    pngFile.close();

    // 4. Populate SAR_DataInfo structure
    SAR_DataInfo dataInfo;
    memset(&dataInfo, 0, sizeof(SAR_DataInfo));

    const double QUANTIZER = 8.38191e-8;
    dataInfo.frame_header = 0x55AA;
    dataInfo.message_type = 0x0003; // Using 0x0003 to signify GMTI data type
    dataInfo.message_count = image_num;
    dataInfo.image_available_flag = 0xFFFF;

    // B/L mapping: 0=TL, 1=BR, 2=TR, 3=BL
    dataInfo.top_left_lng      = static_cast<int32_t>(round(coords.value("L0") / QUANTIZER));
    dataInfo.top_left_lat      = static_cast<int32_t>(round(coords.value("B0") / QUANTIZER));
    dataInfo.bottom_right_lng  = static_cast<int32_t>(round(coords.value("L1") / QUANTIZER));
    dataInfo.bottom_right_lat  = static_cast<int32_t>(round(coords.value("B1") / QUANTIZER));
    dataInfo.top_right_lng     = static_cast<int32_t>(round(coords.value("L2") / QUANTIZER));
    dataInfo.top_right_lat     = static_cast<int32_t>(round(coords.value("B2") / QUANTIZER));
    dataInfo.bottom_left_lng   = static_cast<int32_t>(round(coords.value("L3") / QUANTIZER));
    dataInfo.bottom_left_lat   = static_cast<int32_t>(round(coords.value("B3") / QUANTIZER));

    // Calculate total length and checksum
    dataInfo.data_length = sizeof(SAR_DataInfo) + originalBinData.size() + pngData.size();
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&dataInfo);
    dataInfo.checksum = calculate_checksum(ptr + sizeof(uint16_t), sizeof(SAR_DataInfo) - sizeof(uint16_t) - sizeof(uint8_t));

    // 5. Assemble the full message payload
    QByteArray fullMessage;
    fullMessage.append(reinterpret_cast<const char*>(&dataInfo), sizeof(SAR_DataInfo));
    fullMessage.append(originalBinData);
    fullMessage.append(pngData);

    // 6. Create the new transmittable BIN file with SAR_Frame headers
    QString outputBinFilePath = dirPath + QDir::separator() + baseName + "_gmti_packaged.bin";
    QFile transmitBinFile(outputBinFilePath);
    if (!transmitBinFile.open(QIODevice::WriteOnly)) {
        result.message = "Failed to create temporary bin file for transmission: " + outputBinFilePath;
        qWarning() << result.message;
        return result;
    }

    const qint64 payloadChunkSize = 4096;
    qint64 totalPackets = (fullMessage.size() + payloadChunkSize - 1) / payloadChunkSize;
    qint64 currentOffset = 0;

    for (int i = 0; i < totalPackets; ++i) {
        SAR_Frame header;
        memset(&header, 0, sizeof(SAR_Frame));

        qint64 payloadSize = qMin(payloadChunkSize, fullMessage.size() - currentOffset);
        QByteArray payload = fullMessage.mid(currentOffset, payloadSize);

        header.fixed_value = 0x90E9;
        header.image_number = image_num;
        header.image_size = pngData.size(); // The image part is the PNG data
        header.current_packet = i + 1;
        header.total_packets = totalPackets;
        header.data_length = payloadSize;
        header.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(payload.constData()), payload.size());

        transmitBinFile.write(reinterpret_cast<const char*>(&header), sizeof(SAR_Frame));
        transmitBinFile.write(payload);

        currentOffset += payloadSize;
    }
    transmitBinFile.close();
    qDebug() << "Successfully created GMTI package file:" << outputBinFilePath;

    // 7. Start the online transfer
    SarPacketTransferManager transferManager;
    QEventLoop loop;
    bool transferSuccess = false;
    QString transferMessage;

    QObject::connect(&transferManager, &SarPacketTransferManager::finished,
                     [&](bool success) {
                         transferSuccess = success;
                         transferMessage = success ? "GMTI transfer completed successfully." : "GMTI transfer failed.";
                         loop.quit();
                     });

    qDebug() << "Starting GMTI online transfer...";
    transferManager.startTransfer(outputBinFilePath, ipAddress, port);
    loop.exec();

    result.success = transferSuccess;
    result.message = transferMessage;

    // Optional: clean up the temporary file
    if (QFile::exists(outputBinFilePath)) {
        QFile::remove(outputBinFilePath);
    }

    return result;
}

ImageTransferResult processAndTransferImage(const QString &filePath, const QString &ipAddress, quint16 port, uint16_t image_num)
{
    ImageTransferResult result;
    result.success = false;

    if (QFileInfo(filePath).suffix().toLower() != "tif") {
        result.message = QString("File %1 is not TIF, skip.").arg(filePath);
        qDebug() << result.message;
        return result;
    }

    if (!waitForFileRelease(filePath)) {
        result.message = QString("File %1 is locked for too long, give up processing.").arg(filePath);
        qDebug() << result.message;
        return result;
    }

    // 1. 离线打包阶段：按规则生成AUX文件路径（替换IMG为AUX，后缀为.dat）
    QFileInfo tifFileInfo(filePath);
    // 解析TIF文件的关键信息：路径、不含后缀的文件名（baseName）
    QString tifDir = tifFileInfo.path();          // TIF文件所在目录（AUX文件默认在同一目录）
    QString tifBaseName = tifFileInfo.baseName(); // TIF文件名（不含路径和后缀，如"IMG_20240908_1234"）
    QString tifSuffix = tifFileInfo.suffix();     // TIF后缀（用于验证是否为tif文件）

    // 第一步：验证TIF文件合法性（后缀+文件名长度）
    // 1.1 验证是否为TIF文件（避免非TIF文件进入逻辑）
    if (tifSuffix.compare("tif", Qt::CaseInsensitive) != 0) {
        result.message = QString("File %1 is not TIF (suffix: %2), skip.").arg(filePath).arg(tifSuffix);
        qDebug() << result.message;
        return result;
    }
    // 1.2 验证TIF文件名长度≥3（确保有前3个字符可替换）
    if (tifBaseName.length() < 3) {
        result.message = QString("TIF file name %1 is too short (length < 3), cannot generate AUX name.").arg(tifBaseName);
        qWarning() << result.message;
        return result;
    }
    // 1.3 验证TIF文件名前3个字符是否为"IMG"（按规则要求）
    if (tifBaseName.left(3).compare("IMG", Qt::CaseInsensitive) != 0) {
        result.message = QString("TIF file name %1 does not start with 'IMG', cannot generate AUX name (required: IMGxxx.tif → AUXxxx.dat).").arg(tifBaseName);
        qWarning() << result.message;
        return result;
    }

    // 第二步：按规则生成AUX文件路径
    // 规则：前3个字符"IMG"→"AUX"，后缀".tif"→".dat"
    QString auxBaseName = "AUX" + tifBaseName.mid(3); // 重构AUX文件名（如"IMG_2024"→"AUX_2024"）
    QString auxPath = QString("%1%2%3.dat").arg(
        tifDir,                  // AUX与TIF同目录
        QDir::separator(),       // 系统兼容的路径分隔符（Windows\，Linux/）
        auxBaseName              // 按规则生成的AUX文件名
    );

    // 第三步：原有AUX文件等待重试逻辑（不变，复用原逻辑）
    const int MAX_AUX_RETRIES = 10;
    const int AUX_RETRY_DELAY_MS = 500;
    int auxRetries = 0;
    while (!QFileInfo::exists(auxPath) && auxRetries < MAX_AUX_RETRIES) {
        // qDebug() << QString("AUX file %1 not found. Retrying... (%2/%3)").arg(auxPath).arg(auxRetries + 1).arg(MAX_AUX_RETRIES);
        QThread::msleep(AUX_RETRY_DELAY_MS);
        auxRetries++;
    }

    // 第四步：AUX文件存在性最终校验
    if (!QFileInfo::exists(auxPath)) {
        result.message = QString("Failed to find AUX file %1 after %2 retries. Give up.").arg(auxPath).arg(MAX_AUX_RETRIES);
        qWarning() << result.message;
        return result;
    }

    qDebug() << "AUX file found (generated by rule: IMG→AUX, .tif→.dat):" << auxPath;

    // 后续BIN文件生成逻辑（不变，复用原逻辑）
    QString binPath = filePath;
    binPath.replace(".tif", ".bin", Qt::CaseInsensitive);

    qDebug() << "Starting offline packing...";
    if (!createBinFileFromTifAndAux(filePath, auxPath, binPath, image_num)) {
        result.message = "Failed to create bin file.";
        return result;
    }
    qDebug() << "Offline packing completed successfully.";

    // 2. 在线传输阶段
    SarPacketTransferManager transferManager;
    QEventLoop loop;

    // 关键修正：使用一个局部变量来捕获传输结果
    bool transferSuccess = false;
    QString transferMessage;

    QObject::connect(&transferManager, &SarPacketTransferManager::finished,
                     [&](bool success) {
                         transferSuccess = success;
                         transferMessage = success ? "Transfer completed successfully." : "Transfer failed due to an error.";
                         loop.quit(); // 退出事件循环
                     });

    qDebug() << "Starting online transfer...";
    transferManager.startTransfer(binPath, ipAddress, port);
    loop.exec(); // 阻塞等待传输完成

    // 关键修正：从局部变量中获取并设置最终结果
    result.success = transferSuccess;
    result.message = transferMessage;

    return result;
}


ImageTransferResult processAndTransferManualImage(const QString &tifFilePath, const QString &auxFilePath, const QString &ipAddress, quint16 port, uint16_t image_num)
{
    ImageTransferResult result;
    result.success = false;

    // 1. 验证文件路径
    if (tifFilePath.isEmpty()) {
        result.message = "TIF file path is empty.";
        qWarning() << result.message;
        return result;
    }
    QFileInfo tifFileInfo(tifFilePath);
    if (!tifFileInfo.exists() || !tifFileInfo.isFile()) {
        result.message = QString("TIF file not found: %1").arg(tifFilePath);
        qWarning() << result.message;
        return result;
    }
    // 等待TIF文件释放
    if (!waitForFileRelease(tifFilePath)) {
        result.message = QString("TIF file %1 is locked for too long, give up processing.").arg(tifFilePath);
        qWarning() << result.message;
        return result;
    }

    // 2. 离线打包阶段：根据AUX文件路径是否为空，决定是否打包
    QString binPath = tifFilePath;
    binPath.replace(".tif", ".bin", Qt::CaseInsensitive);

    qDebug() << "Starting offline packing...";

    if (!auxFilePath.isEmpty()) {
        // SAR图像处理：TIF和AUX文件都存在
        QFileInfo auxFileInfo(auxFilePath);
        if (!auxFileInfo.exists() || !auxFileInfo.isFile()) {
            result.message = QString("AUX file not found: %1").arg(auxFilePath);
            qWarning() << result.message;
            return result;
        }
        // 等待AUX文件释放
        if (!waitForFileRelease(auxFilePath)) {
            result.message = QString("AUX file %1 is locked for too long, give up processing.").arg(auxFilePath);
            qWarning() << result.message;
            return result;
        }

        qDebug() << "Packing TIF and AUX files into a single BIN file...";
        if (!createBinFileFromTifAndAux(tifFilePath, auxFilePath, binPath, image_num)) {
            result.message = "Failed to create bin file from TIF and AUX.";
            return result;
        }
    } else {
        // ISAR图像处理：只有TIF文件，AUX路径为空
        qDebug() << "Packing TIF file only into a single BIN file...";
        // 假设createBinFileFromTifAndAux函数能够处理空的auxFilePath参数
        if (!createBinFileFromTifOnly(tifFilePath, binPath, image_num)) {
            result.message = "Failed to create bin file from TIF only.";
            return result;
        }
    }

    qDebug() << "Offline packing completed successfully.";

    // 3. 在线传输阶段（与原函数逻辑完全相同）
    SarPacketTransferManager transferManager;
    QEventLoop loop;
    bool transferSuccess = false;
    QString transferMessage;

    QObject::connect(&transferManager, &SarPacketTransferManager::finished,
                     [&](bool success) {
                         transferSuccess = success;
                         transferMessage = success ? "Transfer completed successfully." : "Transfer failed due to an error.";
                         loop.quit();
                     });

    qDebug() << "Starting online transfer...";
    transferManager.startTransfer(binPath, ipAddress, port);
    loop.exec();

    result.success = transferSuccess;
    result.message = transferMessage;

    return result;
}

/**
 * @brief SarPacketTransferManager的构造函数
 * @param packetizer 负责提供数据包的打包器实例
 * @param parent 父QObject，用于自动内存管理
 */
SarPacketTransferManager::SarPacketTransferManager(QObject *parent)
    : QObject(parent),
    m_socket(new QTcpSocket(this)),
    m_timeoutTimer(new QTimer(this))
{
    m_transferSuccessful = false;
    connect(m_socket, &QTcpSocket::connected, this, &SarPacketTransferManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &SarPacketTransferManager::onSocketDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15 及以上版本使用 errorOccurred 信号
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &SarPacketTransferManager::onSocketError);
#else
    // 旧版本 Qt 使用 error 信号
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &SarPacketTransferManager::onSocketError);
#endif
    connect(m_socket, &QTcpSocket::bytesWritten, this, &SarPacketTransferManager::onBytesWritten);
    connect(m_socket, &QTcpSocket::readyRead, this, &SarPacketTransferManager::onReadyRead);
    connect(m_timeoutTimer, &QTimer::timeout, this, &SarPacketTransferManager::onTimeout);

    m_timeoutTimer->setInterval(5000);
    m_timeoutTimer->setSingleShot(true);
}

/**
 * @brief 启动数据传输
 * @param ip 目标主机的IP地址
 * @param port 目标主机的端口号
 */
void SarPacketTransferManager::startTransfer(const QString& binFilePath, const QString& ipAddress, quint16 port)
{
    // 在这里创建 SarPacketizer，负责从bin文件中读取
    m_packetizer = new SarPacketizer(binFilePath);
    m_transferSuccessful = false;
    m_socket->connectToHost(ipAddress, port);
}

/**
 * @brief 套接字成功连接时的槽函数
 * 启动第一个数据包的发送
 */
void SarPacketTransferManager::onConnected()
{
    qDebug() << "Connected to host. Starting transfer...";
    // 连接成功后，立即开始发送第一个数据包
    sendNextPacket();
}

/**
 * @brief 写入字节后的槽函数
 * 继续发送下一个数据包，直到所有包都发送完毕
 * @param bytes 已经写入套接字的字节数
 */
void SarPacketTransferManager::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    // 每次写入数据后，继续发送下一个数据包
    sendNextPacket();
}

/**
 * @brief 套接字断开连接时的槽函数
 * 告知外部传输已完成
 */
void SarPacketTransferManager::onSocketDisconnected()
{
    qDebug() << "Disconnected from host.";
    if (!m_transferSuccessful) {
        emit finished(false);
    }
}

/**
 * @brief 套接字发生错误时的槽函数
 * 告知外部传输失败
 * @param socketError 发生的错误类型
 */
void SarPacketTransferManager::onSocketError(QAbstractSocket::SocketError socketError)
{
    qWarning() << "Socket error:" << m_socket->errorString() << "Error code:" << socketError;
    // 发生错误时，传输失败
    m_transferSuccessful = false;
    m_timeoutTimer->stop();
    emit finished(false);
}

/**
 * @brief 发送下一个数据包的私有辅助函数
 * 检查是否有更多数据包并写入套接字
 */
void SarPacketTransferManager::sendNextPacket()
{
    if (m_packetizer && m_packetizer->hasNextPacket()) {
        QByteArray packetData = m_packetizer->getNextPacket();
        if (packetData.isEmpty()) {
            qWarning() << "Failed to get next packet from bin file.";
            m_transferSuccessful = false;
            m_timeoutTimer->stop();
            m_socket->disconnectFromHost();
            emit finished(false);
            return;
        }

        qint64 bytesWritten = m_socket->write(packetData);
        if (bytesWritten == -1) {
            qWarning() << "Failed to write data to socket:" << m_socket->errorString();
            m_transferSuccessful = false;
            m_timeoutTimer->stop();
            m_socket->disconnectFromHost();
            emit finished(false);
        }
    } else {
//        qDebug() << "All packets sent. Waiting for acknowledgment...";
        // 所有数据已发送，开始等待确认
//        m_timeoutTimer->start();
        emit finished(true);
    }
}

void SarPacketTransferManager::onReadyRead()
{
    QByteArray ackMessage = m_socket->readAll();
    // 假设接收端发送 "OK" 作为确认消息
    if (ackMessage == "OK") {
        qDebug() << "Received acknowledgment from receiver. Disconnecting...";
        m_transferSuccessful = true;
        m_timeoutTimer->stop();
        m_socket->disconnectFromHost();
        emit finished(true);
    }
}

void SarPacketTransferManager::onTimeout()
{
    qWarning() << "Timeout waiting for acknowledgment. Disconnecting...";
    m_transferSuccessful = false;
    m_socket->disconnectFromHost();
    emit finished(false);
}
