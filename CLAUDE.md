# Bridge75 固件：给 AI 的工作说明

用户用中文交流。键盘是 Shortcut Studio Bridge75 ANSI NoRGB：主控 WB32FQ95，无线模块 CH582F（闭源固件，经 UART 通信，115200）。目标是在官方 QMK 上稳定支持 USB、蓝牙 1–3、2.4G 接收器，并解决长时间闲置后首键延迟或丢失。

## 文档地图

| 文档 | 内容 |
| --- | --- |
| [README.md](README.md) | 项目概览、当前状态表、编译入口 |
| [AI交接.md](AI交接.md) | 本机目录、源码入口、排查经验、待办 |
| [docs/用户指南.md](docs/用户指南.md) | 刷机、键位、指示灯、VIA、常见问题 |
| [docs/诊断与实机测试.md](docs/诊断与实机测试.md) | Fn+D 字段、拆 `rx` 的算法、测试项 T1–T12、反馈格式 |
| [docs/架构与设计.md](docs/架构与设计.md) | 硬件、源码结构、协议与 ACK、低功耗、EEPROM、VIA、常量、设计取舍、风险 |
| [docs/发版规范.md](docs/发版规范.md) | 标签命名、构建、核对、实机验收、文档同步、提交与标记 |
| [docs/更新记录.md](docs/更新记录.md) | 每个版本的改动、验证、状态 |

改了行为就同步改对应文档；同一事实只在一处详写，其他地方链接过去。

## 目录与源码归属

工作目录 `/Volumes/Store/键盘/`：

