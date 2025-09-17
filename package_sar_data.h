#ifndef PACKAGE_SAR_DATA_H
#define PACKAGE_SAR_DATA_H

#pragma once

#include <string>
#include <cstdint>
#include <QByteArray>
#include "AuxFileReader.h"
#include <QFile>

// 确保结构体按照1字节对齐，以匹配协议的字节布局
#pragma pack(1)

// 协议 1.1 帧格式
struct SAR_Frame {
    uint16_t fixed_value;     // 0d, 固定值0x90E9
    uint8_t reserved1[6];     // 2d, 备用
    uint16_t image_number;    // 8d, 图像编号
    uint32_t image_size;      // 10d, 图像总字节数
    uint16_t current_packet;  // 14d, 当前包数
    uint16_t total_packets;   // 16d, 总包数
    uint16_t data_length;     // 18d, 数据信息字节数，固定4096
    uint8_t checksum;         // 20d, 校验和
};

// 协议 1.2 数据信息格式
struct SAR_DataInfo {
    uint16_t frame_header;      // 0d, 0x55AA
    uint32_t data_length;       // 2d, 消息长度
    uint16_t message_addr;      // 6d, 消息地址字
    uint16_t message_type;      // 8d, 消息类型
    uint16_t message_count;     // 10d, 消息计数
    uint16_t source_addr;       // 12d, 源地址
    uint16_t dest_addr;         // 14d, 目的地址
    uint8_t cmd_type;           // 16d, 指令类型
    uint8_t cmd_count;          // 17d, 指令计数
    uint16_t image_rows;        // 18d, 图像行数
    uint16_t image_cols;        // 20d, 图像列数
    uint16_t image_available_flag; // 22d, 图像可用标志

    // SAR成像中心时刻的惯导数据
    int16_t roll_angle;         // 24d, 滚动角
    int16_t heading_angle;      // 26d, 航向角
    int16_t pitch_angle;        // 28d, 俯仰角
    int32_t nav_lng;            // 30d, 导航经度
    int32_t nav_lat;            // 34d, 导航纬度
    int16_t nav_alt;            // 38d, 导航高度
    int16_t north_vel;          // 40d, 北向速度
    int16_t up_vel;             // 42d, 天向速度
    int16_t east_vel;           // 44d, 东向速度
    uint8_t img_time_h;         // 46d, 成像时间，时
    uint8_t img_time_m;         // 47d, 成像时间，分
    uint8_t img_time_s;         // 48d, 成像时间，秒
    uint8_t img_time_ms;        // 49d, 成像时间，毫秒
    uint8_t reserved2[48];      // 50d, 备用

    // SAR成像时的参数
    int16_t top_left_alt;       // 98d, 图像左上点相对高度
    int16_t bottom_left_alt;    // 100d, 图像左下点相对高度
    int16_t bottom_right_alt;   // 102d, 图像右下点相对高度
    int16_t top_right_alt;      // 104d, 图像右上点相对高度
    int16_t center_alt;         // 106d, 图像中心点相对高度
    int32_t top_left_lng;       // 108d, 图像左上点经度
    int32_t bottom_left_lng;    // 112d, 图像左下点经度
    int32_t bottom_right_lng;   // 116d, 图像右下点经度
    int32_t top_right_lng;      // 120d, 图像右上点经度
    int32_t center_lng;         // 124d, 图像中心点经度
    int32_t top_left_lat;       // 128d, 图像左上点纬度
    int32_t bottom_left_lat;    // 132d, 图像左下点纬度
    int32_t bottom_right_lat;   // 136d, 图像右下点纬度
    int32_t top_right_lat;      // 140d, 图像右上点纬度
    int32_t center_lat;         // 144d, 图像中心点纬度
    uint16_t top_left_range;    // 148d, 图像左上点斜距
    uint16_t bottom_left_range; // 150d, 图像左下点斜距
    uint16_t bottom_right_range;// 152d, 图像右下点斜距
    uint16_t top_right_range;   // 154d, 图像右上点斜距
    uint16_t center_range;      // 156d, 图像中心点斜距
    uint8_t reserved3;          // 158d, 备用
    uint8_t pixel_gap;          // 159d, 像素间隙距离
    uint16_t depression_angle;  // 160d, SAR成像下视角
    uint16_t squint_angle;      // 162d, SAR成像斜视角
    uint8_t side_look_dir;      // 164d, 侧视方向
    uint8_t reserved4[4];       // 165d, 备用
    uint8_t checksum;           // 169d, 校验和
};

// 2.1 GMTI信息包
struct GmtInfo {
    uint8_t   target_count;    // 目标数目
    uint8_t   targets[680];    // 目标信息（17字节/个，最多40个）
    uint32_t  imu_time;        // 惯导周秒
    uint16_t  image_number;    // 底图的图像编号
    uint8_t   reserved[68];    // 备用
};

// 2.2 目标信息
struct TargetInfo {
    uint8_t   target_id;        // 目标编号
    int32_t   lng;              // 目标经度
    int32_t   lat;              // 目标纬度
    int16_t   pixel_offset_x;   // 目标像素偏移x
    int16_t   pixel_offset_y;   // 目标像素偏移y
    int16_t   radial_velocity;  // 目标径向速度
    int16_t   heading_direction;// 目标前进方向
};

#pragma pack()

// 计算校验和的私有辅助函数
uint8_t calculate_checksum(const uint8_t* data, size_t length);

// 封装 SAR_DataInfo 的核心函数
SAR_DataInfo createSarDataInfo(const AuxHeader& auxHeader, uint32_t imageSize);

/**
 * @class SarPacketizer
 * @brief 负责从预先生成的bin文件中读取数据包。
 */
class SarPacketizer {
public:
    SarPacketizer(const QString& binFilePath);
    ~SarPacketizer();

    bool hasNextPacket();
    QByteArray getNextPacket();

private:
    QFile m_binFile;
};

/**
 * @brief 离线打包函数：将tif和aux文件内容打包成一个bin文件。
 * @param tifFilePath TIF文件路径
 * @param auxFilePath AUX文件路径
 * @param outputBinFilePath 输出bin文件路径
 * @return 成功返回true，失败返回false
 */
bool createBinFileFromTifAndAux(const QString& tifFilePath, const QString& auxFilePath, const QString& outputBinFilePath, uint16_t image_num);
bool createBinFileFromTifOnly(const QString& tifFilePath, const QString& outputBinFilePath, uint16_t image_num);
bool unpackage_sar_data(const std::string& input_filename, const std::string& output_image_filename);

#endif // PACKAGE_SAR_DATA_H
