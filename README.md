# 犀牛派 X1 · 全域火情隐患智能监测巡检机器人

基于 **阿加犀犀牛派 X1（QCS8550）** 的 ROS 2 巡检机器人：激光 SLAM 建图与定位、Nav2 自主导航、多点位自动巡检、本地视觉大模型火情判定、网页端遥控与地图管理，配套自动灭火机构。

> 本仓库是案例的完整源码与文档。硬件接线、参数标定、复现步骤见 [`docs/`](docs/)。

---

## 1. 项目亮点

| 能力 | 实现 |
|---|---|
| 建图 | RPLidar A1 + 激光/陀螺里程计 + slam_toolbox，网页一键启停 |
| 定位 | AMCL 在保存地图中定位，支持初始位姿拖拽设置 |
| 导航 | Nav2（Smac Hybrid-A* + RPP + 速度平滑），带监督试航限速包络 |
| 巡检 | 多点位路线：到点 → 停稳确认 → 取图 → VLM 检测 → 生成报告 |
| 火情检测 | 本地 VLM（Qwen2.5-VL 3B QNN）判定火焰/烟雾，证据留档 |
| 遥控 | 网页双摇杆 / 单摇杆 / 键盘，带驾驶权仲裁与断线停车 |
| 云台 | 二维云台串口协议，网页摇杆控制 |
| 地图管理 | 地图保存/重命名/缩略图/正式图、语义区域划分、地图清理 |
| 安全 | 电机 armed 门控、deadman 断链停车、急停、传感器超时保护 |

---

## 2. 系统架构

```
                 ┌────────────── 网页 (Flask + SocketIO :5800) ──────────────┐
                 │  遥控 / 建图 / 导航 / 巡检 / 地图编辑 / 报告             │
                 └───────┬───────────────┬───────────────┬───────────────────┘
                         │ UDP 5813      │ UDP 5816      │ UDP 5814
                         ▼               ▼               ▼
                    pwm_proc        servo_proc      gimbal_proc
                  (200Hz 软PWM)    (50Hz 舵机)      (串口云台)
                         │               │               │
                         ▼               ▼               ▼
                    L298N 双通道      转向舵机       二维云台 (UART)

  RPLidar A1 ──/scan──┐
  GY-521 ──/imu/data_raw──┤── laser_imu_odom ──/odom + TF──> slam_toolbox ──/map
                          │                                        │
                          └────────────> AMCL + Nav2 <──────────────┘
                                              │
                                        cmd_vel → 监督包络 → 电机
```

**关键设计**

- 电机与舵机各自独立进程：软 PWM 是 1kHz 纯 Python 忙等线程，放在 ROS 节点里会和 rclpy 抢 GIL，实测控制回调 p99 达 466ms。
- 所有运动输出都要经过 `armed` 门控与 deadman；网页断开 300ms 即停车。
- Nav2 的速度输出不直接给电机，而是经过 `navigation_drive.py` 的**监督试航包络**（限速、限转向、限时长/距离、净空检查）。
- 巡检到点后必须通过"停稳确认"（里程计新鲜、速度足够小、漂移受限）才取图，避免运动中糊片。

---

## 3. 目录结构

| 目录 | 作用 |
|---|---|
| [`motor/`](motor/) | 电机驱动链路：pwm_proc（直连 PWM）、舵机、参数、接线、注入与测试工具 |
| [`slam/`](slam/) | 激光/陀螺里程计、MPU6050 驱动、建图管线、地图保存、建图控制脚本 |
| [`navigation/`](navigation/) | 地图存储、Nav2 启动、监督试航、巡检执行器、路线预览、单元测试 |
| [`inspection/`](inspection/) | 巡检记录与报告生成（本地 VLM 调用、证据文件管理） |
| [`web/`](web/) | 网页前端与后端：slam_viz、遥控/建图/导航/巡检/地图编辑页面 |
| [`gimbal/`](gimbal/) | 二维云台串口协议、进程、注入器与离线测试 |
| [`camera/`](camera/) | 相机持有者 fire_detect、取流与检测 |
| [`ai/`](ai/) | VLM 请求与诊断脚本（长提示词、相机帧、服务健康） |
| [`stage1/`](stage1/) | 阶段一验收：ROS 速度反馈、试航记录与物理测量对照 |
| [`stage2/`](stage2/) | 阶段二验收数据（预检、地图检查、参数快照） |
| [`tools/`](tools/) | 诊断与测试工具：编码器、GPIO、话题、旋转测试、方向探针 |
| [`data/`](data/) | 验收数据与运行期数据（地图、巡检记录、路线） |
| [`docs/`](docs/) | 技术文档、算法解析、代码结构、复现步骤、FAQ |
| `deprecated/` | 历史实现存档，不参与运行，仅供追溯 |

---

## 4. 硬件清单

