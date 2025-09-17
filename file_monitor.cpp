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

void FileMonitor::start(bool isGMTIMonitoring) { // ✅ 修改: 接收一个bool参数
    if (m_mainFolderPath.isEmpty()) {
        qDebug() << "Main folder path is empty. Aborting start().";
        return;
    }
    stop(); // 停止所有监控，避免重复添加路径
    m_isGMTIMonitoring = isGMTIMonitoring;

    if (m_isGMTIMonitoring) {
        // GMTI 模式：只监控主文件夹
        m_mainWatcher->addPath(m_mainFolderPath);
        m_processedBinFiles.clear();
        QDir mainDir(m_mainFolderPath);
        // QStringList binFiles = mainDir.entryList(QStringList("*.bin"), QDir::Files | QDir::NoDotAndDotDot);
        // for (const QString& fileName : binFiles) {
        //     QString fullPath = mainDir.filePath(fileName);
        //     m_processedBinFiles.insert(fullPath);
        //     qDebug() << "Found existing .bin file:" << fullPath;
        // }
        qDebug() << "Watching main folder for .bin files:" << m_mainFolderPath;
    } else {
        // SAR/ISAR 模式：监控子文件夹
        m_mainWatcher->addPath(m_mainFolderPath);
        qDebug() << "Watching main folder for sub-directories:" << m_mainFolderPath;

        QDir mainDir(m_mainFolderPath);
        QStringList subDirs = mainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

        if (!subDirs.isEmpty()) {
            QString latestSubDir = mainDir.filePath(subDirs.first());
            QString filePathToMonitor = QDir(latestSubDir).filePath("result/ID1");
            const int maxRetries = 10;
            const int retryDelayMs = 500;
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
                qCritical() << "Error: File" << filePathToMonitor << "still not found after" << maxRetries << "attempts. Aborting start().";
                return;
            }

            m_currentSubDir = filePathToMonitor;
            m_subWatcher->addPath(m_currentSubDir);
            qDebug() << "Watching newest sub-directory:" << m_currentSubDir;
            emit subDirChanged(m_currentSubDir);

            m_processedFiles.clear();
            // QDir subDir(m_currentSubDir);
            // QStringList tifFiles = subDir.entryList(QStringList("*.tif"), QDir::Files | QDir::NoDotAndDotDot);
            // for (const QString& fileName : tifFiles) {
            //     QString fullPath = subDir.filePath(fileName);
            //     m_processedFiles.insert(fullPath);
            //     qDebug() << "Found existing .tif file:" << fullPath;
            // }
        } else {
            qDebug() << "No sub-directories found in main folder.";
        }
    }
    emit mainDirChanged(m_mainFolderPath);
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
    if (m_isGMTIMonitoring) {
        // GMTI 模式：检查主文件夹中的新 .bin 文件
        QDir dir(path);
        QStringList currentFiles = dir.entryList(QStringList("*.bin"), QDir::Files | QDir::NoDotAndDotDot);

        for (const QString& fileName : currentFiles) {
            QString fullPath = dir.filePath(fileName);
            if (!m_processedBinFiles.contains(fullPath)) {
                qDebug() << "New .bin file detected:" << fullPath;
                emit newFileDetected(fullPath);
                m_processedBinFiles.insert(fullPath);
            }
        }
    } else {
        // SAR/ISAR 模式：检查是否有新子文件夹
        QDir dir(path);
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

        if (!subDirs.isEmpty()) {
            QString latestSubDir = dir.filePath(subDirs.first());
            QString filePathToMonitor = QDir(latestSubDir).filePath("result/ID1");
            const int maxRetries = 10;
            const int retryDelayMs = 500;
            bool fileExists = false;

            for (int i = 0; i < maxRetries; ++i) {
                if (QFileInfo::exists(filePathToMonitor)) {
                    fileExists = true;
                    break;
                }
                QThread::msleep(retryDelayMs);
            }

            if (!fileExists) {
                qCritical() << "Error: File" << filePathToMonitor << "still not found after" << maxRetries << "attempts. Aborting.";
                return;
            }

            if (m_currentSubDir != filePathToMonitor) {
                qDebug() << "Detected new newest sub-directory:" << filePathToMonitor;
                m_subWatcher->removePaths(m_subWatcher->directories());
                m_subWatcher->addPath(filePathToMonitor);
                m_currentSubDir = filePathToMonitor;
                m_processedFiles.clear();
                emit subDirChanged(filePathToMonitor);
            }
        }
    }
    emit mainDirChanged(path);
}

void FileMonitor::onSubdirectoryChanged(const QString& path) {
    if (m_isGMTIMonitoring) {
        return; // GMTI模式不处理子文件夹变化
    }
    QDir dir(path);
    QStringList currentFiles = dir.entryList(QStringList("*.tif"), QDir::Files | QDir::NoDotAndDotDot);

    for (const QString& fileName : currentFiles) {
        QString fullPath = dir.filePath(fileName);
        if (!m_processedFiles.contains(fullPath)) {
            qDebug() << "New .tif file detected:" << fullPath;
            emit newFileDetected(fullPath);
            m_processedFiles.insert(fullPath);
        }
    }
}
