#include "package_sar_data.h"
#include "AuxFileReader.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring> // For memcpy
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>

// 计算校验和的私有辅助函数
static uint8_t calculate_checksum(const uint8_t* data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += data[i];
    }
    return sum;
}

// 封装 SAR_DataInfo 的核心函数
SAR_DataInfo createSarDataInfo(const AuxHeader& auxHeader, uint32_t imageSize, uint16_t image_num) {
    SAR_DataInfo dataInfo;
    memset(&dataInfo, 0, sizeof(SAR_DataInfo));

    // 帧头 (0d): 固定值 0x55AA
    dataInfo.frame_header = 0x55AA;

    // 数据长度 (2d): 消息长度，从该字段开始到消息内容结束，包含帧头和校验和
    // SAR_DataInfo 的总大小是 170字节，所以数据长度 = 170 + 图像大小
    dataInfo.data_length = sizeof(SAR_DataInfo) + imageSize;

    // 消息地址字, 类型, 计数, 源/目的地址, 指令类型/计数 (6d - 17d):
    // 协议未明确说明如何获取，此处使用占位符
    dataInfo.message_addr = 0x0001;
    dataInfo.message_type = 0x0001;
    dataInfo.message_count = image_num;
    dataInfo.source_addr = 0;
    dataInfo.dest_addr = 0;
    dataInfo.cmd_type = 0;
    dataInfo.cmd_count = 0;

    // 图像行数 (18d): 来自 AuxHeader 的 pulse_num
    dataInfo.image_rows = static_cast<uint16_t>(auxHeader.pulse_num);
    // 图像列数 (20d): 来自 AuxHeader 的 pulse_len
    dataInfo.image_cols = static_cast<uint16_t>(auxHeader.pulse_len);

    // 图像可用标志 (22d): 协议指定可用为 FFFFH
    dataInfo.image_available_flag = 0xFFFF;

    // 惯导数据：从 double 转换为 Int16/Int32 并进行量化
    // 注意：你提供的 AuxFileReader 代码中没有具体的惯导数据向量，我将使用 m_header 中的参考值作为示例
    // 实际使用时，你可能需要传入相关的惯导数据数组并取其中位数或特定值

    // 滚动角 (24d): 来自 auxHeader.roll_ref，量化当量 5.49334e-3°
    dataInfo.roll_angle = static_cast<int16_t>(round(auxHeader.roll_ref / 5.49334e-3));
    // 航向角 (26d): 来自 auxHeader.yaw_ref，量化当量 5.49334e-3°
    dataInfo.heading_angle = static_cast<int16_t>(round(auxHeader.yaw_ref / 5.49334e-3));
    // 俯仰角 (28d): 来自 auxHeader.pitch_ref，量化当量 5.49334e-3°
    dataInfo.pitch_angle = static_cast<int16_t>(round(auxHeader.pitch_ref / 5.49334e-3));
    // 导航经度 (30d): 来自 auxHeader.lng_s，量化当量 8.38191e-8°
    dataInfo.nav_lng = static_cast<int32_t>(round(auxHeader.lng_s / 8.38191e-8));
    // 导航纬度 (34d): 来自 auxHeader.lat_s，量化当量 8.38191e-8°
    dataInfo.nav_lat = static_cast<int32_t>(round(auxHeader.lat_s / 8.38191e-8));
    // 导航高度 (38d): 来自 auxHeader.alt_path，量化当量 9.3133e-6°
    dataInfo.nav_alt = static_cast<int16_t>(round(auxHeader.alt_path / 9.3133e-6));

    // 速度 (40d - 44d): AuxFileReader 中没有，此处使用占位符 0
    dataInfo.north_vel = 0;
    dataInfo.up_vel = 0;
    dataInfo.east_vel = 0;

    // 时间 (46d - 49d): AuxFileReader 中没有，此处使用占位符 0
    dataInfo.img_time_h = 0;
    dataInfo.img_time_m = 0;
    dataInfo.img_time_s = 0;
    dataInfo.img_time_ms = 0;

    // 图像点经纬度、高度、斜距 (98d - 156d): AuxFileReader 中没有，此处使用占位符 0
    dataInfo.top_left_alt = 0;
    dataInfo.bottom_left_alt = 0;
    dataInfo.bottom_right_alt = 0;
    dataInfo.top_right_alt = 0;
    dataInfo.center_alt = 0;
    dataInfo.top_left_lng = static_cast<int32_t>(round(auxHeader.lng11 / 8.38191e-8));
    dataInfo.bottom_left_lng = static_cast<int32_t>(round(auxHeader.lngM1 / 8.38191e-8));
    dataInfo.bottom_right_lng = static_cast<int32_t>(round(auxHeader.lngMN / 8.38191e-8));
    dataInfo.top_right_lng = static_cast<int32_t>(round(auxHeader.lng1N / 8.38191e-8));
    dataInfo.center_lng = 0;
    dataInfo.top_left_lat = static_cast<int32_t>(round(auxHeader.lat11 / 8.38191e-8));
    dataInfo.bottom_left_lat = static_cast<int32_t>(round(auxHeader.latM1 / 8.38191e-8));
    dataInfo.bottom_right_lat = static_cast<int32_t>(round(auxHeader.latMN / 8.38191e-8));
    dataInfo.top_right_lat = static_cast<int32_t>(round(auxHeader.lat1N / 8.38191e-8));
    dataInfo.center_lat = 0;
    dataInfo.top_left_range = 0;
    dataInfo.bottom_left_range = 0;
    dataInfo.bottom_right_range = 0;
    dataInfo.top_right_range = 0;
    dataInfo.center_range = 0;

    // 像素间隙距离 (159d): 此处暂定距离像分辨率，即Rbin
    dataInfo.pixel_gap = static_cast<int8_t>(round(auxHeader.Rbin / 0.01));

    // SAR成像下视角 (160d): AuxFileReader 中没有，此处使用占位符 0
    dataInfo.depression_angle = 0;

    // SAR成像斜视角 (162d): AuxFileReader 中没有，此处使用占位符 0
    dataInfo.squint_angle = 0;

    // 侧视方向 (164d): 协议指定 0x00 为左侧视，此处假设为左侧视
    dataInfo.side_look_dir = 0x00;

    // 校验和字段先置零
    dataInfo.checksum = 0;

    // 计算并填充校验和
    // 校验和从第三字节开始（消息地址字）到校验和字段前一位
    // 即从地址 2d 到 168d，对应字节索引 2 到 168
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&dataInfo);
    dataInfo.checksum = calculate_checksum(ptr + sizeof(uint16_t), sizeof(SAR_DataInfo) - sizeof(uint16_t) - sizeof(uint8_t));

    return dataInfo;
}

