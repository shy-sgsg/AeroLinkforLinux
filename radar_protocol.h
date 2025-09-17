#ifndef RADAR_PROTOCOL_H
#define RADAR_PROTOCOL_H

#include <QtGlobal>
#include <QByteArray>

// 宏定义以确保不同编译器下的字节对齐
#ifdef _MSC_VER
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

// 数据帧头结构体，总大小21字节
struct DataHeader {
    quint16 fixed_value;    // 固定值0x9EE9 (2 bytes)
    quint8  reserved[16];   // 备用，共16字节
    quint16 data_length;    // 数据信息的字节数 (2 bytes)
    quint8  checksum;       // 校验和 (1 byte)
};

// 数据信息结构体，总大小20字节
struct DataInfo {
    quint16 frame_header;      // 帧头0x55AA (2 bytes)
    quint32 data_length;       // 消息长度 (4 bytes)
    quint16 message_count;     // 消息计数 (2 bytes)
    quint16 source_address;    // 源地址 (2 bytes)
    quint16 destination_address; // 目的地址 (2 bytes)
    quint8  command_type;      // 指令类型 (1 byte)
    quint8  reserved;          // 备用 (1 byte)
    quint16 image_number;      // 图像编号 (2 bytes)
    qint16  pixel_offset_x;    // 像素偏移x (2 bytes)
    qint16  pixel_offset_y;    // 像素偏移y (2 bytes)
    qint16  total_y;           // 图像总列数 (2 bytes)
};

// 恢复默认的内存对齐方式
#ifdef _MSC_VER
#pragma pack(pop)
#else
#pragma pack()
#endif

// 计算校验和函数
inline quint8 calculateChecksum(const QByteArray& data) {
    quint8 sum = 0;
    for (char byte : data) {
        sum += static_cast<quint8>(byte);
    }
    return sum;
}

#endif // RADAR_PROTOCOL_H
