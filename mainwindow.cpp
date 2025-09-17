#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QTimer>
#include <QButtonGroup>
#include <QImageReader>
#include <QThread>
#include <QXmlStreamWriter>
#include "logmanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "radar_protocol.h"
#include "image_transfer.h"
#include "file_monitor.h"
#include "message_transfer.h"

QString mainFolderPath = "E:/AIR/小长ISAR/实时数据回传/data";

QString ipAddress = "127.0.0.1";
quint16 port = 65432;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_imageCounter(0)
    , m_isarRequestCounter(0)
{
    fileMonitor = new FileMonitor(this);
    connect(fileMonitor, &FileMonitor::newFileDetected, this, &MainWindow::processAndTransferFile);
    // 可选：连接 mainDirChanged、subDirChanged 信号做UI更新
    ui->setupUi(this);
    ui->ipAddressLineEdit->setPlaceholderText("请输入 IP 地址");
    ui->portLineEdit->setPlaceholderText("请输入端口号");
    ui->pathLineEdit->setPlaceholderText("请输入监控文件夹路径"); // ✅ 设置路径编辑框占位符
    ui->pathLineEdit->setText(mainFolderPath); // ✅ 将硬编码路径设为默认值

    imageTypeButtonGroup = new QButtonGroup(this);
    imageTypeButtonGroup->setExclusive(true); // 设置为互斥模式
    imageTypeButtonGroup->addButton(ui->sarCheckBox);
    imageTypeButtonGroup->addButton(ui->isarCheckBox);
    imageTypeButtonGroup->addButton(ui->GMTICheckBox);
    ui->sarCheckBox->setChecked(true);
    ui->auxPathLineEdit->setEnabled(true);
    ui->selectAuxButton->setEnabled(true);
    connect(ui->sarCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::on_sarCheckBox_stateChanged);
    connect(ui->isarCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::on_isarCheckBox_stateChanged);
    connect(ui->GMTICheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::on_GMTICheckBox_stateChanged);

    connect(&LogManager::instance(), &LogManager::logMessage, this, &MainWindow::onLogMessage);

    updateStatistics();

//    QImageReader::setAllocationLimit(1024);
//    QImageReader reader;
//    reader.setAllocationLimit(1024);  // 旧版本中是实例函数，作用是设置该实例的内存上限

    qRegisterMetaType<qint64>("qint64");

    // 设置TCP服务器线程
    m_serverThread = new QThread(this);
    m_tcpServerThreadObject = new TcpServerThread();
    m_tcpServerThreadObject->moveToThread(m_serverThread);

    // 连接主窗口的槽函数到服务器状态变化信号
    // 您需要在 TcpServerThread 中添加这些信号
    connect(m_tcpServerThreadObject, &TcpServerThread::serverStarted, this, &MainWindow::onServerStarted);
    connect(m_tcpServerThreadObject, &TcpServerThread::serverStopped, this, &MainWindow::onServerStopped);
    connect(m_tcpServerThreadObject, &TcpServerThread::logMessage, this, &MainWindow::onLogMessage);
    connect(m_tcpServerThreadObject, &TcpServerThread::receivedIsarRequest, this, &MainWindow::onReceivedIsarRequest);

    // 启动线程
    m_serverThread->start();

    // 初始化按钮状态
    ui->toggleServerButton->setText("启动服务器");
    ui->toggleServerButton->setEnabled(true);

    m_testSocket = new QTcpSocket(this);
}

MainWindow::~MainWindow()
{
    // 安全地停止线程
    m_serverThread->quit();
    m_serverThread->wait();
    delete m_tcpServerThreadObject;
    delete ui;
}

