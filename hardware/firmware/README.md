# 步进电机云台控制固件（STM32F103C8T6）

本目录是**云台的接收端固件**：犀牛派通过串口发 6 字节速度帧，C8T6 解析后产生
STEP/DIR 脉冲，驱动 X/Y 两轴步进电机。与仓库里的 [`gimbal/`](../../gimbal/) 配套：

```
网页摇杆 → gimbal_proc (UDP 5814) → /dev/ttyHS1 → 本固件 (C8T6) → STEP/DIR → 步进驱动器
```

## 1. 硬件

| 项 | 值 |
|---|---|
| 主控 | STM32F103C8T6（"蓝 pill"最小系统板） |
| 时钟 | 8 MHz 外部晶振（工程默认） |
| 串口 | USART2：PA2 = TX，PA3 = RX，**115200 8-N-1** |
| 下载 | SWD（工程配 J-Link，见 `Project.uvprojx`） |

### 引脚定义（与 `User/main.c` 一致）

| 轴 | 信号 | 引脚 |
|---|---|---|
| X | EN（使能，低有效） | PA4 |
| X | STP（脉冲） | PA5 |
| X | DIR（方向） | PA6 |
| Y | EN（使能，低有效） | PB0 |
| Y | STP（脉冲） | PB1 |
| Y | DIR（方向） | PB10 |
| 状态灯 | LED | PC13（低亮） |
| 火焰输入（可选） | DO1 / DO2 | PA0 / PA1 |

## 2. 串口协议

6 字节定长帧，无转义、无重发；`Sum = (byte1 + byte2 + byte3 + byte4) & 0xFF`。

### 2.1 速度帧（0xAA）

```
0xAA | dxH dxL | dyH dyL | SUM
```

- `dx/dy`：**有符号 int16 大端**，是"速度档位"而不是步频；
- 固件内部换算：`steps_per_sec = |value| × SPEED_SCALE`（当前 `80`）；
- `|value| ≤ INPUT_DEAD_ZONE`（当前 `5`）→ 停转并释放电机（EN 拉高）；
- 上限 `MAX_STEPS_PER_SEC = 20000`；
- 速度**锁存**：不发新帧就一直按上一个速度转，所以**必须持续发送**（50Hz），
  停转要显式发 `(0,0)`。

### 2.2 动作帧（0xBB）

```
0xBB | cmd | 0 | 0 | 0 | SUM
```

| cmd | 含义 | 行为 |
|---|---|---|
| `1` | 软件清零角度 | 速度清零、角度计数归零，回 `ANGLE_RST` |
| `2` | 开环回零 | 两轴各走 `HOME_MOVE_*` 步后角度归零，回 `HOMING... / HOME_OK` |

### 2.3 调试回包

速度变化时回一行 ASCII：`dx=.. dy=.. ax=.. ay=..`（角度为脉冲换算的整数度）。

## 3. 关键参数（`User/main.c`）

| 宏 | 默认值 | 说明 |
|---|---|---|
| `SPEED_SCALE` | 80 | 每个输入单位对应的步/秒 |
| `INPUT_DEAD_ZONE` | 5 | 死区，防抖 |
| `MAX_STEPS_PER_SEC` | 20000 | 步频上限 |
| `STEPS_PER_REV_X/Y` | 3200 | 每圈步数（细分） |
| `HOME_ON_BOOT_OPEN_LOOP` | 0 | 上电是否自动回零 |
| `FLAME_STOP_MOTORS_ON_DETECT` | 0 | 火焰检测到即停（需接 PA0/PA1） |

## 4. 编译与烧录

1. 用 **Keil MDK 5** 打开 `Project.uvprojx`
2. 选择 `STM32F103C8` 器件与 `startup_stm32f10x_md.s`（工程已配好）
3. `Build` → 用 J-Link / ST-Link 烧录到 C8T6

> 工程使用 ST **标准外设库**（`Library/`），不依赖 CubeMX HAL。

### ⚠️ 关于 `flame_sensor.c/h`

原始压缩包里**缺少** `hardware/flame/flame_sensor.c/h`，但 `main.c` 里 `#include` 并调用了
`FlameSensor_Init()`。为保证工程可直接编译，这里按 `main.c` 的调用点与注释
（"hardware uses PA0/PA1"）**重建了最小实现**（上拉输入、低电平=有火）。
若你本地保留了原始文件，请覆盖这两个文件。

## 5. 与上位机的联调

```bash
# 板端：启动云台进程
bash ~/robot/gimbal/start_gimbal.sh start

# 方向与行程验证（不经过网页）
python3 ~/robot/gimbal/gimbal_inject.py --axis-sweep

# 看发送状态（帧/s、目标 vs 已发）
tail -f /tmp/gimbal_proc.log
```

判据：

- 摇杆前推 → 画面向上（或按机械定义）；不符就改 `gimbal_invert_y`
- 没反应：先确认 115200 8N1、TX/RX 是否交叉、共地
- 一直转：没有持续发零速帧（协议是速度锁存）

## 6. 目录

```
stm32-gimbal/
├── Project.uvprojx        Keil 工程
├── User/                  main.c、中断、配置
├── hardware/us2/          USART2 驱动与协议解析
├── hardware/flame/        火焰输入（重建的最小实现）
├── System/                Delay
├── Start/                 启动文件 + CMSIS
└── Library/               ST 标准外设库
```
