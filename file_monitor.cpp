#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QSet>
#include <QThread>
#include "file_monitor.h"

FileMonitor::FileMonitor(QObject* parent) : QObject(parent) {
    m_mainWatcher = new QFileSystemWatcher(this);
    m_subWatcher = new QFileSystemWatcher(this);
    connect(m_mainWatcher, &QFileSystemWatcher::directoryChanged, this, &FileMonitor::onMainDirectoryChanged);
    connect(m_subWatcher, &QFileSystemWatcher::directoryChanged, this, &FileMonitor::onSubdirectoryChanged);
}

void FileMonitor::setMainFolder(const QString& folderPath) {
    m_mainFolderPath = folderPath;
}

void FileMonitor::start() {
    if (!m_mainFolderPath.isEmpty()) {
        stop(); // 停止所有监控，避免重复添加路径
        m_mainWatcher->addPath(m_mainFolderPath);
        qDebug() << "Watching main folder:" << m_mainFolderPath;

        QDir mainDir(m_mainFolderPath);
        QStringList subDirs = mainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

        if (!subDirs.isEmpty()) {
            QString latestSubDir = mainDir.filePath(subDirs.first());

            // --- 添加延迟和重试机制以等待 result/ID1 文件被创建 ---
            QString filePathToMonitor = QDir(latestSubDir).filePath("result/ID1");
            const int maxRetries = 10;
            const int retryDelayMs = 500; // 500毫秒
            bool fileExists = false;

            for (int i = 0; i < maxRetries; ++i) {
                if (QFileInfo::exists(filePathToMonitor)) {
                    fileExists = true;
                    break;
                }
                qDebug() << "File" << filePathToMonitor << "not found. Retrying in" << retryDelayMs << "ms... (Attempt" << i + 1 << "/" << maxRetries << ")";
                // WARNING: QThread::msleep() is a blocking call.
                // It will freeze the main application UI. For a responsive UI,
                // consider using a non-blocking approach like QTimer.
                QThread::msleep(retryDelayMs);
            }

            if (!fileExists) {
                qCritical() << "Error: File" << filePathToMonitor << "still not found after" << maxRetries << "attempts. Aborting start().";
                return;
            }
            // --- 延迟和重试机制结束 ---

            // 修正：m_currentSubDir现在精确地指向result/ID1
            m_currentSubDir = filePathToMonitor;
            m_subWatcher->addPath(m_currentSubDir);
            qDebug() << "Watching newest sub-directory:" << m_currentSubDir;
            emit subDirChanged(m_currentSubDir);

            // 扫描最新子文件夹中的所有TIF文件，并添加到已处理的集合中
            m_processedFiles.clear(); // 清空旧的集合
            QDir subDir(m_currentSubDir);
            QStringList tifFiles = subDir.entryList(QStringList("*.tif"), QDir::Files | QDir::NoDotAndDotDot);
            for (const QString& fileName : tifFiles) {
                QString fullPath = subDir.filePath(fileName);
                m_processedFiles.insert(fullPath); // 将所有现有文件都标记为已处理
                qDebug() << "Found existing .tif file:" << fullPath;
                emit newTifFileDetected(fullPath);
            }
        } else {
            qDebug() << "No sub-directories found in main folder.";
        }
        emit mainDirChanged(m_mainFolderPath);
    } else {
        qDebug() << "Main folder path is empty. Aborting start().";
    }
}


void FileMonitor::stop() {
    if (m_mainWatcher->directories().isEmpty() && m_subWatcher->directories().isEmpty()) {
        return;
    }
    m_mainWatcher->removePaths(m_mainWatcher->directories());
    m_subWatcher->removePaths(m_subWatcher->directories());
    qDebug() << "停止监控文件夹...";
}

QString FileMonitor::getCurrentSubDir() const {
    return m_currentSubDir;
}

void FileMonitor::onMainDirectoryChanged(const QString& path) {
    QDir dir(path);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

    if (!subDirs.isEmpty()) {
        QString latestSubDir = dir.filePath(subDirs.first());

        // --- 添加延迟和重试机制 ---
        QString filePathToMonitor = QDir(latestSubDir).filePath("result/ID1");
        const int maxRetries = 10;
        const int retryDelayMs = 500; // 500毫秒
        bool fileExists = false;

        for (int i = 0; i < maxRetries; ++i) {
            if (QFileInfo::exists(filePathToMonitor)) {
                fileExists = true;
                break;
            }
            qDebug() << "File" << filePathToMonitor << "not found. Retrying in" << retryDelayMs << "ms... (Attempt" << i + 1 << "/" << maxRetries << ")";
            QThread::msleep(retryDelayMs);
        }

        if (!fileExists) {
            qCritical() << "Error: File" << filePathToMonitor << "still not found after" << maxRetries << "attempts. Aborting.";
            return;
        }
        // --- 延迟和重试机制结束 ---

        // 确保新找到的文件夹存在且不是当前监控的文件夹
        // 修正：现在我们检查的是精确的result/ID1路径，而不是根子文件夹
        if (m_currentSubDir != filePathToMonitor) {
            qDebug() << "Detected new newest sub-directory:" << filePathToMonitor;
            // 移除旧的监控
            m_subWatcher->removePaths(m_subWatcher->directories());
            // 添加新的监控
            m_subWatcher->addPath(filePathToMonitor);
            // 更新当前子文件夹路径
            m_currentSubDir = filePathToMonitor;
            // 清空已处理文件集合，准备处理新文件夹中的文件
            m_processedFiles.clear();
            // 发出信号，通知外部最新子文件夹已切换
            emit subDirChanged(filePathToMonitor);
        }
    }
    // 即使没有切换，也发出主目录变化信号（如果需要）
    emit mainDirChanged(path);
}

void FileMonitor::onSubdirectoryChanged(const QString& path) {
    QDir dir(path);
    QStringList currentFiles = dir.entryList(QStringList("*.tif"), QDir::Files | QDir::NoDotAndDotDot);

    // 找出新增的文件
    for (const QString& fileName : currentFiles) {
        QString fullPath = dir.filePath(fileName);
        if (!m_processedFiles.contains(fullPath)) {
            // 这是新文件，发出信号并添加到已处理集合
            qDebug() << "New .tif file detected:" << fullPath;
            emit newTifFileDetected(fullPath);
            m_processedFiles.insert(fullPath);
        }
    }
}
