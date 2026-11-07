# web/ — 网页端（Flask + SocketIO）

网页端是整套系统的**唯一操作入口**：遥控、建图、导航、巡检、地图编辑、报告查看都在这里。
后端 `slam_viz.py`（约 5800 端口）同时承担：ROS 话题桥接、UDP 电机/舵机/云台输出、
SocketIO 事件分发、地图与巡检数据读写。

## 页面路由

| 路由 | 页面 | 主要脚本 |
|---|---|---|
| `/control` | 遥控驾驶（双摇杆/单摇杆/键盘、云台摇杆、急停） | `control.js` + `control.css` |
| `/mapping` | 实时建图（地图、扫描、轨迹、建图开关） | `mapping.js` + `mapping.css` |
| `/navigation` | 导航（加载定位、选点、预览、执行） | `navigation_page.js` + `navigation.js` |
| `/patrol` | 巡检（路线编辑、预览、启动、报告） | `missions.js` + `missions.css` |
| `/explore` | 自主探索（前沿探索任务） | `autonomy.js` |
| `/map-editor` | 地图管理（缩略图、重命名、正式图、语义区域、清理） | `map_editor.js` + `map_editor.py` |
| `/inspection` | 巡检记录与报告 | `inspection.js` + `inspection.html` / `inspection_report.html` |

## 文件说明

| 文件 | 作用 |
|---|---|
| `slam_viz.py` | 后端主程序：ROS 订阅/发布、SocketIO、UDP 输出、地图与巡检 API、页面模板 |
| `start_slam_viz.sh` | 启动脚本（含 `RHINO_WEB_*` 环境变量与依赖自检） |
| `control.js` / `control.css` | 遥控页：摇杆映射、驾驶权、键盘、急停 |
| `mapping.js` / `mapping.css` | 建图页：地图渲染、扫描叠加、跟随/缩放、建图按钮 |
| `navigation_page.js` | 导航页：地图交互、选点、预览、目标下发 |
| `navigation.js` | 导航页的 SocketIO 通道与状态机（心跳、取消） |
| `missions.js` / `missions.css` | 巡检/探索页：路线列表、点位编辑、报告入口 |
| `autonomy.js` | 探索任务控制（任务状态、前沿点） |
| `map_editor.py` | 地图编辑后端：只读副本编辑，不覆盖原图 |
| `map_editor.js` / `map_editor.html` / `map_editor.css` | 地图编辑前端：区域绘制、缩略图、重命名、清理 |
| `inspection.js` / `inspection.html` / `inspection_report.html` | 巡检记录与报告展示（按点位/区域汇总） |
| `web_drive_test.js` | **前端离线测试**：从 `slam_viz.py` 抽出真实内联 JS 在假 DOM 上跑（104 项） |
| `navigation_ui_test.js` | 导航页前端逻辑测试 |
| `autonomy_ui_test.js` | 探索页前端逻辑测试 |
| `inspection_ui_test.js` | 巡检页前端逻辑测试 |

## 关键约定

1. **驾驶权**：`armed` 只认点了「启用电机」的客户端（`_drive_owner`，按 localStorage 的稳定 CLIENT_ID）。
   非持权客户端的速度包一律丢弃；断线有 6 秒宽限。
2. **断线停车**：300ms 收不到摇杆包 → `pwm_proc` deadman 归零；页面不可见时前端主动停发。
3. **零速包不算接管**：按下摇杆瞬间的 `takeover=1 + 0/0` 不会取消巡检（必须带速度才算手动接管）。
4. **原地旋转**：`motor_control` 的 `rot` 字段只由导航/巡检内部使用，手动摇杆永不发送。
5. **测试**：改任何前端脚本后必须跑 `node web/web_drive_test.js`（退出码 0 = 全过）。

## 启动

```bash
bash ~/robot/web/start_slam_viz.sh
# 打开 http://<板卡IP>:5800/control
```

## 依赖

见仓库根 `requirements.txt`（Flask、Flask-SocketIO、simple-websocket、Pillow 等）。
