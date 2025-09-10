#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QDir>
#include <QSet>

class FileMonitor : public QObject {
    Q_OBJECT
public:
    explicit FileMonitor(QObject* parent = nullptr);
    void setMainFolder(const QString& folderPath);
    void start();
    void stop();
    QString getCurrentSubDir() const;

signals:
    void newTifFileDetected(const QString& tifPath);
    void mainDirChanged(const QString& mainDir);
    void subDirChanged(const QString& subDir);

private slots:
    void onMainDirectoryChanged(const QString& path);
    void onSubdirectoryChanged(const QString& path);

private:
    QFileSystemWatcher* m_mainWatcher;
    QFileSystemWatcher* m_subWatcher;
    QString m_mainFolderPath;
    QString m_currentSubDir;
    QSet<QString> m_processedFiles;
};
