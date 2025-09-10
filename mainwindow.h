#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQueue>
#include <QMap>
#include <QSet>
#include <QMutex>
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



};
#endif // MAINWINDOW_H
