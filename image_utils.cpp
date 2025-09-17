#include "image_utils.h"
#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <QImageReader>
#include <QThread>
#include <QDebug>
#include <QMutex>

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

// 校正图像并保存为临时文件
QString correctAndSaveImage(const QString &tifFilePath, double xBin, double rBin) {
    QImage originalImage(tifFilePath);
    if (originalImage.isNull()) {
        qWarning() << "Failed to load TIF image for correction:" << tifFilePath;
        return QString();
    }

    if (qFuzzyCompare(xBin, rBin)) {
        qDebug() << "Xbin and Rbin are already equal. No correction needed.";
        return tifFilePath; // 无需校正，直接返回原文件路径
    }

    double scaleFactor = xBin / rBin;
    int newHeight = qRound(originalImage.height() * scaleFactor);

    qDebug() << "Original size:" << originalImage.width() << "x" << originalImage.height();
    qDebug() << "Correcting image with scale factor:" << scaleFactor;
    qDebug() << "New size:" << originalImage.width() << "x" << newHeight;

    QImage correctedImage = originalImage.scaled(
        originalImage.width(),
        newHeight,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
        );

    // 生成临时文件名，使用QDir::tempPath()保证跨平台兼容
    QFileInfo fileInfo(tifFilePath);
    QString correctedFilePath = QDir::tempPath() + QDir::separator() + fileInfo.baseName() + "_corrected.tif";

    if (correctedImage.save(correctedFilePath, "TIF")) {
        qDebug() << "Corrected image saved to:" << correctedFilePath;
        return correctedFilePath;
    } else {
        qWarning() << "Failed to save corrected image to temporary file.";
        return QString();
    }
}

