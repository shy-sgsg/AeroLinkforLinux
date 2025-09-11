#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQueue>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include "tcp_server_thread.h"
#include "file_monitor.h"
#include "message_transfer.h"
#include "image_transfer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    FileMonitor* fileMonitor;

private slots:
    void on_pushButton_clicked();
    void on_stopButton_clicked();
    void on_browseButton_clicked();
    void on_sendMessageButton_clicked();
    void onLogMessage(const QString &message);
    void updateStatistics();
    void processAndTransferFile(const QString &filePath);
    void on_selectTifButton_clicked();
    void on_selectAuxButton_clicked();
    void on_manualSendButton_clicked();
    void on_sarCheckBox_stateChanged(int state);
    void on_isarCheckBox_stateChanged(int state);
    void on_toggleServerButton_clicked(); // 新增：合并后的槽函数
    void onServerStarted(); // 新增：服务器成功启动时更新UI的槽
    void onServerStopped(); // 新增：服务器成功停止时更新UI的槽
    void on_sendTestDataButton_clicked();
    void onReceivedImageData(quint16 image_num, qint16 pixel_x, qint16 pixel_y);

private:
    Ui::MainWindow *ui;
    QFileSystemWatcher* m_mainWatcher; // 新增：主文件夹监控器
    QFileSystemWatcher* m_subWatcher;  // 新增：子文件夹监控器

    QMap<QString, FileStatus> m_fileStatus; // 文件状态映射
    QMutex m_mutex;

    qint64 m_fileSize;
    qint64 m_bytesWrittenTotal;

    // 消息传输类
    class MessageTransfer* m_messageTransfer;

    QCheckBox* sarCheckBox;
    QCheckBox* isarCheckBox;
    QLineEdit* tifPathLineEdit;
    QLineEdit* auxPathLineEdit;
    QPushButton* selectTifButton;
    QPushButton* selectAuxButton;
    QPushButton* manualSendButton;
    QButtonGroup* imageTypeButtonGroup;

    TcpServerThread* m_tcpServerThreadObject;
    QThread* m_serverThread;
    bool m_isServerRunning = false; // 新增：服务器运行状态标记

    QTcpSocket* m_testSocket;

};
#endif // MAINWINDOW_H
