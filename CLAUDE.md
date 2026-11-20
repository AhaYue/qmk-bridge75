# Bridge75 固件：给 AI 的工作说明

用户用中文交流。键盘是 Shortcut Studio Bridge75 ANSI NoRGB：主控 WB32FQ95，无线模块 CH582F（闭源固件，经 UART 通信，115200）。目标是在官方 QMK 上稳定支持 USB、蓝牙 1–3、2.4G 接收器，并解决长时间闲置后首键延迟或丢失。背景和排查经验在 [AI交接.md](AI交接.md)，版本历史在 [docs/更新记录.md](docs/更新记录.md)。

## 目录与源码归属

工作目录 `/Volumes/Store/键盘/`：

| 路径 | 作用 |
| --- | --- |
| `bridge75-qmk/` | **本仓库，源码唯一来源。** GitHub 私有仓库 `AhaYue/bridge75-qmk`，分支 `main` |
| `qmk-bridge75-ansi-norgb/` | 编译用的官方 QMK checkout，基线 `33292390974c75362ca30873c482d052e0bd7a63`。其中 `keyboards/shortcut/{bridge75,wireless}` 和 `tools/bridge75` 由 `build.sh` 同步覆盖，**不要在那边改，也不要在那边提交**（它的 origin 是官方 qmk/qmk_firmware） |
| `firmware/` | 构建产物：`.bin`、`SHA256SUMS-*`、`logs-<标签>-<时间>/`、旧版本的补丁和说明。不在 git 里 |
| `qmk_firmware-em-bridge75/`、`qmk_wireless_rc8/` | 只读参考：原板级适配、厂商 RC8 无线实现 |
| `.venv-qmk/` | QMK CLI 的 Python 环境，构建脚本会自己设 PATH |

用户另有一个旧仓库 `AhaYue/qmk-bridge75`（VIA、ansi/iso 带 RGB 的另一条线），与本项目无关，不要动。

## 构建

```sh
cd /Volumes/Store/键盘/bridge75-qmk
BRIDGE75_TAG=<标签> ./build.sh
```

- 同步三个目录到 QMK checkout（只复制内容有变化的文件），然后运行 lint、三组主机测试、编译软唤醒版和 `_resetwake` 回退版。任何一步失败脚本即退出，去看对应日志。
- 产物：`firmware/bridge75_ansi_norgb_<标签>_<YYYYMMDD-HHMM>.bin`、`..._resetwake.bin`、`SHA256SUMS-<标签>-<时间>`，日志在 `firmware/logs-<标签>-<时间>/`。
- 构建时间同时写进固件，Fn+D 诊断以 `bridge75-<标签>-<时间>` 开头。**用户要求文件名带到分钟的时间**，用来区分版本；不经脚本编译的固件前缀是 `bridge75-dev`。
- 标签用来描述这一批改动（已用过 `final`、`ack`、`power`），同一批的后续修正沿用标签，靠时间区分。
- 构建完核对：`grep -ci warning firmware/logs-*/build-*.log` 为 0；`strings <bin> | grep 'bridge75-.* fw'` 的前缀与文件名一致；`shasum -a 256 -c SHA256SUMS-...`（在 `firmware/` 里运行）。
- 只改测试或文档时不必打包；只跑测试：

```sh
sh keyboards/shortcut/wireless/tests/run.sh
python3 keyboards/shortcut/wireless/tests/lowpower/run.py
python3 keyboards/shortcut/wireless/tests/board/run.py
```

测试是把真实源码 `#include` 进主机程序，配合桩函数和模拟时间；`test_wireless.c` 里有一个可丢包、带延迟回 ACK 的模拟模块（`sim_module`）。改协议逻辑时要加测试，并确认新测试在旧代码上会失败。

- GitHub Actions：`.github/workflows/build.yml` 在手动触发或推送 `keyboards/`、`tools/`、`build.sh`、`docs/via/` 时，用固定 QMK 基线在 Ubuntu 22.04 上跑同一套 lint、测试和编译，把两个 bin、`SHA256SUMS-*`、日志和 VIA JSON 作为 artifact 上传。改自定义键位时同步更新工作流里的 `expected` 列表。

## 每次发布新固件要做的事

1. `BRIDGE75_TAG=<标签> ./build.sh`，按上面核对。
2. 在 `docs/更新记录.md` 顶部加一条：文件名、校验文件、改了什么、为什么、怎么验证、建议用户实机测什么。旧条目改成"已被取代"或补上实测结果。
3. 更新 `README.md` 的"当前状态"表；工作目录根下的 `/Volumes/Store/键盘/README.md` 也指向新文件（那份是给用户刷机看的入口）。
4. 提交并推送 `main`。提交信息用中文，第一行概括，正文写原因和验证。
5. 告诉用户：文件名、Fn+D 应显示的前缀、要测哪些项目。

## 用户怎么测、怎么读诊断

用户刷机后在空白文本框按 **Fn+D**，把输出粘贴回来，例如：

```
bridge75-power-20261113-2349 fw=0 dev=1 state=2 wakes=0 source=0 retries=6 drops=0 sleep=0 bat=87 rx=1191 ack=352 valid=42 bad=0 tx=358 rgb=1/1/0 leds=7622 rail=0
```

