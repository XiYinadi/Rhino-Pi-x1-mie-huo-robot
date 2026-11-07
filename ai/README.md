# ai/ — 本地视觉大模型（VLM）请求与诊断

本目录是**诊断脚本集合**：用于验证板端 VLM 服务是否健康、请求是否会被打崩、相机帧能否正确送入模型。
运行时真正的 VLM 调用在 `inspection/local_model.py` 与 `camera/fire_detect.py`，这里只放排查工具。

## 文件说明

| 文件 | 作用 |
|---|---|
| `_vlm_req.py` | 参数化 VLM 请求：`_vlm_req.py <img_src\|CAM> <long\|short>`，打印模型原始返回与耗时 |
| `_vlm_probe.py` | 单次长提示词请求，用于判断"长提示词是否会把服务打崩" |
| `_sock_cam_test.py` | 连 `slam_viz:5800`，开 `camera_control`，验证能收到 `frame`（原始帧） |
| `_sock_fire_test.py` | 同上，验证 `fire_frame` / `fire_status`（检测结果帧） |
| `_vlm_matrix.sh` | 批量矩阵：多组图片 × 提示词，统计成功率与耗时 |
| `_vlm_restart_test.sh` | 反复重启 VLM 服务并请求，验证恢复行为 |
| `_api_fg_diag.sh` | 前台运行 `api` 进程并观察日志（判断是否被 OOM/超时杀掉） |

## 典型用法

```bash
# 单张图片 + 长提示词
python3 _vlm_req.py /tmp/frame.jpg long

# 直接抓相机帧
python3 _vlm_req.py CAM short

# 相机帧通道是否通
python3 _sock_cam_test.py

# 火情结果通道是否通
python3 _sock_fire_test.py
```

## 已知现象

- VLM 推理会占用 **100%~220% CPU**，长时间连发会让整板负载升高，进而影响 SSH（出现过 `kex_exchange_identification: Connection reset`）。
  压力测试请**短时、单发**，并在结束后确认 `api` 进程已退出。
- 长提示词（>2k token）比短提示词更容易触发超时；超时后服务可能短暂无响应，用 `_vlm_restart_test.sh` 验证恢复。
- 这些脚本**不参与生产链路**，也不会驱动任何执行器。