// =================== 新增离线打包函数 ===================
bool createBinFileFromTifAndAux(const QString& tifFilePath, const QString& auxFilePath, const QString& outputBinFilePath, uint16_t image_num)
{
    // 1. 读取AUX文件
    AuxFileReader auxReader;
    if (!auxReader.read(auxFilePath)) {
        qWarning() << "Failed to read AUX file:" << auxFilePath;
        return false;
    }

    // 2. 使用QImageReader来安全地读取TIF文件并转换为JPG数据
    QImageReader reader(tifFilePath);
    if (!reader.canRead()) {
        qWarning() << "QImageReader cannot read file:" << tifFilePath;
        return false;
    }

    const int newMemoryLimitMB = 2048;
    qDebug() << "Setting QImageReader allocation limit to" << newMemoryLimitMB << "MB.";
    reader.setAllocationLimit(newMemoryLimitMB);

    QImage tifImage = reader.read();
    if (tifImage.isNull()) {
        qWarning() << "Failed to load TIF image into QImage. Potential reasons: file corrupted or still too large.";
        qWarning() << "Reader error:" << reader.errorString();
        return false;
    }

    // 将读取的QImage数据保存为JPG格式到QByteArray
    QByteArray jpgData;
    QBuffer buffer(&jpgData);
    buffer.open(QIODevice::WriteOnly);

    QImageWriter writer(&buffer, "JPG");
    writer.setQuality(80); // 设置JPG质量
    if (!writer.write(tifImage)) {
        qWarning() << "Failed to save QImage to JPG buffer.";
        return false;
    }

    // 3. 准备SAR_DataInfo
    SAR_DataInfo dataInfo = createSarDataInfo(auxReader.getHeader(), jpgData.size(), image_num);

    // 4. 将 SAR_DataInfo 和 JPG 数据组合成完整数据包
    QByteArray fullMessage;
    fullMessage.append(reinterpret_cast<const char*>(&dataInfo), sizeof(SAR_DataInfo));
    fullMessage.append(jpgData);

    // 5. 将完整数据包拆分为帧并写入bin文件
    QFile binFile(outputBinFilePath);
    if (!binFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open bin file for writing:" << outputBinFilePath;
        return false;
    }

    qint64 totalPackets = (fullMessage.size() + 4096 - 1) / 4096;
    qint64 currentOffset = 0;

    for (int i = 0; i < totalPackets; ++i) {
        SAR_Frame header;
        memset(&header, 0, sizeof(SAR_Frame));

        qint64 payloadSize = qMin(static_cast<qint64>(4096), fullMessage.size() - currentOffset);
        QByteArray payload = fullMessage.mid(currentOffset, payloadSize);

        header.fixed_value = 0x90E9;
        header.image_number =image_num;
        header.image_size = jpgData.size();
        header.current_packet = i + 1;
        header.total_packets = totalPackets;
        header.data_length = payloadSize;
        header.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(payload.constData()), payload.size());

        binFile.write(reinterpret_cast<const char*>(&header), sizeof(SAR_Frame));
        binFile.write(payload);

        currentOffset += payloadSize;
    }

    binFile.close();
    qDebug() << "Successfully created bin file at:" << outputBinFilePath;
    return true;
}

