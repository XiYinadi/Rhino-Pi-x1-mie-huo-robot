# stage2/ — 阶段二验收数据

阶段二目标是"保存地图 + 定位 + 规划"的**非行驶验收**：只订阅、不驱动。
本目录保存 2026-10-23 的验收快照，便于回溯当时的参数与结论。

## 文件说明

| 文件 | 内容 |
|---|---|
| `preflight-20261023.json` | 启动前预检：ROS 环境、话题、TF、地图文件、权限 |
| `saved-map-check-20261023.json` | 保存地图校验：尺寸、分辨率、原点、占据比例 |
| `slam-parameters-20261023.json` | slam_toolbox / AMCL 关键参数快照 |
| `web-save-preflight-20261023.json` | 网页保存地图前的预检（存储、缩略图、区域文件） |

## 相关代码

- 验收脚本：`stage2/` 未包含脚本本体，逻辑在 `navigation/navigation_smoke.py`（板端非行驶验收）。
- 地图存储：`navigation/map_store.py`（版本化、不覆盖、`.trash/`）。
- 定位收敛判定：`navigation/localization_settle.py`。

## 复跑

```bash
source /opt/ros/humble/setup.bash
source ~/lidar_ws/install/setup.bash
python3 ~/robot/navigation/navigation_smoke.py --map <地图名>
```

验收判据：地图可加载、AMCL 协方差 < 0.25、规划成功、**拒绝任何运动指令**。