| 部件 | 型号 / 规格 | 接口 | 说明 |
|---|---|---|---|
| 计算板 | 犀牛派 X1（QCS8550，6 核，14GB） | — | Ubuntu 22.04 / AidLux，ROS 2 Humble |
| 激光雷达 | RPLidar A1 | USB 串口 `/dev/ttyUSB0` | 实测 7.7Hz（硬件上限） |
| 姿态传感器 | GY-521（MPU6050） | I²C-1，地址 0x68 | 100Hz，提供角速度 |
| 底盘 | 阿克曼转向底盘 | — | 后驱 + 前舵机转向 |
| 电机驱动 | L298N 双通道 | GPIO 软 PWM | 通道 L=右轮，通道 R=左轮（交叉） |
| 转向舵机 | 标准舵机 | GPIO 33（50Hz） | 独立进程 servo_proc |
| 相机 | USB UVC 相机 | `/dev/video2` | MJPG 640×480 |
| 二维云台 | 串口云台 | `/dev/ttyHS1`，115200 | 6 字节速度帧协议 |
| 灭火机构 | 执行器 + 驱动 | GPIO/继电器 | 详见 `docs/技术文档.md` |

> 完整引脚级接线见 [`motor/motor_wiring.md`](motor/motor_wiring.md)、[`gimbal/gimbal_wiring.md`](gimbal/gimbal_wiring.md)、[`slam/gyro_mapping_README.md`](slam/gyro_mapping_README.md)。

---

## 5. 快速开始

### 5.1 环境

- 犀牛派 X1（AidLux，Ubuntu 22.04 ARM64）
- ROS 2 Humble（`/opt/ros/humble`）
- `~/lidar_ws`：rplidar_ros、slam_toolbox、nav2

```bash
source /opt/ros/humble/setup.bash
source ~/lidar_ws/install/setup.bash
```

### 5.2 依赖

```bash
pip install -r requirements.txt
```

系统级依赖（apt）：`python3-gpiod`、`python3-smbus`、`python3-opencv`、`python3-serial`（或 pip 等价包）。
逐个目录的依赖与用途见各目录 `README.md`。

### 5.3 启动

```bash
# 1) 电机直连 PWM（会先停掉 motor_driver 让出 GPIO）
bash ~/robot/motor/start_pwm.sh

# 2) 网页（遥控/建图/导航/巡检）
bash ~/robot/web/start_slam_viz.sh      # http://<板卡IP>:5800

# 3) 建图（网页"建图"按钮等价）
bash ~/robot/slam/mapping_ctl.sh start
bash ~/robot/slam/mapping_ctl.sh status
bash ~/robot/slam/mapping_ctl.sh stop
```

### 5.4 验证

```bash
# 雷达
ros2 topic hz /scan
# 姿态
ros2 topic hz /imu/data_raw          # 期望 ~100Hz
# 里程计
ros2 topic hz /odom
# 网页
curl -s http://127.0.0.1:5800/status
```

---

## 6. 使用流程

1. **建图**：网页点"建图" → 遥控走一圈 → 停 → 保存地图（自动生成缩略图与 YAML）。
2. **定位**：选择地图 → 加载定位 → 在页面上拖拽初始位姿 → 等待定位收敛（协方差 < 0.25）。
3. **画路线**：在导航页/巡检页按地图点选点位（或按语义区域筛选）。
4. **预览**：预览路线，确认可规划、方向正确。
5. **执行**：启动巡检 → 机器人逐点导航、停稳、取图、检测 → 生成报告。
6. **查看报告**：按点位/区域汇总，含原图、结论、置信度与时间。

---

## 7. 测试

| 测试文件 | 覆盖 | 结果 |
|---|---|---|
| `navigation/navigation_test.py` | 坐标转换、门控 | 10/10 |
| `navigation/patrol_test.py` | 路线存储与预览 | 14/14 |
| `navigation/navigation_runtime_test.py` | 看门狗、定位跳变 | 12/12 |
| `navigation/patrol_runtime_test.py` | Nav2 回调 | 6/6 |
| `navigation/patrol_drive_test.py` | 短段直行 | 6/6 |
| `navigation/frontier_test.py` | 前沿探索 | 6/6 |
| `navigation/web_navigation_test.py` | SocketIO 事件 | 8/8 |
| `navigation/autonomy_test.py` | 任务编排 | 16 + 1 skip |
| `motor/motor_driver_test.py` | 电机控制数学 | 全通过 |
| `gimbal/gimbal_test.py` | 云台协议 | 15/15 |
| `web/web_drive_test.js` | 网页驾驶控件 | 104/104 |

```bash
python3 -m pytest navigation/ motor/ gimbal/ inspection/ -q
```

---

## 8. 文档索引

| 文档 | 内容 |
|---|---|
| [`docs/技术文档.md`](docs/技术文档.md) | 设计理念、应用场景、硬件选型、系统搭建、完整流程 |
| [`docs/核心算法解析.md`](docs/核心算法解析.md) | 里程计、SLAM、定位、规划、巡检状态机、火情判定 |
| [`docs/代码结构说明.md`](docs/代码结构说明.md) | 模块划分、调用关系、关键类与函数 |
| [`docs/复现步骤与调试技巧.md`](docs/复现步骤与调试技巧.md) | 从零复现、分步验证、常见故障排查 |
| [`docs/FAQ.md`](docs/FAQ.md) | 常见问题 |
| [`docs/材料清单.md`](docs/材料清单.md) | BOM（型号、数量、采购关键词、参考价） |

---

## 9. 开源协议与致谢

本项目采用 **MIT License**，详见 [`LICENSE`](LICENSE) 与 [`开源授权声明.md`](开源授权声明.md)。

- 作者：XiYinadi
- 硬件平台：阿加犀 犀牛派 X1
- 第三方组件：ROS 2 Humble、slam_toolbox、Nav2、rplidar_ros、Qwen2.5-VL（各自遵循其原始协议）
