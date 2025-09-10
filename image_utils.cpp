#include "image_utils.h"
#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <QImageReader>
#include <QThread>
#include <QDebug>

bool convertTiffToJpg(const QString &inputPath, const QString &outputPath)
{
    QFileInfo fileInfo(inputPath);
    if (!fileInfo.exists()) {
        qDebug() << "Source file does not exist:" << inputPath;
        return false;
    }
    QImage image;
    if (!image.load(inputPath)) {
        qDebug() << "Failed to load image:" << inputPath;
        return false;
    }
    QDir destinationDir(QFileInfo(outputPath).absolutePath());
    if (!destinationDir.exists()) {
        if (!destinationDir.mkpath(".")) {
            qDebug() << "Failed to create destination dir:" << destinationDir.absolutePath();
            return false;
        }
    }
    if (!image.save(outputPath, "JPG", 80)) {
        qDebug() << "Failed to save JPG file:" << outputPath;
        return false;
    }
    qDebug() << "Convert success:" << inputPath << "->" << outputPath;
    return true;
}

bool waitForFileRelease(const QString &filePath, int maxRetries, int waitMs)
{
    QFile file(filePath);
    int retries = 0;
    while (!file.open(QIODevice::ReadOnly) && retries < maxRetries) {
        file.close();
        QThread::msleep(waitMs);
        retries++;
    }
    file.close();
    return retries < maxRetries;
}