// =================== SarPacketizer 类的新实现 ===================
SarPacketizer::SarPacketizer(const QString& binFilePath)
{
    m_binFile.setFileName(binFilePath);
    if (!m_binFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open bin file for reading:" << binFilePath;
    }
}

SarPacketizer::~SarPacketizer()
{
    if (m_binFile.isOpen()) {
        m_binFile.close();
    }
}

// 检查是否还有下一个数据包
bool SarPacketizer::hasNextPacket()
{
    return m_binFile.isOpen() && !m_binFile.atEnd();
}

// 获取下一个数据包
QByteArray SarPacketizer::getNextPacket()
{
    if (!hasNextPacket()) {
        return QByteArray();
    }

    // 读取帧头以获取数据长度
    QByteArray headerData = m_binFile.read(sizeof(SAR_Frame));
    if (headerData.size() != sizeof(SAR_Frame)) {
        qWarning() << "Failed to read full frame header from bin file.";
        return QByteArray();
    }

    const SAR_Frame* header = reinterpret_cast<const SAR_Frame*>(headerData.constData());
    qint64 payloadSize = header->data_length;

    // 读取有效载荷
    QByteArray payloadData = m_binFile.read(payloadSize);
    if (payloadData.size() != payloadSize) {
        qWarning() << "Failed to read full payload from bin file.";
        return QByteArray();
    }

    // 组合成完整的包
    QByteArray fullPacket = headerData;
    fullPacket.append(payloadData);

    return fullPacket;
}

