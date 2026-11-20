# Bridge75 固件交接

给接手的人（或 AI）看：本机目录怎么摆、从哪里读起、排查时踩过哪些坑。技术细节在[架构与设计](docs/架构与设计.md)，版本状态在[更新记录](docs/更新记录.md)，发版步骤在[发版规范](docs/发版规范.md)。

## 1. 项目一句话

把 Shortcut Studio Bridge75 ANSI NoRGB（WB32FQ95 主控 + 闭源 CH582F 无线模块，UART 通信）的无线支持移植到官方 QMK，并解决拔线后无线长时间闲置、首次输入延迟或丢键的问题。

## 2. 源码与本机目录

本仓库（GitHub `AhaYue/qmk-bridge75`，分支 `main`）是**源码的唯一来源**，只包含 `keyboards/shortcut/{bridge75,wireless}`、`tools/bridge75`、文档和 CI。编译时 `build.sh` 用 rsync 把这三个目录同步进一个官方 QMK checkout 再构建。

维护者本机的工作目录 `/Volumes/Store/键盘/`：

| 路径 | 作用 |
| --- | --- |
| `bridge75-qmk/` | 本仓库 |
| `qmk-bridge75-ansi-norgb/` | 编译用的官方 QMK checkout，基线 `33292390974c75362ca30873c482d052e0bd7a63`。其中三个目录会被同步覆盖，**不要在那边改、也不要在那边提交**（它的 origin 是官方 qmk/qmk_firmware） |
| `firmware/` | 构建产物、校验值、日志；还有旧版本的单文件补丁。不在 git 里 |
| `.venv-qmk/` | QMK CLI 1.2.0 的 Python 环境；构建脚本另外把 Homebrew ARM GCC 8、binutils、GNU Make 加进 PATH |
| `README.md` | 给用户刷机看的入口，发版时同步更新 |

数据流：原项目板级适配 + RC8 无线库（两者本机已删除，需要时按[架构与设计](docs/架构与设计.md#2-源码结构)里的命令重新 clone）→ 本仓库 → `build.sh` 同步进 QMK checkout 编译 → `firmware/`。提交历史按"移植 → ACK 修正 → 功耗 → VIA → via2 → CI"分开，`git log -p` 可看每一步。

## 3. 从哪里读起

| 要查的问题 | 源码入口 |
| --- | --- |
| Fn 键、配对计时、EEPROM、状态灯、诊断、开机恢复 | [bridge75.c](keyboards/shortcut/bridge75/bridge75.c)、[默认键图](keyboards/shortcut/bridge75/ansi_norgb/keymaps/default/keymap.c) |
| USB 与无线切换、HID 报告分发 | [wireless.c](keyboards/shortcut/wireless/wireless.c)、[transport.c](keyboards/shortcut/wireless/transport.c) |
| UART 帧、ACK、重试、报告缓存、重连 | [module.c](keyboards/shortcut/wireless/module.c)、[smsg.c](keyboards/shortcut/wireless/smsg.c) |
| MCU 休眠状态机、STOP 前后的时钟与串口恢复 | [lowpower.c](keyboards/shortcut/wireless/lowpower.c)、[lpwr_wb32.c](keyboards/shortcut/wireless/lpwr_wb32.c) |
| 常量覆盖 | [ansi_norgb/config.h](keyboards/shortcut/bridge75/ansi_norgb/config.h) |
| 测试 | `keyboards/shortcut/wireless/tests/` |

## 4. 排查经验

- **先确认退出了 DFU，再确认刷的是哪一版。** 用户曾因刷完留在 DFU 而无法打字。能打字后按 Fn+D，前缀与文件名对得上才说明刷对了。
- **分清两级休眠。** MCU 约 5 分钟进入 STOP（固件控制）；闭源模块另有 30 分钟睡眠策略，由 Fn+` 开关（`sleep`）。`sleep=0` 时 MCU STOP 照常工作。
- **把日志和实际输入一起看。** 计数只说明发生了什么，首键是否丢、是否延迟要看醒来后实际打出的字。两份日志相减才是那一段的数据。
- **用 UART 计数区分"收不到"和"解析不了"。** `rx` 不涨是供电或接收问题；`rx` 涨而 `valid`/`ack` 不涨是协议或时序问题。拆 `rx` 的公式见[诊断与实机测试](docs/诊断与实机测试.md#3-读-uart-计数)。
- **`fw=0` 不是故障。** 读不到模块版本，但蓝牙照常工作。
- **灯和无线分开验证。** `rgb`/`leds`/`rail` 只说明代码走到哪一步，实际亮不亮要肉眼看 Caps 灯、按住 Fn 时的 Esc 灯和配对灯。
- **沿 QMK 的实际调用顺序排查。** `protocol_post_init()` 在 `keyboard_post_init_kb()` 之后才安装 USB driver，所以保存的无线模式在第一次 housekeeping 恢复；STOP 后矩阵扫描可能先于恢复回调，所以用 `suspend_wakeup_init_quantum()` 保留已扫描的按键；QMK 的 `uart_init()` 只初始化一次，所以 STOP 后用 `sdStop/sdStart`。
- **先复现再改队列。** smsg 队列覆盖的怀疑在百万次模型里没复现；真正的问题是 ACK 时序。改协议前先在 `test_wireless.c` 的 `sim_module` 里复现。
- **ACK 不带序号**，现在的欠账机制是有意为之的取舍（每丢一份让两个报告各晚约 30 ms），不要改回"只晚一次"的方案。
- **切主机时**检查松开帧有没有留在旧主机、旧报告有没有混进新主机。
- **用户可能担心自己的测试误导判断。** 同一种缺陷不一定每次触发，要结合多份日志和模拟测试下结论，并向用户解释清楚。

## 5. 当前进度与待办

- 当前发布版是 via-v1.5-20261121（20261121-0356 构建，git 标签和 GitHub Release 同名），固件行为与 via-v1/via2 相同，尚未实机测试。各版本状态见[更新记录](docs/更新记录.md)。
- 实机待测项（编号见[诊断与实机测试](docs/诊断与实机测试.md#5-实机测试项)）：T1–T4、T9、T10（当前功能）；T5 蓝牙闲置首键（项目最初的问题，via 系列还没测）；T6 2.4G 闲置与主机睡眠（从未测）；T7 主机离开回连；T8 待机耗电；T11 2.4G `drops=76` 的来源；可选 T12 `sleep=1` 对照。
- 旧问题的根因仍未单独证实（模块 30 分钟休眠 vs RC8 复位唤醒），见[架构与设计](docs/架构与设计.md#11-设计取舍与排查结论)。
- 流程上的缺口（固件里没有提交哈希、本地不校验标签等）见[发版规范](docs/发版规范.md#11-已知缺口)。
