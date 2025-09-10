#ifndef AUXFILEREADER_H
#define AUXFILEREADER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstdint> // 用于 qint64 类型
#include <cmath>   // 用于 M_PI
#include <QString>
#include <QtGlobal>

// 如果编译器没有定义 M_PI，则定义一个常数
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct AuxHeader {
    // 雷达参数
    qint64 op_mode;    // 工作模式（0:单通道, 1:全极化, 2:单极化双天线干涉, 3:单极化多天线干涉, 4:全极化双天线干涉）
    qint64 pp_mode;    // 乒乓模式（0:标准模式, 1:乒乓模式）
    qint64 Kr_sign;    // 调频率符号（0:负, 1:正）
    double fc;          // 载频（Hz）
    double fd;          // 中频（Hz），FMCW 无此参数
    double Br;          // 带宽（Hz）
    double Fsr;         // 降采样之后的采样率（Hz），FMCW 无此参数
    double Tr;          // 脉宽（s），对于 FMCW 是扫频周期
    double theta_bw;    // 方位波束宽度（deg）
    double Ba;          // 多普勒带宽（Hz）
    double PRF;         // 降采样之后的脉冲重复频率（Hz）

    // 图像参数
    qint64 pulse_num;  // 行数（图像高度，方位点数）
    qint64 pulse_len;  // 列数（图像宽度，距离点数）
    qint64 amp_bit;    // 幅度图像数据位数（bit）
    double Xbin;        // 行间间距（方位向像素宽度）（m）
    double Rbin;        // 列间间距（距离向像素宽度）（m）

    // 几何参数
    qint64 geo_mode;   // 成像几何模式（0:偏移固定 fdc, 1:偏移变化 fdc, 2:正侧视）
    qint64 look_mode;  // 侧视模式（0:右侧视, 1:左侧视）
    qint64 flag_flat;  // 是否去平地（0:未去平地, 1:已去平地）
    double fdc_ref;     // 处理多普勒中心（Hz）
    double fdc0;        // 多普勒常数项（Hz）
    double fdc1;        // 多普勒一次项系数（Hz/m）
    double fdc2;        // 多普勒二次项系数（Hz/m^2）
    double v;           // 参考飞行速度（m/s）
    double Rmin;        // 最短斜距（m）
    double Rref;        // 参考斜距（m）
    double alt_scene;   // 参考地面海拔（m）
    double alt_path;    // 参考航迹海拔（m）
    double yaw_ref;     // 偏航参考值（rad）
    double pitch_ref;   // 俯仰参考值（rad）
    double roll_ref;    // 横滚参考值（rad）

    // 结构参数
    double yaw0;        // 偏航初始安装值（rad）
    double pitch0;      // 俯仰初始安装值（rad）
    double roll0;       // 横滚初始安装值（rad）
    double X_APC;       // APC 初始前向坐标（m），地面测量所得
    double Y_APC;       // APC 初始右向坐标（m），地面测量所得
    double Z_APC;       // APC 初始地向坐标（m），地面测量所得
    double X_APC_ref;   // APC 参考前向坐标（m），考虑姿态变化后
    double Y_APC_ref;   // APC 参考右向坐标（m），考虑姿态变化后
    double Z_APC_ref;   // APC 参考地向坐标（m），考虑姿态变化后

    // 地理参数
    double lng_Gauss;   // 高斯投影参考经度（deg），3 度带
    double heading_ref; // 参考飞行航向（deg）
    double lat_s;       // 参考航迹起点纬度（deg）
    double lng_s;       // 参考航迹起点经度（deg）
    double lat_e;       // 参考航迹终点纬度（deg）
    double lng_e;       // 参考航迹终点经度（deg）
    double lat11;       // 1 行 1 列纬度（deg）
    double lng11;       // 1 行 1 列经度（deg）
    double lat1N;       // 1 行 N 列纬度（deg）
    double lng1N;       // 1 行 N 列经度（deg）
    double latM1;       // M 行 1 列纬度（deg）
    double lngM1;       // M 行 1 列经度（deg）
    double latMN;       // M 行 N 列纬度（deg）
    double lngMN;       // M 行 N 列经度（deg）
    double IMG_TH;      // 未在文档中说明，可能为预留字段
    qint64 az_MLK_num; // 未在文档中说明，可能为预留字段
};

class AuxFileReader {
public:
    // 构造函数
    AuxFileReader();

    // 核心函数：读取并解析 AUX 文件，返回读取是否成功
    bool read(const QString& filename);

    // 获取读取到的头信息
    AuxHeader getHeader() const;

    // 获取运动数据
    const std::vector<double>& getTaRef() const;
    const std::vector<double>& getXRef() const;
    const std::vector<double>& getYRef() const;
    const std::vector<double>& getZRef() const;
    const std::vector<double>& getLatRef() const;
    const std::vector<double>& getLngRef() const;
    const std::vector<double>& getAltRef() const;

    const std::vector<double>& getTa() const;
    const std::vector<double>& getX() const;
    const std::vector<double>& getY() const;
    const std::vector<double>& getZ() const;
    const std::vector<double>& getXImu() const;
    const std::vector<double>& getYImu() const;
    const std::vector<double>& getZImu() const;
    const std::vector<double>& getYaw() const;
    const std::vector<double>& getPitch() const;
    const std::vector<double>& getRoll() const;

    // 打印所有头信息
    void printHeader() const;

    // 打印所有运动数据
    void printData() const;

private:
    // 模板辅助函数：从文件中读取单个值
    template<typename T>
    T readValue(std::ifstream& file);

    // 辅助函数：打印向量的前 N 个元素
    void printVector(const std::string& name, const std::vector<double>& vec, size_t count = 5) const;

    // 私有数据成员，存储所有读取到的数据
    AuxHeader m_header;
    std::vector<double> m_ta_ref;
    std::vector<double> m_x_ref;
    std::vector<double> m_y_ref;
    std::vector<double> m_z_ref;
    std::vector<double> m_lat_ref;
    std::vector<double> m_lng_ref;
    std::vector<double> m_alt_ref;

    std::vector<double> m_ta;
    std::vector<double> m_x;
    std::vector<double> m_y;
    std::vector<double> m_z;
    std::vector<double> m_x_imu;
    std::vector<double> m_y_imu;
    std::vector<double> m_z_imu;
    std::vector<double> m_yaw;
    std::vector<double> m_pitch;
    std::vector<double> m_roll;
};

#endif // AUXFILEREADER_H
