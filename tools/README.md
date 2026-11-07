# tools/ — 诊断与测试工具

本目录放**不参与运行时**的排查工具。它们的共同前提是：**只读或人工观察，不改变机器人的运动状态**。
除 `rotate_test.py`（会驱动电机，需手动确认）外，其余工具都不会给电机使能。

## 文件说明

| 文件 | 作用 | 是否会动电机 |
|---|---|---|
| `gpio_smoke.py` | GPIO 冒烟测试 + libgpiod API 兼容性检查（验证 `motor_hw.py` 能否在本板申请到线） | 否 |
| `pin_hold.py` | 把电机控制脚钉在固定电平，方便万用表逐点排查"为什么电机不动" | 否（只拉电平） |
| `enc_test.py` | 编码器接线测试：手转轮子，看 4 路输入有没有边沿 | 否 |
| `encoder_signal_probe.py` | 只读原始编码器事件，统计边沿与计数 | 否 |
| `encoder_native_probe.c` | C 版编码器探针（排查 Python 层丢边沿） | 否 |
| `enc_dir_test.py` | **用编码器判断左右轮实际转向**（pwm_proc 不申请编码器线，可直接读） | 是（低速） |
| `wheel_dir_probe.py` | 单通道方向探针：先停 pwm_proc，再逐通道给 A/B 两种方向位 + 15% 占空比 | 是（需架空） |
| `rotate_test.py` | 原地旋转固定角度（陀螺闭环），用于标定角速度与转向 | 是（需清场） |
| `rate_probe.py` | 测 `/wheel_speed`（控制回调）的真实周期分布，用来发现控制环被饿死 | 否 |
| `cmd_log.py` | 把 `/cmd_vel` 与 `/wheel_speed` 逐条带时间戳打到 stdout | 否 |
| `diag_client.py` | 假扮网页客户端连 `slam_viz`，量真实发送率/接收率 | 否 |
| `motor_probe.py` | 直接 rclpy 探针（绕过 AidLux 上坏掉的 ros2 daemon） | 否 |
| `topic_probe.py` | 从任意话题取一条消息，按字段路径打印（超时返回 1） | 否 |
| `start_vnc.sh` | 启动 VNC（DISPLAY=:2） | 否 |
| `ENCODER_DIAGNOSTICS.md` | 编码器只读诊断的结论与命令记录 | — |

## 典型用法

```bash
# 1) GPIO 是否可用（不需要 sudo）
python3 gpio_smoke.py

# 2) 编码器有没有接线
python3 enc_test.py            # 手转轮子观察边沿

# 3) 左右轮转向是否一致（架空）
pkill -f 'python3 .*pwm_proc.py'
python3 wheel_dir_probe.py     # 逐段观察哪个方向是前进

# 4) 控制环节拍是否正常
python3 rate_probe.py /wheel_speed

# 5) 取一条话题消息的字段
python3 topic_probe.py /odom pose.pose.position.x
```

## 注意

- `wheel_dir_probe.py` / `enc_dir_test.py` 会**占用 GPIO 或驱动电机**，运行前先停 `pwm_proc`，测完用 `bash ~/robot/motor/start_pwm.sh` 恢复。
- 这些脚本依赖 `python3-gpiod` 与 ROS 2 环境，按文件头的注释 source 对应 setup。
- 工具脚本允许出现中文注释与打印，但**不要**把它们接进生产启动链路。
