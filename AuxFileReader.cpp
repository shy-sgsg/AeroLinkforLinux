#include "AuxFileReader.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <QFile>
#include <QDataStream>
#include <QString>
#include <QDebug>
#include <vector>

// 构造函数
AuxFileReader::AuxFileReader() {
    // 可以在这里初始化数据成员
}

// 模板辅助函数实现
template<typename T>
T AuxFileReader::readValue(std::ifstream& file) {
    T value;
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

// 打印所有头信息
void AuxFileReader::printHeader() const {
    std::cout << "--- AUX File Header ---" << std::endl;
    // Radar parameters
    std::cout << "op_mode     = " << m_header.op_mode << std::endl;
    std::cout << "pp_mode     = " << m_header.pp_mode << std::endl;
    std::cout << "Kr_sign     = " << m_header.Kr_sign << std::endl;
    std::cout << "fc          = " << m_header.fc * 1e-9 << " GHz" << std::endl;
    std::cout << "fd          = " << m_header.fd * 1e-9 << " GHz" << std::endl;
    std::cout << "Br          = " << m_header.Br * 1e-6 << " MHz" << std::endl;
    std::cout << "Fsr         = " << m_header.Fsr * 1e-6 << " MHz" << std::endl;
    std::cout << "Tr          = " << m_header.Tr * 1e6 << " us" << std::endl;
    std::cout << "theta_bw    = " << m_header.theta_bw / M_PI * 180 << " deg" << std::endl;
    std::cout << "Ba          = " << m_header.Ba << " Hz" << std::endl;
    std::cout << "PRF         = " << m_header.PRF << " Hz" << std::endl;
    std::cout << "-----------------------" << std::endl;
    // Image parameters
    std::cout << "pulse_num   = " << m_header.pulse_num << std::endl;
    std::cout << "pulse_len   = " << m_header.pulse_len << std::endl;
    std::cout << "amp_bit     = " << m_header.amp_bit << std::endl;
    std::cout << "Xbin        = " << m_header.Xbin << " m" << std::endl;
    std::cout << "Rbin        = " << m_header.Rbin << " m" << std::endl;
    std::cout << "Xbin/Rbin   = " << m_header.Xbin / m_header.Rbin << std::endl;
    std::cout << "-----------------------" << std::endl;
    // Geometric parameters
    std::cout << "geo_mode    = " << m_header.geo_mode << std::endl;
    std::cout << "look_mode   = " << m_header.look_mode << std::endl;
    std::cout << "flag_flat   = " << m_header.flag_flat << std::endl;
    std::cout << "fdc_ref     = " << m_header.fdc_ref << " Hz" << std::endl;
    std::cout << "fdc0        = " << m_header.fdc0 << " Hz" << std::endl;
    std::cout << "fdc1        = " << m_header.fdc1 << " Hz/m" << std::endl;
    std::cout << "fdc2        = " << m_header.fdc2 << " Hz/m^2" << std::endl;
    std::cout << "v           = " << m_header.v << " m/s" << std::endl;
    std::cout << "Rmin        = " << m_header.Rmin << " m" << std::endl;
    std::cout << "Rref        = " << m_header.Rref << " m" << std::endl;
    std::cout << "alt_scene   = " << m_header.alt_scene << " m" << std::endl;
    std::cout << "alt_path    = " << m_header.alt_path << " m" << std::endl;
    std::cout << "yaw_ref     = " << m_header.yaw_ref << " deg" << std::endl;
    std::cout << "pitch_ref   = " << m_header.pitch_ref << " deg" << std::endl;
    std::cout << "roll_ref    = " << m_header.roll_ref << " deg" << std::endl;
    std::cout << "-----------------------" << std::endl;
    // Structural parameters
    std::cout << "yaw0        = " << m_header.yaw0 / M_PI * 180 << " deg" << std::endl;
    std::cout << "pitch0      = " << m_header.pitch0 / M_PI * 180 << " deg" << std::endl;
    std::cout << "roll0       = " << m_header.roll0 / M_PI * 180 << " deg" << std::endl;
    std::cout << "X_APC       = " << m_header.X_APC << " m" << std::endl;
    std::cout << "Y_APC       = " << m_header.Y_APC << " m" << std::endl;
    std::cout << "Z_APC       = " << m_header.Z_APC << " m" << std::endl;
    std::cout << "X_APC_ref   = " << m_header.X_APC_ref << " m" << std::endl;
    std::cout << "Y_APC_ref   = " << m_header.Y_APC_ref << " m" << std::endl;
    std::cout << "Z_APC_ref   = " << m_header.Z_APC_ref << " m" << std::endl;
    std::cout << "-----------------------" << std::endl;
    // Geographical parameters
    std::cout << "lng_Gauss   = " << m_header.lng_Gauss << " deg" << std::endl;
    std::cout << "heading_ref = " << m_header.heading_ref / M_PI * 180 << " deg" << std::endl;
    std::cout << "lat_s       = " << m_header.lat_s << " deg" << std::endl;
    std::cout << "lng_s       = " << m_header.lng_s << " deg" << std::endl;
    std::cout << "lat_e       = " << m_header.lat_e << " deg" << std::endl;
    std::cout << "lng_e       = " << m_header.lng_e << " deg" << std::endl;
    std::cout << "lat11       = " << m_header.lat11 << " deg" << std::endl;
    std::cout << "lng11       = " << m_header.lng11 << " deg" << std::endl;
    std::cout << "lat1N       = " << m_header.lat1N << " deg" << std::endl;
    std::cout << "lng1N       = " << m_header.lng1N << " deg" << std::endl;
    std::cout << "latM1       = " << m_header.latM1 << " deg" << std::endl;
    std::cout << "lngM1       = " << m_header.lngM1 << " deg" << std::endl;
    std::cout << "latMN       = " << m_header.latMN << " deg" << std::endl;
    std::cout << "lngMN       = " << m_header.lngMN << " deg" << std::endl;
    std::cout << "IMG_TH      = " << m_header.IMG_TH << std::endl;
    std::cout << "az_MLK_num  = " << m_header.az_MLK_num << std::endl;
    std::cout << "-----------------------" << std::endl;
}

// 打印所有运动数据
void AuxFileReader::printData() const {
    std::cout << "\n--- First Few Elements of Data Arrays ---" << std::endl;
    printVector("ta_ref", m_ta_ref);
    printVector("x_ref", m_x_ref);
    printVector("y_ref", m_y_ref);
    printVector("z_ref", m_z_ref);
    printVector("lat_ref", m_lat_ref);
    printVector("lng_ref", m_lng_ref);
    printVector("alt_ref", m_alt_ref);
    std::cout << "-----------------------" << std::endl;
    printVector("ta", m_ta);
    printVector("x", m_x);
    printVector("y", m_y);
    printVector("z", m_z);
    printVector("x_imu", m_x_imu);
    printVector("y_imu", m_y_imu);
    printVector("z_imu", m_z_imu);
    printVector("yaw", m_yaw);
    printVector("pitch", m_pitch);
    printVector("roll", m_roll);
    std::cout << "-----------------------" << std::endl;
}

// 修改 printVector 辅助函数
void AuxFileReader::printVector(const std::string& name, const std::vector<double>& vec, size_t count) const {
    std::cout << name << " (first " << count << " elements): ";
    for (size_t i = 0; i < std::min(count, vec.size()); ++i) {
        std::cout << std::fixed << std::setprecision(6) << vec[i] << " ";
    }
    if (vec.size() > count) {
        std::cout << "...";
    }
    std::cout << std::endl;
}

bool AuxFileReader::read(const QString& filename) {
    // 1. 打开文件
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Error: Could not open file" << file.errorString();
        return false;
    }

    // 2. 创建 QDataStream 并设置字节序
    QDataStream in(&file);
    // 这里假设数据是 little-endian（小端序），因为大多数现代处理器都是如此。
    // 如果数据源是其他字节序，您需要修改这里。
    in.setByteOrder(QDataStream::LittleEndian);

    // 3. 读取所有头文件参数到 m_header 结构体中
    // 使用 QDataStream 的 >> 运算符来读取数据
    in >> m_header.op_mode >> m_header.pp_mode >> m_header.Kr_sign;
    in >> m_header.fc >> m_header.fd >> m_header.Br >> m_header.Fsr >> m_header.Tr;
    in >> m_header.theta_bw >> m_header.Ba >> m_header.PRF >> m_header.pulse_num;
    in >> m_header.pulse_len >> m_header.amp_bit >> m_header.Xbin >> m_header.Rbin;
    in >> m_header.geo_mode >> m_header.look_mode >> m_header.flag_flat;
    in >> m_header.fdc_ref >> m_header.fdc0 >> m_header.fdc1 >> m_header.fdc2;
    in >> m_header.v >> m_header.Rmin >> m_header.Rref >> m_header.alt_scene;
    in >> m_header.alt_path >> m_header.yaw_ref >> m_header.pitch_ref >> m_header.roll_ref;
    in >> m_header.yaw0 >> m_header.pitch0 >> m_header.roll0 >> m_header.X_APC;
    in >> m_header.Y_APC >> m_header.Z_APC >> m_header.X_APC_ref >> m_header.Y_APC_ref;
    in >> m_header.Z_APC_ref >> m_header.lng_Gauss >> m_header.heading_ref;
    in >> m_header.lat_s >> m_header.lng_s >> m_header.lat_e >> m_header.lng_e;
    in >> m_header.lat11 >> m_header.lng11 >> m_header.lat1N >> m_header.lng1N;
    in >> m_header.latM1 >> m_header.lngM1 >> m_header.latMN >> m_header.lngMN;
    in >> m_header.IMG_TH >> m_header.az_MLK_num;

    // 4. 读取所有剩余数据
    qint64 remaining_size = file.size() - file.pos();
    if (remaining_size <= 0) {
        qDebug() << "Error: No data found after header.";
        file.close();
        return false;
    }

    QByteArray dataBlock = file.readAll();
    file.close();

    // 5. 将 QByteArray 转换为 std::vector<double>
    qint64 total_doubles = remaining_size / sizeof(double);
    std::vector<double> data_aux(total_doubles);

    // 检查数据块大小是否正确
    if (dataBlock.size() != remaining_size) {
        qDebug() << "Error: Read data block size mismatch.";
        return false;
    }

    // 使用 memcpy 将数据从 QByteArray 复制到 vector
    memcpy(data_aux.data(), dataBlock.data(), remaining_size);

    // 6. 根据 MATLAB 逻辑分区数据
    int64_t pulse_num_matlab = m_header.pulse_num;
    int64_t num_ta_ref = pulse_num_matlab * 7;
    int64_t num_ta = (total_doubles - num_ta_ref) / 10;

    if (num_ta <= 0) {
        qDebug() << "Error: The calculated length of the main motion data array is invalid.";
        return false;
    }

    // 7. 填充数据向量
    try {
        m_ta_ref.assign(data_aux.begin(), data_aux.begin() + pulse_num_matlab);
        m_x_ref.assign(data_aux.begin() + pulse_num_matlab, data_aux.begin() + pulse_num_matlab * 2);
        // ... (其他向量的填充代码不变) ...
        m_roll.assign(data_aux.begin() + num_ta_ref + num_ta * 9, data_aux.end());
    } catch (const std::out_of_range& e) {
        qDebug() << "Error: Data parsing failed. The file structure might be incorrect.";
        return false;
    }

    return true;
}