- 计数从开机累计，拔插 USB 不清零（有电池）。两份日志相减才是那一段的数据。
- `dev`：0 USB、1–3 蓝牙、6 为 2.4G。`state`：2 已连接，1 配对中，3 断开。
- **拆 `rx`**：模块发来的帧除 RAW（34 字节，本项目不用）外都是 3 字节，所以 `收到的 ACK 数 = (rx − 3 × valid) / 3`。
  - `tx − 收到的 ACK` = 模块没回 ACK 的份数（丢包）；
  - `收到的 ACK − ack` = 被当作欠账吃掉的 ACK；
  - `ack + retries` 应等于 `tx`。
  - 正常时每丢一份正好 `retries` +2，`bad=0`。若 `retries` 远大于丢包数的两倍，说明出现连锁重发。
- `drops`：报告缓存溢出或过期、发送队列满、重试耗尽、串口半帧，共用一个计数，单看无法区分来源。
- `wakes`：MCU 从 STOP 唤醒次数；`source` 为最近一次唤醒源：1 矩阵按键、2 UART（模块消息）、3 线缆。
- `bat`：模块上报的电量；模块回复前为默认 100。
- `fw=0` 是老问题（读不到模块版本），蓝牙照常工作，与 ACK 无关。
- 用户可能担心自己的测试结果误导判断：同一种缺陷不一定每次都触发，要结合多份日志和模拟测试下结论，并向用户解释清楚。

## 当前状态（2026-11-21）

- 最新：via2 20261121-0202，在 via 基础上删除 4 个未实现的自定义键位（VIA 定义缩为 8 项），新增 Fn+B 电量输出 `bat=NN`，Fn+D/Fn+B 输出末尾不再按回车，默认键位改为 Fn+1–5 = 蓝牙 1–3、2.4G、有线（模块休眠移到 Fn+`），待实机测试。via 20261121-0106 已被取代；power 20261113-2349 的无线项也仍待测。ack 20261113-2237 已在 USB、蓝牙、2.4G 验证 ACK 修正（丢包率约 0.14%–0.8%）。diag3 是唯一通过蓝牙 6/35 分钟闲置实测的版本。
- **待用户测试**：
  1. 2.4G 下 `drops=76` 的来源：保持连接，打字前后各按一次 Fn+D，看 `drops` 是否增长。这 76 次不是配对产生的（接收器本来就已配对，切过去即连上）。
  2. 蓝牙闲置 6 分钟和 35 分钟后的首键（项目最初的问题），via/via2 上还没测。
  3. 主机离开：蓝牙模式下关掉电脑蓝牙 30–60 分钟，看能否自动回连，以及 `wakes`/`source`（`source=2` 很多表示被模块消息反复唤醒）。
  4. 闲置前后各记一次 `bat`，估算待机耗电；可选：整夜放置；`sleep=1` 下 35 分钟（缓存 5 秒的报告重放可能让首键只是晚到而不丢）。
  5. 2.4G 下电脑睡眠和唤醒（只有 2.4G 会走 `md_receive_host_cb`），从未实测。
  6. 有线 VIA：在 usevia.app 的 Design 页加载 `docs/via/bridge75-norgb-via.json`（8 项），确认能读写键位、能选到 `DIAG`/`BAT`/`SLEEP`；Fn+Esc 后恢复默认键位；切到蓝牙/2.4G 后设备从 VIA 列表消失。
  7. Fn+B：USB 和蓝牙下各按一次，确认输出 `bat=NN`，且与 Fn+D 里的 `bat=` 一致；两种输出末尾都没有回车。
  8. Fn+1–5 依次切到蓝牙 1–3、2.4G、有线；Fn+` 切换模块休眠；Fn+Esc 清 EEPROM。
- 已知取舍：ACK 不带序号。每丢一份会让两个报告各晚约 30 ms；要降到一次就得承担 ACK 错配丢帧的风险，不要改回去。

## 已知风险与可改进点

- STOP 恢复依赖厂商机器码 `PRE_LP`/`POST_LP`（`lpwr_wb32.c`），无法审查；QMK 没有看门狗，恢复失败会卡死直到断电。
- 每次 STOP 唤醒都调用 `md_recover()` 重置链路，依赖模块重新上报"已连接"；2.4G 下未验证。
- 醒着时 MCU 全速运行 5 分钟（`LPWR_TIMEOUT`），6 颗 WS2812 的供电直到休眠前才切断。
- `smsg` 环形缓冲的正确性依赖未写明的容量关系，有百万次队列测试兜底，改 `SMSG_NUM`/`SMSG_PAYLOAD_LEN` 前先想清楚。
- RAW HID 只支持 USB 有线（VIA），无线不隧道 RAW HID；`wireless.mk` 在未声明 `BRIDGE75_USB_RAW_HID=yes` 时会拒绝显式打开的 `RAW_ENABLE`。`VIA_ENABLE` 必须放在 keymap 的 `rules.mk`，键盘级会被 `qmk lint` 拒绝。
- VIA 定义现在是 8 项，顺序必须与 `info.json` 的 `keycodes` 一致；不要再加入没有处理函数的键位。`BR_DIAG`、`BR_BAT` 共用 `bridge75.c` 的 `output_text` 缓冲，`BR_SLEEP` 仍独立；改输出逻辑时注意不要让两种输出交错。VIA magic 随构建时间变化，刷入新固件后旧键图会自动重置。

## 约定

- 改代码前先读相关源文件；改协议、低功耗、报告队列时补测试。
- 不要覆盖用户实机测过的产物；新构建一律带新时间戳。
- 推送、建 Release、改仓库可见性等对外操作先问用户。