| 路径 | 作用 |
| --- | --- |
| `bridge75-qmk/` | **本仓库，源码唯一来源。** GitHub 仓库 `AhaYue/qmk-bridge75`，分支 `main` |
| `qmk-bridge75-ansi-norgb/` | 编译用的官方 QMK checkout，基线 `33292390974c75362ca30873c482d052e0bd7a63`。其中 `keyboards/shortcut/{bridge75,wireless}` 和 `tools/bridge75` 由 `build.sh` 同步覆盖，**不要在那边改，也不要在那边提交**（它的 origin 是官方 qmk/qmk_firmware） |
| `firmware/` | 构建产物：`.bin`、`SHA256SUMS-*`、`logs-<标签>-<时间>/`、旧版本的补丁。不在 git 里 |
| 上游参考（已删除） | 原板级适配 `qmk_firmware-em-bridge75`、厂商 RC8 无线实现 `qmk_wireless_rc8` 已移植完毕，本机不保留；需要对照时按 [docs/架构与设计.md](docs/架构与设计.md#2-源码结构) 的命令重新 clone |
| `.venv-qmk/` | QMK CLI 的 Python 环境，构建脚本会自己设 PATH |
| `README.md`（工作目录根） | 给用户刷机看的入口，发版时同步更新 |

用户另有一个旧仓库 `AhaYue/qmk-bridge75`（VIA、ansi/iso 带 RGB 的另一条线），与本项目无关，不要动。

## 构建与发版

按 [docs/发版规范.md](docs/发版规范.md) 执行，要点：

```sh
cd /Volumes/Store/键盘/bridge75-qmk
BRIDGE75_TAG=via-v<版本号>-<YYYYMMDD> ./build.sh   # 例如 via-v1.5-20261121
```

- 标签格式 `via-v<版本号>-<YYYYMMDD>`（用户定的规则），版本号为 `<主>` 或 `<主>.<次>`，由用户决定；同一版本的修正沿用标签、靠构建时间区分。**用户要求文件名带到分钟的构建时间**，这由脚本自动加上。
- 脚本：同步 → lint → 三组主机测试 → 编译 `_resetwake` 版和正式版 → 校验值。任何一步失败即退出，去看 `firmware/logs-<标签>-<时间>/`。
- 构建完核对：`shasum -a 256 -c SHA256SUMS-…`（在 `firmware/` 里）、`grep -ci warning logs-*/build-*.log` 为 0、`strings <bin> | grep 'bridge75-.* fw'` 的前缀与文件名一致。
- 发版时同步：`docs/更新记录.md` 顶部加条目（按模板）、`README.md` 当前状态表、工作目录根的 `README.md`、本文件的"当前状态"。
- 提交信息用中文，第一行概括，正文写原因和验证。
- 只改测试或文档时不必打包；只跑测试用 README 里的三条命令。
- CI（`.github/workflows/build.yml`）只手动触发，校验标签格式，用 GCC 10（本机 GCC 8.5，告警更严）；改自定义键位时同步更新工作流里的 `expected` 列表。

## 测试

测试把真实源码 `#include` 或抽取进主机程序，配合桩函数和模拟时间；`test_wireless.c` 里有可丢包、带延迟回 ACK 的模拟模块 `sim_module`。改协议、低功耗、报告队列时要加测试，并确认新测试在旧代码上会失败。

## 用户怎么测、怎么读诊断

用户刷机后在空白文本框按 **Fn+D**，把输出粘贴回来。字段含义、拆 `rx` 的公式（`收到的 ACK = (rx − 3 × valid) / 3`；`ack + retries = tx`；每丢一份 `retries` +2）和测试项 T1–T12 都在 [docs/诊断与实机测试.md](docs/诊断与实机测试.md)。

- 计数从开机累计，拔插 USB 不清零。两份日志相减才是那一段的数据。
- `fw=0` 是老问题，与 ACK 无关。
- 用户可能担心自己的测试结果误导判断：同一种缺陷不一定每次都触发，要结合多份日志和模拟测试下结论，并向用户解释清楚。

## 当前状态（2026-11-21）

- **当前发布版**：via-v1.5-20261121，构建 20261121-0356（源码提交 `f0c45ea`，git 标签与 GitHub Release 同名），固件行为与 via-v1 / via2 相同。按用户决定未做 T1–T4 实机验收就发布，实机待测。
- **已实测**：ack 20261113-2237 的 ACK 修正（USB、蓝牙、2.4G，丢包率约 0.14%–0.8%）；diag3 是唯一通过蓝牙 6/35 分钟闲置的版本。
- **待用户测试**（编号见诊断文档）：T1–T4、T9、T10 当前功能；T5 蓝牙闲置首键；T6 2.4G 闲置与电脑睡眠（只有 2.4G 会走 `md_receive_host_cb`，从未实测）；T7 主机离开回连；T8 待机耗电；T11 2.4G `drops=76` 的来源；可选 T12 `sleep=1` 对照。
- **已知取舍**：ACK 不带序号。每丢一份会让两个报告各晚约 30 ms；要降到一次就得承担 ACK 错配丢帧的风险，不要改回去。

## 已知风险与注意点

详见 [docs/架构与设计.md](docs/架构与设计.md#12-已知风险)。改代码时特别注意：

- STOP 恢复依赖厂商机器码 `PRE_LP`/`POST_LP`，QMK 没有看门狗。
- `smsg` 环形缓冲的正确性依赖未写明的容量关系，改 `SMSG_NUM`/`SMSG_PAYLOAD_LEN` 前先想清楚。
- VIA 定义 8 项，顺序必须与 `info.json` 的 `keycodes` 一致；不要加入没有处理函数的键位。`VIA_ENABLE` 必须放在 keymap 的 `rules.mk`。无线不隧道 RAW HID。
- `BR_DIAG`、`BR_BAT` 共用 `bridge75.c` 的 `output_text` 缓冲，改输出逻辑时注意不要让两种输出交错。

## 约定

- 改代码前先读相关源文件；改协议、低功耗、报告队列时补测试。
- 不要覆盖用户实机测过的产物；新构建一律带新时间戳。
- 推送、打 git 标签、建 Release、改仓库可见性等对外操作先问用户。