// 获取数据向量的函数实现
AuxHeader AuxFileReader::getHeader() const { return m_header; }
const std::vector<double>& AuxFileReader::getTaRef() const { return m_ta_ref; }
const std::vector<double>& AuxFileReader::getXRef() const { return m_x_ref; }
const std::vector<double>& AuxFileReader::getYRef() const { return m_y_ref; }
const std::vector<double>& AuxFileReader::getZRef() const { return m_z_ref; }
const std::vector<double>& AuxFileReader::getLatRef() const { return m_lat_ref; }
const std::vector<double>& AuxFileReader::getLngRef() const { return m_lng_ref; }
const std::vector<double>& AuxFileReader::getAltRef() const { return m_alt_ref; }
const std::vector<double>& AuxFileReader::getTa() const { return m_ta; }
const std::vector<double>& AuxFileReader::getX() const { return m_x; }
const std::vector<double>& AuxFileReader::getY() const { return m_y; }
const std::vector<double>& AuxFileReader::getZ() const { return m_z; }
const std::vector<double>& AuxFileReader::getXImu() const { return m_x_imu; }
const std::vector<double>& AuxFileReader::getYImu() const { return m_y_imu; }
const std::vector<double>& AuxFileReader::getZImu() const { return m_z_imu; }
const std::vector<double>& AuxFileReader::getYaw() const { return m_yaw; }
const std::vector<double>& AuxFileReader::getPitch() const { return m_pitch; }
const std::vector<double>& AuxFileReader::getRoll() const { return m_roll; }
