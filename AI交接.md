# Bridge75 固件交接

项目目标是把 Shortcut Studio Bridge75 ANSI NoRGB 的无线支持移植到官方 QMK 基线，并解决拔线后无线长时间闲置、首次输入延迟或丢键的问题。键盘主控是 WB32FQ95，无线模块是通过 UART 通信的 CH582F，模块固件闭源。

## 源码仓库与本机目录

本仓库 `bridge75-qmk` 是源码的唯一来源，只包含 `keyboards/shortcut/{bridge75,wireless}`、`tools/bridge75` 和文档，**改代码请在这里改**。编译需要一个官方 QMK checkout（基线 `33292390974c75362ca30873c482d052e0bd7a63`）：仓库根目录的 `build.sh` 会先用 rsync 把这三个目录同步进去，再运行其中的 `tools/bridge75/build.sh`。QMK checkout 里这几个目录会被覆盖，不要在那边改。固件 `.bin` 不进仓库，版本差异见 [更新记录](docs/更新记录.md)。

原作者本机的工作目录如下，仓库克隆在同一层的 `bridge75-qmk/`：

| 工作目录下的文件夹 | 用途 |
| --- | --- |
| `qmk_firmware-em-bridge75/` | 原项目和板级硬件参考。分支 `em-bridge75`，基线 `da9e806e39`。旧目标 `shortcut/bridge75_nolight`，无线代码在 `modules/westberry/wireless`。核对引脚、灯供电极性、键位时从这里找。 |
| `qmk_wireless_rc8/` | 厂商 RC8 无线协议与低功耗参考，基线 `c0f86de489`。无线实现位于 `keyboards/shortcut/wireless`。查模块命令、串口帧、STOP 寄存器时用它对照。 |
| `qmk-bridge75-ansi-norgb/` | 编译用的官方 QMK checkout，`build.sh` 默认同步到这里（可用 `QMK_DIR` 指定别处）。分支 `bridge75-norgb`，官方 QMK 基线 `33292390974c75362ca30873c482d052e0bd7a63`。目标 `shortcut/bridge75/ansi_norgb`，键图 `default`。移植和终版修改仍在工作区，查看整体改动用 `git -C qmk-bridge75-ansi-norgb diff HEAD`，它包含已暂存和未暂存的修改。 |
| `firmware/` | 刷机产物、校验值、补丁、构建和测试日志。旧产物也在这里，先确认文件名和诊断前缀。 |
| `.venv-qmk/` | 本机 QMK CLI 的 Python 环境。构建脚本同时设置 Homebrew ARM GCC 8、binutils 和 GNU Make 的路径。 |
| `bridge75-qmk/` | 本仓库。 |

代码来源是 **原项目的板级适配 + RC8 无线实现 → 移植到本仓库 → `build.sh` 同步进 `qmk-bridge75-ansi-norgb/` 编译 → 输出到 `firmware/`**。三个 QMK 目录是独立 checkout，另两个供对照。提交历史按终版、ACK 修正、功耗改进分开，`git log -p` 可看每一步的改动。整体迁移的历史记录见 [分析与验证](docs/分析与验证.md)，其中关于实机结果的状态以 [终版说明](docs/终版说明.md) 为准。

## 代码从哪里看

| 要查的问题 | 源码入口 |
| --- | --- |
| Fn、配对、EEPROM、状态灯、诊断、开机恢复连接模式 | [bridge75.c](keyboards/shortcut/bridge75/bridge75.c)、[默认键图](keyboards/shortcut/bridge75/ansi_norgb/keymaps/default/keymap.c) |
| USB 与无线切换、HID 报告分发 | [wireless.c](keyboards/shortcut/wireless/wireless.c)、[transport.c](keyboards/shortcut/wireless/transport.c) |
| UART 帧、ACK、重试、报告缓冲和重连 | [module.c](keyboards/shortcut/wireless/module.c)、[smsg.c](keyboards/shortcut/wireless/smsg.c) |
| MCU 休眠状态机、STOP 前后的时钟与串口恢复 | [lowpower.c](keyboards/shortcut/wireless/lowpower.c)、[lpwr_wb32.c](keyboards/shortcut/wireless/lpwr_wb32.c) |

## 排查经验

