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
#include <QButtonGroup>
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

    imageTypeButtonGroup = new QButtonGroup(this);
    imageTypeButtonGroup->setExclusive(true); // 设置为互斥模式
    imageTypeButtonGroup->addButton(ui->sarCheckBox);
    imageTypeButtonGroup->addButton(ui->isarCheckBox);
    ui->sarCheckBox->setChecked(true);
    ui->auxPathLineEdit->setEnabled(true);
    ui->selectAuxButton->setEnabled(true);
    connect(ui->sarCheckBox, &QCheckBox::stateChanged, this, &MainWindow::on_sarCheckBox_stateChanged);
    connect(ui->isarCheckBox, &QCheckBox::stateChanged, this, &MainWindow::on_isarCheckBox_stateChanged);

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

// 槽函数：选择TIF文件
void MainWindow::on_selectTifButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择TIF文件", "", "TIF文件 (*.tif)");
    if (!filePath.isEmpty()) {
        ui->tifPathLineEdit->setText(filePath);
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
    QString tifFilePath = ui->tifPathLineEdit->text();
    QString ipAddress = ui->ipAddressLineEdit->text();
    quint16 port = ui->portLineEdit->text().toUShort();
    uint16_t image_num = 1; // 示例值，根据实际情况调整

    if (tifFilePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择一个TIF文件！");
        return;
    }

    ImageTransferResult result;

    // 根据复选框状态决定是否传递AUX文件路径
    if (ui->isarCheckBox->isChecked()) {
        ui->textEdit_Log->append("发送ISAR图像，仅发送TIF文件。");
        result = processAndTransferManualImage(tifFilePath, QString(), ipAddress, port, image_num);
    } else {
        QString auxFilePath = ui->auxPathLineEdit->text();
        if (auxFilePath.isEmpty()) {
            QMessageBox::warning(this, "警告", "请选择一个AUX文件！");
            return;
        }
        ui->textEdit_Log->append("发送SAR图像，打包TIF和AUX文件。");
        result = processAndTransferManualImage(tifFilePath, auxFilePath, ipAddress, port, image_num);
    }

    // 处理传输结果并更新日志
    if (result.success) {
        ui->textEdit_Log->append("手动数传成功：" + result.message);
    } else {
        ui->textEdit_Log->append("手动数传失败：" + result.message);
    }
}

// 槽函数：SAR复选框状态改变
void MainWindow::on_sarCheckBox_stateChanged(int state)
{
    // 检查SAR复选框是否被选中
    if (state == Qt::Checked) {
        // 如果选中，则启用AUX文件相关控件
        ui->auxPathLineEdit->setEnabled(true);
        ui->selectAuxButton->setEnabled(true);
    }
}

// 槽函数：ISAR复选框状态改变
void MainWindow::on_isarCheckBox_stateChanged(int state)
{
    // 检查ISAR复选框是否被选中
    if (state == Qt::Checked) {
        // 如果选中，则禁用AUX文件相关控件并清空路径
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



