/*
 * @Author: shysgsg 1054733568@qq.com
 * @Date: 2025-09-05 13:27:49
 * @LastEditors: shysgsg 1054733568@qq.com
 * @LastEditTime: 2025-09-05 23:26:02
 * @FilePath: \AeroLink\main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <QApplication>
#include <QImage>
#include "mainwindow.h"
#include "logmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LogManager::instance();
    MainWindow w;
    w.show();
    return a.exec();
}
