#include "logmanager.h"
#include <QDebug>
#include <QMutex>
#include <QDateTime> // 新增：包含 QDateTime 头文件

// 原始消息处理函数指针
static QtMessageHandler originalMessageHandler = nullptr;

LogManager& LogManager::instance()
{
    static LogManager logManager;
    return logManager;
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 使用线程本地变量来保护，避免同一个线程内的递归调用
    static thread_local bool isInsideHandler = false;
    if (isInsideHandler) {
        if (originalMessageHandler) {
            originalMessageHandler(type, context, msg);
        }
        return;
    }

    isInsideHandler = true;

    // --- 新增：获取当前时间并格式化为字符串 ---
    QString formattedTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    // --- 新增：拼接时间戳和原始消息 ---
    QString formattedMessage = QString("[%1] %2").arg(formattedTime).arg(msg);

    // 将带时间戳的消息转发给主窗口
    LogManager::instance().logMessage(formattedMessage);

    // 调用原始的消息处理函数，将消息打印到终端
    if (originalMessageHandler) {
        originalMessageHandler(type, context, msg);
    }

    isInsideHandler = false;
}

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
    // 保存原始的消息处理函数，然后安装我们自己的
    originalMessageHandler = qInstallMessageHandler(messageHandler);
}