void MainWindow::onReceivedIsarRequest(quint16 imageNumber, quint16 offsetX, quint16 offsetY)
{
    // 1. 递增请求计数器，并使用它的新值
    m_isarRequestCounter++;
    quint16 requestId = m_isarRequestCounter;

    // 2. 在主线程中安全地创建和显示弹窗
    QMessageBox::information(this,
                             "收到ISAR成像请求",
                             QString("请求编号：%1\n图像编号：%2\n像素偏移x：%3\n像素偏移y：%4")
                                 .arg(requestId)
                                 .arg(imageNumber)
                                 .arg(offsetX)
                                 .arg(offsetY));

    // 3. 获取TIF文件路径
    QString tifPath = getImagePath(imageNumber);
    if (tifPath.isEmpty()) {
        qWarning() << "未找到图像编号 " << imageNumber << " 对应的文件路径。无法生成XML文件。";
        return;
    }

    // 4. 构造SLC和AUX文件路径
    QFileInfo tifInfo(tifPath);
    QString baseName = tifInfo.completeBaseName();
    QString directory = tifInfo.absolutePath();

    QString slcPath = QDir(directory).filePath("SLC_" + baseName.replace("IMG_", "") + ".slc");
    QString auxPath = QDir(directory).filePath("AUX_" + baseName.replace("IMG_", "") + ".dat");

    // 5. 构造XML文件名，现在包含请求编号
    QString xmlPath = QDir(directory).filePath(QString("ISARConfig_IMG_%1_REQ_%2.xml").arg(imageNumber).arg(requestId));

    // 6. 使用 QXmlStreamWriter 写入XML文件
    QFile file(xmlPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开文件 " << xmlPath << " 进行写入。错误：" << file.errorString();
        return;
    }

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement("ISARConfig");

    xmlWriter.writeTextElement("file_slc", slcPath);
    xmlWriter.writeTextElement("file_aux", auxPath);
    xmlWriter.writeTextElement("rg_center", QString::number(offsetX));
    xmlWriter.writeTextElement("az_center", QString::number(offsetY));
    xmlWriter.writeTextElement("rg_win_len", "1024");
    xmlWriter.writeTextElement("az_win_len", "4096");
    xmlWriter.writeTextElement("request_id", QString::number(requestId)); // 将请求编号写入XML

    xmlWriter.writeEndElement(); // </ISARConfig>
    xmlWriter.writeEndDocument();

    file.close();

    qDebug() << "XML配置文件已成功生成：" << xmlPath;
}

void MainWindow::on_sendTestDataButton_clicked()
{
    // 获取服务器IP和端口号
    QString ipAddress = ui->serverIpLineEdit->text();
    quint16 port = ui->serverPortLineEdit->text().toUShort();

    if (ipAddress.isEmpty() || port == 0) {
        QMessageBox::warning(this, "警告", "请先正确设置服务器的IP地址和端口号！");
        return;
    }

    // 确保套接字处于断开状态
    if (m_testSocket->state() == QAbstractSocket::ConnectedState) {
        m_testSocket->disconnectFromHost();
        m_testSocket->waitForDisconnected();
    }

    // 连接到服务器
    m_testSocket->connectToHost(ipAddress, port);
    if (!m_testSocket->waitForConnected(5000)) { // 5秒超时
        qDebug() << "连接服务器失败:" << m_testSocket->errorString();
        return;
    }

    qDebug() << "测试客户端已连接，正在打包数据...";

    // 1. 构造数据信息结构体
    DataInfo dataInfo = {};
    dataInfo.frame_header = qToLittleEndian(static_cast<quint16>(0x55AA));
    dataInfo.data_length = qToLittleEndian(static_cast<quint32>(sizeof(DataInfo) + sizeof(DataHeader)));
    dataInfo.message_count = qToLittleEndian(static_cast<quint16>(1));
    dataInfo.source_address = qToLittleEndian(static_cast<quint16>(100));
    dataInfo.destination_address = qToLittleEndian(static_cast<quint16>(200));
    dataInfo.command_type = 0x01;
    dataInfo.reserved = 0;
    dataInfo.image_number = qToLittleEndian(static_cast<quint16>(3));
    dataInfo.pixel_offset_x = qToLittleEndian(static_cast<qint16>(50));
    dataInfo.pixel_offset_y = qToLittleEndian(static_cast<qint16>(100));
    dataInfo.total_y = qToLittleEndian(static_cast<qint16>(4096));

    // 2. 将 DataInfo 结构体转换为字节数组
    QByteArray dataInfoPayload(reinterpret_cast<const char*>(&dataInfo), sizeof(DataInfo));

    // 3. 计算校验和
    quint8 checksum = calculateChecksum(dataInfoPayload);

    // 4. 构造数据帧头结构体
    DataHeader dataHeader = {};
    dataHeader.fixed_value = qToLittleEndian(static_cast<quint16>(0x9EE9));
    dataHeader.data_length = qToLittleEndian(static_cast<quint16>(dataInfoPayload.size()));
    dataHeader.checksum = checksum;

    // 5. 拼接完整数据包
    QByteArray fullPacket;
    fullPacket.append(reinterpret_cast<const char*>(&dataHeader), sizeof(DataHeader));
    fullPacket.append(dataInfoPayload);

    qDebug() << "准备发送测试数据包，总大小:" << fullPacket.size();
    qDebug() << "  - 校验和:" << QString::number(checksum, 16);
    qDebug() << "  - 图像编号:" << qFromLittleEndian(dataInfo.image_number);
    qDebug() << "  - 像素偏移x:" << qFromLittleEndian(dataInfo.pixel_offset_x);
    qDebug() << "  - 像素偏移y:" << qFromLittleEndian(dataInfo.pixel_offset_y);
    qDebug() << "  - 图像总列数:" << qFromLittleEndian(dataInfo.total_y);

    // 6. 发送数据
    m_testSocket->write(fullPacket);
    m_testSocket->flush();

    qDebug() << "测试数据已发送。";
    m_testSocket->disconnectFromHost();
}

