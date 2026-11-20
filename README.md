# Bridge75 ANSI NoRGB · 官方 QMK 无线固件

把 Shortcut Studio Bridge75 ANSI NoRGB 的无线支持（WB32FQ95 主控 + CH582F 无线模块，UART 通信）移植到官方 QMK，并解决拔线后长时间闲置、首次输入延迟或丢键的问题。支持 USB、蓝牙 1–3 和 2.4G 接收器。

接手代码和排障请先读 [AI 交接](AI交接.md)，版本差异见 [更新记录](docs/更新记录.md)。

## 当前状态

| 版本 | 诊断前缀 | 状态 |
| --- | --- | --- |
| via2 20261121-0202 | `bridge75-via2-20261121-0202` | 最新：清理 VIA 占位键位，新增 Fn+B 电量输出，输出不带回车，Fn+1–5 为标准设备切换。待实机测试 |
| via 20261121-0106 | `bridge75-via-20261121-0106` | 已被 via2 取代：仅 USB 有线的 VIA/RAW HID，11 个自定义键位 |
| power 20261113-2349 | `bridge75-power-20261113-2349` | 诊断加入电量，电量查询 30 秒一次，断线重连逐步退避。待实机测试 |
| ack 20261113-2237 | `bridge75-ack-20261113-2237` | ACK 对账修正，已在 USB、蓝牙、2.4G 实测 |
| diag3 | `bridge75-diag3` | 唯一通过蓝牙 6/35 分钟闲置实测的版本 |

## 目录

| 路径 | 内容 |
| --- | --- |
| `keyboards/shortcut/bridge75/` | 板级代码、`ansi_norgb` 变体和默认键图 |
| `keyboards/shortcut/wireless/` | 无线协议、传输切换、低功耗；`tests/` 为主机回归测试 |
| `tools/bridge75/build.sh` | 在 QMK checkout 内运行：lint、测试、编译两个变体、生成校验值 |
| `build.sh` | 把上面三个目录同步进 QMK checkout 后调用构建 |
| `docs/` | 更新记录、各版本说明、分析与验证记录 |

## 编译

需要官方 QMK checkout，基线为 `33292390974c75362ca30873c482d052e0bd7a63`，以及 QMK CLI 和 ARM GCC。

```sh
git clone https://github.com/qmk/qmk_firmware.git ../qmk-bridge75-ansi-norgb
git -C ../qmk-bridge75-ansi-norgb checkout 33292390974c75362ca30873c482d052e0bd7a63
git -C ../qmk-bridge75-ansi-norgb submodule update --init --recursive
BRIDGE75_TAG=power ./build.sh
```

`build.sh` 默认使用同级目录 `../qmk-bridge75-ansi-norgb`，可用 `QMK_DIR=/path/to/qmk_firmware` 指定。输出在 QMK checkout 同级的 `firmware/` 目录，文件名带标签和构建时间，例如 `bridge75_ansi_norgb_power_20261113-2349.bin`，同时生成 `_resetwake` 回退版和 `SHA256SUMS-power-20261113-2349`，日志在 `firmware/logs-power-20261113-2349/`。

**源码以本仓库为准。** 同步会覆盖 QMK checkout 里的 `keyboards/shortcut/bridge75`、`keyboards/shortcut/wireless` 和 `tools/bridge75`，不要在那边改。

只跑主机测试（不需要 ARM 工具链）：

```sh
sh keyboards/shortcut/wireless/tests/run.sh
python3 keyboards/shortcut/wireless/tests/lowpower/run.py
python3 keyboards/shortcut/wireless/tests/board/run.py
```

## GitHub Actions 构建

`.github/workflows/build.yml` 在 GitHub 上跑同一套流程，不需要本机工具链：

1. 仓库 → **Actions** → **构建固件与 VIA 定义** → **Run workflow**，填标签，格式为 `via-v<版本>-<日期>`（例如 `via-v1-20261121`）；标签会写进文件名和 Fn+D 前缀，不代表 VIA 官方版本。
3. 运行结束后在该次运行的 **Artifacts** 里下载 `bridge75-<标签>-<运行号>`，内容为软唤醒版和 `_resetwake` 版 `.bin`、`SHA256SUMS-*`、构建日志和 `bridge75-norgb-via.json`。失败的运行会额外上传日志 artifact。

工作流固定检出 QMK 基线 `33292390974c75362ca30873c482d052e0bd7a63`，使用 Ubuntu 22.04 自带的 ARM GCC 10 和与本机一致的 QMK CLI 1.2.0；构建时间取 runner 的 UTC 时间，以产物文件名为准。私有仓库的 Actions 运行会计入账户的免费分钟数。

## 刷机与验证

按住 **Esc** 插入 USB 进入 DFU，用 QMK Toolbox 刷入，完成后退出 DFU。

1. 插着 USB 确认能打字。在空白文本编辑器按 **Fn+D**，松手后确认输出开头的标签和时间与固件文件名一致。
2. 按 **Fn+B**，确认输出 `bat=NN`，数值与 Fn+D 里的 `bat=` 一致；蓝牙下再按一次。两种输出末尾都不按回车，光标停在输出最后。
3. 按 **Fn+1** 切到蓝牙 1，快速连打、长按后松手，确认没有卡键，再拔掉 USB。
4. 从最后一次按键开始分别闲置 **6 分钟**和 **35 分钟**。每轮醒来先输入 `abcdef123456`，确认没有缺首字或明显延迟，再按 **Fn+D** 保存诊断。

诊断字段的含义见 [AI 交接](AI交接.md) 的排查经验一节。正常时 `tx` 与 `ack` 接近，`retries` 约为丢包数的两倍，`bad=0`。

## 有线 VIA

固件只在 **USB 有线模式**下提供 VIA（RAW HID）；切到蓝牙或 2.4G 时 USB 会断开，VIA 不可用。

1. 用 USB 线连接键盘并切到有线（Fn+5，Fn+D 里 `dev=0`）。
2. 打开 <https://usevia.app>，进入 **Design** 标签，点 **Load Draft Definition**，选择 [`docs/via/bridge75-norgb-via.json`](docs/via/bridge75-norgb-via.json)。
3. 回到 **Configure** 标签选择 `Bridge75 NoRGB`，即可读取和改写键位。定义加载一次后浏览器会记住。via2 的定义只有 8 个自定义键位（蓝牙 1–3、2.4G、USB、诊断、电量、模块休眠）；via 版（11 项）的旧定义不能用于 via2。
4. 按 **Fn+Esc** 清空 EEPROM 会恢复编译进固件的默认键位。

VIA 列表里可能仍有 `Bridge75 2.4G`：那是接收器自己的 raw HID 接口，不是本固件，配置不到键盘。

## 键位要点

| 按键 | 功能 |
| --- | --- |
| Fn+1 / 2 / 3 | 切到蓝牙 1 / 2 / 3，长按 1 秒重新配对 |
| Fn+4 | 切到 2.4G，长按 1 秒重新配对 |
| Fn+5 | 切到有线 USB |
| Fn+` | 切换并保存模块 30 分钟休眠（诊断里的 `sleep`） |
| Fn+D | 输出诊断 |
| Fn+B | 输出电量（`bat=NN`） |
| Fn+左 Win | 切换 Windows / Mac 默认层 |
| Fn+Esc | 清空 EEPROM，回到 USB 模式 |

## 许可

GPL-2.0-or-later，与 QMK 一致；各源文件保留原作者版权声明。
