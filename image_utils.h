#pragma once
#include <QString>

// 图像工具函数
bool convertTiffToJpg(const QString &inputPath, const QString &outputPath);
// 文件处理工具
bool waitForFileRelease(const QString &filePath, int maxRetries = 50, int waitMs = 200);
