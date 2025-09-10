#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QTimer>
#include "logmanager.h"
#include <QImageReader>
#include <QThread>
#include "image_transfer.h"
#include "file_monitor.h"
#include "message_transfer.h"
#include "package_sar_data.h"

QString mainFolderPath = "E:/AIR/小长ISAR/实时数据回传/data";

QString ipAddress = "127.0.0.1";
quint16 port = 65432;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    fileMonitor = new FileMonitor(this);
    connect(fileMonitor, &FileMonitor::newTifFileDetected, this, &MainWindow::processAndTransferFile);
    // 可选：连接 mainDirChanged、subDirChanged 信号做UI更新
    ui->setupUi(this);
    ui->ipAddressLineEdit->setPlaceholderText("请输入 IP 地址");
    ui->portLineEdit->setPlaceholderText("请输入端口号");
    ui->pathLineEdit->setPlaceholderText("请输入监控文件夹路径"); // ✅ 设置路径编辑框占位符
    ui->pathLineEdit->setText(mainFolderPath); // ✅ 将硬编码路径设为默认值

    connect(&LogManager::instance(), &LogManager::logMessage, this, &MainWindow::onLogMessage);

    updateStatistics();

//    QImageReader::setAllocationLimit(1024);
//    QImageReader reader;
//    reader.setAllocationLimit(1024);  // 旧版本中是实例函数，作用是设置该实例的内存上限

    qRegisterMetaType<qint64>("qint64");

    // 初始化消息传输类
    m_messageTransfer = new MessageTransfer(this);
    connect(m_messageTransfer, &MessageTransfer::logMessage, this, &MainWindow::onLogMessage);

    const std::string inputFileName1 = "E:/AIR/raw_data_20250908_214537.bin";
    const std::string outputFileName1 = "E:/AIR/raw_data_20250908_214537.jpg";
//     const std::string inputFileName2 = "E:/AIR/raw_data_20250907_130209.bin";
//     const std::string outputFileName2 = "E:/AIR/raw_data_20250907_130209.jpg";
    unpackage_sar_data(inputFileName1, outputFileName1);
//     unpackage_sar_data(inputFileName2, outputFileName2);
}

MainWindow::~MainWindow()
{
    delete ui;
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

// “开始监控”按钮的槽函数
void MainWindow::on_pushButton_clicked()
{
    ipAddress = ui->ipAddressLineEdit->text();
    port = ui->portLineEdit->text().toUShort();

    if (ipAddress.isEmpty() || port == 0) {
        qDebug() << "请正确填写IP地址与端口号！";
        QMessageBox::warning(this, "警告", "请正确填写IP地址与端口号！");
        return;
    }

    mainFolderPath = ui->pathLineEdit->text();
    if (!QDir(mainFolderPath).exists()) {
        qDebug() << "错误：指定的监控路径不存在：" << mainFolderPath;
        QMessageBox::warning(this, "警告", "指定的监控文件夹不存在。");
        return;
    }
    fileMonitor->setMainFolder(mainFolderPath);
    fileMonitor->start();
    updateStatistics();
}

// “停止监控”按钮的槽函数
void MainWindow::on_stopButton_clicked()
{
    fileMonitor->stop();
}

// 浏览按钮的槽函数
void MainWindow::on_browseButton_clicked()
{
    QString selectedDir = QFileDialog::getExistingDirectory(this, "选择监控文件夹", ui->pathLineEdit->text());
    if (!selectedDir.isEmpty()) {
        ui->pathLineEdit->setText(selectedDir);
    }
}

// 只做信号转发和状态更新，实际处理交给image_transfer
void MainWindow::processAndTransferFile(const QString &filePath)
{
    // 关键修正：使用 QMutexLocker 确保线程安全
    QMutexLocker locker(&m_mutex);
    // 检查文件是否已经在处理中。
    // 如果存在并且状态不是 Success 或 Failure，则认为它正在被处理，直接返回。
    if (m_fileStatus.contains(filePath) && m_fileStatus.value(filePath) != Success && m_fileStatus.value(filePath) != Failure) {
        qDebug() << "File" << filePath << "is already being processed. Skipping duplicate signal.";
        return;
    }
    // 将新文件状态设置为 Pending，表示任务已开始
    m_fileStatus[filePath] = Pending;
    // 释放锁，以便其他线程可以访问。
    // 在调用耗时函数 processAndTransferImage() 期间，我们不需要保持锁定。
    locker.unlock();
    qDebug() << "Processing new file:" << filePath;
    ipAddress = ui->ipAddressLineEdit->text();
    port = ui->portLineEdit->text().toUShort();
    uint16_t image_num = 1;
    auto result = processAndTransferImage(filePath, ipAddress, port, image_num);
    // 再次锁定互斥量，以便安全地更新共享状态
    locker.relock();
    // 根据处理结果更新文件状态
    m_fileStatus[filePath] = result.success ? Success : Failure;
    // 释放锁
    locker.unlock();
    // 更新界面统计信息
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