// 新增：合并后的槽函数，用于启动和停止服务器
void MainWindow::on_toggleServerButton_clicked()
{
    if (!m_isServerRunning) {
        // 当前为停止状态，执行启动操作
        QString ipAddress = ui->serverIpLineEdit->text();
        quint16 port = ui->serverPortLineEdit->text().toUShort();
        if (ipAddress.isEmpty() || port == 0) {
            QMessageBox::warning(this, "警告", "IP地址或端口号无效，请检查输入！");
            return;
        }
        ui->toggleServerButton->setEnabled(false); // 启动过程中禁用按钮
        QMetaObject::invokeMethod(m_tcpServerThreadObject, "startServer", Qt::QueuedConnection, Q_ARG(QString, ipAddress), Q_ARG(quint16, port));

    } else {
        // 当前为运行状态，执行停止操作
        ui->toggleServerButton->setEnabled(false); // 停止过程中禁用按钮
        QMetaObject::invokeMethod(m_tcpServerThreadObject, "stopServer", Qt::QueuedConnection);
    }
}

// 新增：服务器成功启动时更新UI的槽
void MainWindow::onServerStarted()
{
    m_isServerRunning = true;
    ui->toggleServerButton->setText("停止服务器");
    ui->toggleServerButton->setEnabled(true);
    // 您也可以在这里禁用IP和端口编辑框，防止运行时修改
    ui->serverIpLineEdit->setEnabled(false);
    ui->serverPortLineEdit->setEnabled(false);
}

// 新增：服务器成功停止时更新UI的槽
void MainWindow::onServerStopped()
{
    m_isServerRunning = false;
    ui->toggleServerButton->setText("启动服务器");
    ui->toggleServerButton->setEnabled(true);
    // 重新启用IP和端口编辑框
    ui->serverIpLineEdit->setEnabled(true);
    ui->serverPortLineEdit->setEnabled(true);
}

// 槽函数：选择TIF文件
void MainWindow::on_selectImageButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择图像文件", "", "图像文件 (*.tif *.jpg *.png)");
    if (!filePath.isEmpty()) {
        ui->imagePathLineEdit->setText(filePath);
    }
}

// 槽函数：选择AUX文件
void MainWindow::on_selectAuxButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择AUX文件", "", "AUX文件 (*.dat)");
    if (!filePath.isEmpty()) {
        ui->auxPathLineEdit->setText(filePath);
    }
}

