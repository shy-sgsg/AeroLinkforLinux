#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

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
    void start(bool isGMTIMonitoring); // ✅ 修改: 增加参数以选择监控模式
    void stop();
    QString getCurrentSubDir() const;

signals:
    void newFileDetected(const QString& filePath);
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
    QSet<QString> m_processedBinFiles; // ✅ 新增：用于跟踪GMTI模式下已处理的bin文件
    bool m_isGMTIMonitoring = false; // ✅ 新增：用于区分监控模式
};

#endif // FILE_MONITOR_H