- **先确认退出 DFU，再确认刷的是哪一版。** 用户曾因刷完留在 DFU 而无法打字。能打字后，在空白编辑器按 Fn+D 并松手，查看 `bridge75-diag3`、`bridge75-final`、`bridge75-ack`（20261113-2227 构建）或 `bridge75-<标签>-<时间>`；脚本构建会把文件名里的标签和时间写进诊断前缀，前缀与文件名对得上才说明刷对了。Fn+D 有输出就能证明 Fn 层有响应，排查灯时仍要肉眼观察。
- **区分两级休眠。** MCU 约 5 分钟进入 STOP。闭源模块另有约 30 分钟的睡眠策略，Fn+` 切换并保存。诊断里的 `sleep=0` 表示 MCU 侧保存的模块休眠选项关闭，MCU STOP 仍工作。拔 USB、保持蓝牙连接，从最后一次按键计时，分别测 6 分钟和 35 分钟。若要隔离旧问题的原因，可用同一固件、同一主机对比 `sleep=0` 和 `sleep=1`，控制其余条件。
- **把日志和用户实际输入一起看。** `wakes` 是本次启动以来的 STOP 唤醒累计次数，`source=1` 是最近一次由键盘矩阵触发，`dev=1 state=2` 表示采样时蓝牙 1 已连接。计数差只说明两次采样间发生了几次唤醒。输入是否完整、首键是否延迟，要看唤醒后的第一次输入。
- **用 UART 计数区分收不到和解析失败。** `rx` 是收取的字节数，`ack` 是匹配到的 ACK，`valid/bad` 是完整帧的校验计数，`tx` 是发送调用计数。实机上曾有 `fw=0` 但蓝牙正常、`ack` 和 `valid` 都增长。两次日志间 `retries` 增加 49，`drops` 和 `bad` 没增加。判断异常应看计数差、连接状态和输入表现。
- **灯光和无线分开验证。** `rgb` 是渲染器状态，`leds` 是回调次数，`rail=0` 是 PB7 的低电平读值，代码将其用作低有效灯供电使能。诊断可用于判断代码走到了哪一步。要确认实际亮灯，观察 Caps 灯、单独按住 Fn 时 Esc 位置的灯，以及配对灯。必要时检查供电与 WS2812 信号。Fn+Esc 会清 EEPROM。
- **沿 QMK 的实际调用顺序排查。** `protocol_post_init()` 安装 USB host driver，所以保存的无线模式在首次 housekeeping 恢复。ChibiOS 已处理 USB resume 回调。无线 STOP 返回后矩阵扫描可能先于延迟恢复回调运行，因此终版恢复外设时调用 `suspend_wakeup_init_quantum()`，保留已扫描的按键和修饰键。STOP 后用 `sdStop/sdStart` 恢复 UART，因为 QMK 的 `uart_init()` 只初始化一次。
- **先复现再改队列。** 原计划怀疑 `smsg` 队列覆盖，当前默认配置下百万次队列模型未复现。已验证的边界是 ACK 在重试耗尽前已到达 RX，旧代码却先处理超时；终版先处理 RX，并在重试状态接受有效 ACK。模块 ACK 不带序号，重发副本的迟到 ACK 会被误算给下一帧；ack 版按发送次数记欠账，先抵欠账再确认新帧，并把重发超时从 10 ms 调到 30 ms，见 [ack 版说明](docs/ack版说明.md)。涉及切主机时，也要检查释放帧是否留在旧主机、旧报告是否混进新主机。

## 已验证的版本与复测

- diag3（`bridge75_ansi_norgb_diag3.bin`） 已实机通过蓝牙 1、USB 拔线、`sleep=0` 下的 6 分钟和 35 分钟测试。两次诊断分别是 `wakes=1 source=1` 和 `wakes=3 source=1`，均为 `state=2 bad=0`。修改前重编结果与这份二进制逐字节一致。
- 终版（`bridge75_ansi_norgb_final.bin`） 已通过编译、QMK lint、UART/队列、低功耗、USB 和板级回归。实机待复测 USB、蓝牙切换及 6/35 分钟首键，建议其中一轮用 Shift+A。此前报告的 Fn 指示灯、2.4 GHz、`sleep=1` 和待机耗电仍缺实测。
- **旧问题的根因仍未单独证实。** 旧固件开启模块 30 分钟休眠，时间与症状吻合。RC8 的复位唤醒路径也可能造成停顿或丢首键。diag3 同时改变了模块策略和 MCU 唤醒路径，因此通过结果只能验证该组合。
- power 版（`bridge75_ansi_norgb_power_20261113-2349.bin`） 是当前源码的构建：诊断多了 `bat=`，电量查询 30 秒一次，断线重连从 500 ms 翻倍到 10 秒封顶，待实机测试。版本差异统一记在 [更新记录](docs/更新记录.md)，以后每次构建在顶部加一条。
- ack 版（`bridge75_ansi_norgb_ack_20261113-2237.bin`） 在终版基础上修正 ACK 对账，诊断前缀为 `bridge75-ack-20261113-2237`。主机回归（含新增的重复 ACK 用例）全部通过，同样待实机测试；除终版的复测项外，还应在蓝牙下快速连打和长按松手，确认没有卡键，并对比 `retries` 增长。之前的 2227 构建在实机 USB 日志里出现过一次连锁重发（第一份丢失后 10 帧全部重发，`tx=20 retries=10 ack=10`），2237 已修正，见 [ack 版说明](docs/ack版说明.md)；正常时 `tx` 与 `ack` 相等、`retries` 为 0 或个位数。
- 若 ack 版或终版出现回归，优先刷回实测通过的 diag3 做对照。另有 终版复位回退构建（`bridge75_ansi_norgb_final_resetwake.bin`），它在 STOP 唤醒时重启 MCU，可能丢首键并清零 RAM 计数。

在本仓库根目录重建与校验（`QMK_DIR` 默认 `../qmk-bridge75-ansi-norgb`）

```sh
BRIDGE75_TAG=power ./build.sh
(cd ../firmware && shasum -a 256 -c SHA256SUMS-power-<本次时间>)
```

脚本会运行 lint 和三组主机回归后输出软唤醒版、复位回退版和校验值，文件名带标签和精确到分钟的构建时间，例如 `bridge75_ansi_norgb_ack_20261113-2237.bin` 与 `SHA256SUMS-ack-20261113-2237`，校验时换成本次输出的文件名。不设 `BRIDGE75_TAG` 时标签为 `final`；终版与更早的产物是加时间戳之前生成的，文件名不带时间。每次构建的日志自动写进 `firmware/logs-<标签>-<时间>/`，不会互相覆盖；终版日志在 `firmware/logs-final/`。各版本源码就是本仓库的提交历史（`firmware/` 里另有旧的补丁文件），刷写步骤见 [README](README.md)。早期排障会话 ID 为 `01a1d421-278d-7101-9522-12624f50a320`。