void MainWindow::on_manualSendButton_clicked()
{
    QString ipAddress = ui->ipAddressLineEdit->text();
    quint16 port = ui->portLineEdit->text().toUShort();

    uint16_t currentImageNum = m_imageCounter;
    m_imageCounter++;

    ImageTransferResult result;

    QString imageFilePath = ui->imagePathLineEdit->text();
    if (imageFilePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择一个图像文件！");
        return;
    }

    if (ui->GMTICheckBox->isChecked()) {
        qDebug() << "发送GMTI图像以及目标信息包。";
        result = processAndTransferGMTI(imageFilePath, ipAddress, port, currentImageNum);
    }
    else {
        // 根据复选框状态决定是否传递AUX文件路径
        if (ui->isarCheckBox->isChecked()) {
            qDebug() << "发送ISAR图像，仅发送TIF文件。";
            result = processAndTransferManualImage(imageFilePath, QString(), ipAddress, port, currentImageNum);
        } else {
            QString auxFilePath = ui->auxPathLineEdit->text();
            if (auxFilePath.isEmpty()) {
                QMessageBox::warning(this, "警告", "请选择一个AUX文件！");
                return;
            }
            qDebug() << "发送SAR图像，打包TIF和AUX文件。";
            result = processAndTransferManualImage(imageFilePath, auxFilePath, ipAddress, port, currentImageNum);
        }
    }

    // 处理传输结果并更新日志
    if (result.success) {
        m_fileStatusMutex.lock();
        m_imageLog[currentImageNum] = imageFilePath;
        m_fileStatusMutex.unlock();
        qDebug() << QString("手动数传成功。图片编号: %1, 文件路径: %2").arg(currentImageNum).arg(imageFilePath);
    } else {
        qDebug() << ("手动数传失败：" + result.message);
    }
}

// 槽函数：SAR复选框状态改变
void MainWindow::on_sarCheckBox_stateChanged(Qt::CheckState state)
{
    // 检查SAR复选框是否被选中
    if (state == Qt::Checked) {
        // 如果选中，则启用AUX文件相关控件
        ui->auxPathLineEdit->setEnabled(true);
        ui->selectAuxButton->setEnabled(true);
    }
}

// 槽函数：ISAR复选框状态改变
void MainWindow::on_isarCheckBox_stateChanged(Qt::CheckState state)
{
    // 检查ISAR复选框是否被选中
    if (state == Qt::Checked) {
        // 如果选中，则禁用AUX文件相关控件并清空路径
        ui->auxPathLineEdit->setEnabled(false);
        ui->selectAuxButton->setEnabled(false);
        ui->auxPathLineEdit->clear();
    }
}

void MainWindow::on_GMTICheckBox_stateChanged(Qt::CheckState state)
{
    if (state == Qt::Checked) {
        ui->auxPathLineEdit->setEnabled(false);
        ui->selectAuxButton->setEnabled(false);
        ui->auxPathLineEdit->clear();
    }
}

// 发送消息按钮的槽函数，使用MessageTransfer
void MainWindow::on_sendMessageButton_clicked()
{
    QString message = ui->messageLineEdit->text().trimmed();
    if (!message.isEmpty()) {
        m_messageTransfer->sendMessage(message, ipAddress, port);
        ui->messageLineEdit->clear();
    }
}

void MainWindow::on_toggleMonitorButton_clicked()
{
    if (!m_isMonitoring) {
        // 当前为停止状态，执行启动操作
        ipAddress = ui->ipAddressLineEdit->text();
        port = ui->portLineEdit->text().toUShort();

        if (ipAddress.isEmpty() || port == 0) {
            QMessageBox::warning(this, "警告", "请正确填写IP地址与端口号！");
            return;
        }

        mainFolderPath = ui->pathLineEdit->text();
        if (!QDir(mainFolderPath).exists()) {
            QMessageBox::warning(this, "警告", "指定的监控文件夹不存在。");
            return;
        }

        bool isGMTI = ui->GMTICheckBox->isChecked();
        fileMonitor->setMainFolder(mainFolderPath);
        fileMonitor->start(isGMTI); // ✅ 调用新的 start 函数

        m_isMonitoring = true;
        ui->toggleMonitorButton->setText("停止监控");
        qDebug() << "开始监控文件夹：" << mainFolderPath;
    } else {
        // 当前为运行状态，执行停止操作
        fileMonitor->stop();
        m_isMonitoring = false;
        ui->toggleMonitorButton->setText("开始监控");
        qDebug() << "停止监控。";
    }
}

// 浏览按钮的槽函数
void MainWindow::on_browseButton_clicked()
{
    QString selectedDir = QFileDialog::getExistingDirectory(this, "选择监控文件夹", ui->pathLineEdit->text());
    if (!selectedDir.isEmpty()) {
        ui->pathLineEdit->setText(selectedDir);
    }
}

