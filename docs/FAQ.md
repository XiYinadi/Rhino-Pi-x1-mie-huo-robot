# 常见问题（FAQ）

## 硬件与接线

**Q1：电机不转，日志里 `armed=N`？**
点网页上的"启用电机"。`armed` 只认点击的那个客户端（按 localStorage 的 CLIENT_ID），
换浏览器/清缓存后需要重新点。

**Q2：电机不转，`armed=Y` 但没有 duty？**
1. 检查是否在推摇杆（deadman 300ms 无包即归零）。
2. 看 `duty L/R` 是否非零、`dir` 是否在变。
3. 用万用表量 L298N 的 ENA/ENB：PWM 时应为脉冲。
4. 若某方向始终不转：该通道的方向引脚或电机线接反/断线，用 `tools/wheel_dir_probe.py` 逐通道验证。

**Q3：左轮前进/倒车方向反了？**
先确认接线（通道 R 驱动左轮）。方向由 `flip_left/flip_right` 决定，
改完必须重启 `pwm_proc`。⚠️ 确认 `~/motor_params.yaml` 是指向仓库文件的软链接，
否则改了不生效。

**Q4：舵机抖？**
1. 抖是**持续**的还是**按住转向时**的？持续抖多为共地/供电/机械限位。
2. `tail -f /tmp/servo_proc.log` 看脉宽是否稳定。
3. 供电不足（排针 5V 同时带相机/逻辑）也会抖。

**Q5：云台不动？**
1. 只接 TX + GND，不要接 RX。
2. `bash ~/robot/gimbal/start_gimbal.sh status`。
3. 云台协议是**速度锁存**：断链必须持续发零速帧，否则会一直转。

## IMU 与里程计

**Q6：`Sensor moving during calibration` 反复出现？**
静止噪声超过门限。检查：
1. 模块是否固定（晃动会被当成运动）。
2. 供电/接线是否稳定（读取失败会丢样本）。
3. 门限当前为 `std ≤ 0.05 rad/s`、`|mean| ≤ 0.20 rad/s`（`slam/gyro_core.py`）。

**Q7：`/imu/data_raw` 没有数据？**
1. `WHO_AM_I` 能否读到 `0x68`。
2. 是否有第二个进程占着 `/dev/i2c-1`（`fuser -v /dev/i2c-1`）。
3. 模块型号：GY-85 的陀螺是 ITG3200，**不兼容**本驱动。

**Q8：里程计频繁跳变？**
激光匹配退化（长走廊、玻璃、空旷）。看日志 `rejected` 计数；
必要时重新定位、降低速度、增加环境特征。

## 建图与定位

**Q9：建图不出图 / 地图全空？**
1. `base_frame` 必须是 `base_link`（与静态 TF 一致）。
2. `/odom` 是否在发布（slam_toolbox 会丢弃拿不到位姿的帧）。
3. 雷达是否真的在转（`ros2 topic hz /scan`）。

**Q10：保存的地图加载后定位乱飘？**
1. 初始位姿拖准了吗（车头方向）。
2. 地图是否在**车走过**的区域，而不是空白处。
3. AMCL 参数 `max_beams`/`laser_max_range` 是否与雷达匹配。

## 导航与巡检

**Q11：规划失败（`status=6` ABORTED）？**
1. 起点离墙太近（<0.3m）或落在障碍里 → 挪车。
2. 目标点在障碍/未知区域 → 换个点。
3. `allow_unknown=true` 已开；仍有问题看 `/tmp/navigation.log`。

**Q12：规划路径绕圈？**
Smac Hybrid-A\* 在 1.2m 内的目标可能画整圈（最小转弯半径 0.65m）。
巡检对 ≤2m 的短段走直行段，不走 Nav2。

**Q13：巡检到了点位但报告说"未到达"？**
1. 看报告里的 `arrival_basis`：`nav2_success...` 还是 `direct_leg...`。
2. 到点半径 0.20m；若定位误差大于此，会判"未到达"。
3. 冲过头规则：最近 0.6m 内又远离 0.3m 也算到。

**Q14：巡检到点后不动了？**
看状态：`dwell`（停稳确认中，最多 25s）→ `detecting`（VLM 推理）。
若卡在 `detecting`，检查 VLM 服务是否活着。

**Q15：巡检中途停止，提示"实测速度异常"？**
连续 3 帧里程计速度超出 `[-0.03, 0.60] m/s`。常见于启动瞬间的里程计尖峰或轮子打滑。
可稍等里程计稳定后再启动。

## 网页

**Q16：网页打开很慢/卡？**
1. 板卡负载（`uptime`）——VLM 推理会占满 CPU。
2. 地图较大时前端渲染压力大，可关闭"跟随"。
3. 用有线或 5GHz Wi-Fi。

**Q17：摇杆延迟高？**
1. 页面必须在**前台**（后台标签页被浏览器节流到 1Hz）。
2. 包率：`tools/diag_client.py` 量真实发送率。
3. 网络 RTT。

**Q18：换了浏览器后要重新点"启用电机"？**
是。驾驶权绑定 CLIENT_ID（localStorage），换浏览器/无痕模式视为新客户端。

## 其他

**Q19：SSH 掉线（`kex_exchange_identification: Connection reset`）？**
板卡资源耗尽（常见于出报告时 VLM 占满 CPU）。等 1~2 分钟或重启。
建议测试 VLM 时短时单发。

**Q20：`gpioinfo` 报 Permission denied？**
`fix-gpio-perms.service` 未生效：`systemctl status fix-gpio-perms`，或手动
`sudo chgrp gpio /dev/gpiochip* && sudo chmod 660 /dev/gpiochip*`。
