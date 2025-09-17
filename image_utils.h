#pragma once
#include <QString>

// 图像工具函数
bool convertTiffToJpg(const QString &inputPath, const QString &outputPath);
// 文件处理工具
bool waitForFileRelease(const QString &filePath, int maxRetries = 50, int waitMs = 200);
bool getBinValuesFromAux(const QString &auxFilePath, double &xBin, double &rBin);
QString correctAndSaveImage(const QString &tifFilePath, double xBin, double rBin);