// ✅ 修改后的 processAndTransferFile 槽函数
void MainWindow::processAndTransferFile(const QString &filePath)
{
    QMutexLocker locker(&m_fileStatusMutex);
    if (m_fileStatus.contains(filePath) && m_fileStatus.value(filePath) != Success && m_fileStatus.value(filePath) != Failure) {
        qDebug() << "File" << filePath << "is already being processed. Skipping duplicate signal.";
        return;
    }
    m_fileStatus[filePath] = Pending;

    uint16_t currentImageNum = m_imageCounter++;
    locker.unlock();

    qDebug() << "Processing new file:" << filePath;
    ipAddress = ui->ipAddressLineEdit->text();
    port = ui->portLineEdit->text().toUShort();

    ImageTransferResult result;

    if (QFileInfo(filePath).suffix().toLower() == "bin") {
        qDebug() << "Detected a .bin file. Processing in GMTI mode.";
        result = processAndTransferGMTI(filePath, ipAddress, port, currentImageNum);
    } else if (QFileInfo(filePath).suffix().toLower() == "tif") {
        qDebug() << "Detected a .tif file. Processing in SAR/ISAR mode.";
        result = processAndTransferImage(filePath, ipAddress, port, currentImageNum);
    } else {
        qWarning() << "Unsupported file type detected:" << filePath;
        result.success = false;
        result.message = "Unsupported file type";
    }

    locker.relock();
    m_fileStatus[filePath] = result.success ? Success : Failure;
    if (result.success) {
        m_imageLog[currentImageNum] = filePath;
        qDebug() << QString("自动数传成功。图片编号: %1, 文件路径: %2").arg(currentImageNum).arg(filePath);
    } else {
        qDebug() << ("自动数传失败：" + result.message);
    }
    locker.unlock();
    updateStatistics();
    qDebug() << "File" << filePath << (result.success ? "processed successfully." : "failed to process.");
}

// 接收日志消息的槽函数
void MainWindow::onLogMessage(const QString &message)
{
    if (ui->textEdit_Log) {
        ui->textEdit_Log->append(message);
    }
}

// 更新统计标签的槽函数
void MainWindow::updateStatistics()
{
    int totalFiles = m_fileStatus.size();
    int successFiles = 0;
    int failedFiles = 0;

    for (FileStatus status : m_fileStatus.values()) {
        if (status == Success) {
            successFiles++;
        } else if (status == Failure) {
            failedFiles++;
        }
    }

    if (ui->label_total) {
        ui->label_total->setText(QString("总文件数：%1").arg(totalFiles));
    }
    if (ui->label_success) {
        ui->label_success->setText(QString("成功发送：%1").arg(successFiles));
    }
    if (ui->label_failed) {
        ui->label_failed->setText(QString("发送失败：%1").arg(failedFiles));
    }
}

/**
 * @brief 根据图像编号查询文件路径
 * @param imageNum 图像的唯一编号
 * @return 对应的文件路径，如果未找到则返回空字符串
 */
QString MainWindow::getImagePath(uint16_t imageNum)
{
    QMutexLocker locker(&m_fileStatusMutex); // 使用互斥锁确保线程安全
    if (m_imageLog.contains(imageNum)) {
        return m_imageLog.value(imageNum);
    }
    return QString(); // 如果未找到，返回一个空的 QString
}

// 槽函数：演示查询功能
// 假设UI上有一个名为 queryImageButton 的按钮和一个名为 imageNumLineEdit 的文本框
void MainWindow::on_queryImageButton_clicked()
{
    bool ok;
    uint16_t imageNum = ui->imageNumLineEdit->text().toUInt(&ok);
    if (!ok) {
        qDebug() << "查询失败：请输入一个有效的图像编号。";
        return;
    }

    QString imagePath = getImagePath(imageNum);

    if (imagePath.isEmpty()) {
        qDebug() << QString("查询失败：未找到图像编号 %1 对应的文件路径。").arg(imageNum);
    } else {
        qDebug() << QString("查询成功：图像编号 %1 对应的文件路径为：\n%2").arg(imageNum).arg(imagePath);
    }
}