// 核心解包函数实现
bool unpackage_sar_data(const std::string& input_filename, const std::string& output_image_filename) {
    std::ifstream file(input_filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << input_filename << std::endl;
        return false;
    }

    std::vector<uint8_t> full_message;
    uint16_t total_packets = 0;

    // 逐个数据包读取并拼接
    for (;;) {
        SAR_Frame frame_header;
        file.read(reinterpret_cast<char*>(&frame_header), sizeof(SAR_Frame));

        if (file.eof()) {
            // 正常结束，文件已读取完毕
            break;
        }

        if (file.gcount() != sizeof(SAR_Frame)) {
            std::cerr << "Error: Failed to read frame header or file is corrupted." << std::endl;
            return false;
        }

        // 验证帧头固定值
        if (frame_header.fixed_value != 0x90E9) {
            std::cerr << "Error: Frame header fixed value mismatch, file may be corrupted." << std::endl;
            return false;
        }

        if (total_packets == 0) {
            total_packets = frame_header.total_packets;
            std::cout << "Total packets: " << total_packets << std::endl;
            std::cout << "Image size: " << frame_header.image_size << " bytes" << std::endl;
        }

        // 读取数据包
        std::vector<uint8_t> packet_data(frame_header.data_length);
        file.read(reinterpret_cast<char*>(packet_data.data()), frame_header.data_length);
        if (file.gcount() != frame_header.data_length) {
            std::cerr << "Error: Failed to read data packet." << std::endl;
            return false;
        }

        // 验证数据包校验和
        uint8_t calculated_checksum = calculate_checksum(packet_data.data(), packet_data.size());
        if (calculated_checksum != frame_header.checksum) {
            std::cerr << "Error: Checksum mismatch in packet " << frame_header.current_packet << "." << std::endl;
            return false;
        }

        // 将数据包内容附加到完整消息中
        full_message.insert(full_message.end(), packet_data.begin(), packet_data.end());

    std::cout << "Successfully unpacked packet " << frame_header.current_packet << " / " << frame_header.total_packets << ", data length: " << frame_header.data_length << " bytes" << std::endl;
    }

    // 验证完整消息的 SAR_DataInfo 校验和
    size_t data_info_fixed_size = sizeof(SAR_DataInfo);
    if (full_message.size() < data_info_fixed_size) {
        std::cerr << "Error: Full message length is insufficient, cannot parse SAR_DataInfo." << std::endl;
        return false;
    }
    uint8_t internal_checksum = calculate_checksum(full_message.data() + 2, data_info_fixed_size - 2 - sizeof(uint8_t));
    if (full_message[data_info_fixed_size - sizeof(uint8_t)] != internal_checksum) {
        std::cerr << "Error: SAR_DataInfo internal checksum mismatch." << std::endl;
        return false;
    }

    // 解析 SAR_DataInfo
    SAR_DataInfo data_info;
    memcpy(&data_info, full_message.data(), data_info_fixed_size);

    std::cout << "\n--- SAR_DataInfo Parsed Result ---" << std::endl;
    std::cout << "Frame header: 0x" << std::hex << data_info.frame_header << std::dec << std::endl;
    std::cout << "Message length: " << data_info.data_length << " bytes" << std::endl;
    std::cout << "Message address: " << data_info.message_addr << std::endl;
    std::cout << "Message type: " << data_info.message_type << std::endl;
    std::cout << "Message count: " << data_info.message_count << std::endl;
    std::cout << "Source address: " << data_info.source_addr << std::endl;
    std::cout << "Destination address: " << data_info.dest_addr << std::endl;
    std::cout << "Command type: " << static_cast<int>(data_info.cmd_type) << std::endl;
    std::cout << "Command count: " << static_cast<int>(data_info.cmd_count) << std::endl;
    std::cout << "Image rows: " << data_info.image_rows << std::endl;
    std::cout << "Image cols: " << data_info.image_cols << std::endl;
    std::cout << "Image available flag: 0x" << std::hex << data_info.image_available_flag << std::dec << std::endl;
    std::cout << "IMU - Roll angle: " << data_info.roll_angle << std::endl;
    std::cout << "IMU - Heading angle: " << data_info.heading_angle << std::endl;
    std::cout << "IMU - Pitch angle: " << data_info.pitch_angle << std::endl;
    std::cout << "Navigation longitude: " << data_info.nav_lng << std::endl;
    std::cout << "Navigation latitude: " << data_info.nav_lat << std::endl;
    std::cout << "Navigation altitude: " << data_info.nav_alt << std::endl;
    std::cout << "North velocity: " << data_info.north_vel << std::endl;
    std::cout << "Up velocity: " << data_info.up_vel << std::endl;
    std::cout << "East velocity: " << data_info.east_vel << std::endl;
    std::cout << "Imaging time: " << static_cast<int>(data_info.img_time_h) << ":" << static_cast<int>(data_info.img_time_m) << ":" << static_cast<int>(data_info.img_time_s) << "." << static_cast<int>(data_info.img_time_ms) << std::endl;
    std::cout << "Pixel gap: " << static_cast<int>(data_info.pixel_gap) << std::endl;
    std::cout << "Checksum: 0x" << std::hex << static_cast<int>(data_info.checksum) << std::dec << std::endl;

    // 分离图像数据
    std::vector<uint8_t> image_data(full_message.begin() + data_info_fixed_size, full_message.end());
    std::cout << "\nUnpacked image data size: " << image_data.size() << " bytes" << std::endl;

    // 将解包后的图像数据保存为.tif/.jpg文件
    std::ofstream image_file(output_image_filename, std::ios::binary);
    if (image_file) {
        image_file.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
        std::cout << "Image data saved to '" << output_image_filename << "'." << std::endl;
    } else {
        std::cerr << "Failed to save image to '" << output_image_filename << "'." << std::endl;
    }

    return true;
}
