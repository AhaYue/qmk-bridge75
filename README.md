# Bridge75 ANSI NoRGB · 官方 QMK 无线固件

为 Shortcut Studio **Bridge75 ANSI NoRGB** 编写的 QMK 固件：基于官方 QMK，支持 USB 有线、蓝牙 1–3 和 2.4G 接收器，并修正原厂固件长时间闲置后首键延迟或丢失的问题。USB 有线模式下可用 VIA 改键。

| 项 | 内容 |
| --- | --- |
| 主控 | WB32FQ95 |
| 无线模块 | CH582F（闭源固件，UART 115200） |
| QMK 基线 | `33292390974c75362ca30873c482d052e0bd7a63` |
| 目标 | `shortcut/bridge75/ansi_norgb`，键图 `default` |
| 许可 | GPL-2.0-or-later |

## 当前状态

| 版本 | Fn+D 前缀 | 状态 |
| --- | --- | --- |
| **via-v1.5-20261121** | `bridge75-via-v1.5-20261121-20261121-0356` | **当前发布版**，见 [GitHub Release](https://github.com/AhaYue/qmk-bridge75/releases/tag/via-v1.5-20261121)。实机待测。功能见下方"功能" |
| ack 20261113-2237 | `bridge75-ack-20261113-2237` | ACK 修正已在 USB、蓝牙、2.4G 实测；没有 VIA、Fn+B，键位为旧布局 |
| diag3 | `bridge75-diag3` | 唯一通过蓝牙 6/35 分钟闲置首键实测的版本，作对照 |

完整版本历史和实测结果见[更新记录](docs/更新记录.md)。固件 `.bin` 不在仓库里，从 GitHub Actions 的构建产物、Release 或维护者处获取。

## 功能

- 三种连接：Fn+1/2/3 蓝牙、Fn+4 2.4G、Fn+5 有线；按住 1 秒配对。
- 无线下无按键 5 分钟后 MCU 休眠，按键唤醒且保留首键。
- Fn+D 输出诊断（构建标识、连接状态、唤醒次数、UART 计数、电量等），Fn+B 输出电量。
- USB 有线模式支持 VIA（8 个自定义键位）；无线模式不支持。
- Windows / Mac 两套布局，Fn+Win 键切换。

## 文档

| 我要… | 看 |
| --- | --- |
| 刷机、查键位、指示灯、用 VIA | [用户指南](docs/用户指南.md) |
| 读 Fn+D 输出、做实机测试、反馈问题 | [诊断与实机测试](docs/诊断与实机测试.md) |
| 了解每个版本改了什么 | [更新记录](docs/更新记录.md) |
| 编译、核对、发布新固件 | [发版规范](docs/发版规范.md) |
| 改代码：协议、低功耗、常量、设计取舍 | [架构与设计](docs/架构与设计.md) |
| 接手项目（本机目录、排查经验） | [AI 交接](AI交接.md) |

## 快速刷机

1. 按住 **Esc** 插入 USB，进入 DFU。
2. 用 QMK Toolbox 刷入 `bridge75_ansi_norgb_<标签>_<时间>.bin`（不带 `_resetwake` 的那个）。
3. 拔插 USB 退出 DFU，在空白文本框按 **Fn+D**，确认输出开头与文件名一致。

详见[用户指南](docs/用户指南.md)。

## 编译

需要官方 QMK checkout（固定基线）、QMK CLI 和 ARM GCC：

```sh
git clone https://github.com/qmk/qmk_firmware.git ../qmk-bridge75-ansi-norgb
git -C ../qmk-bridge75-ansi-norgb checkout 33292390974c75362ca30873c482d052e0bd7a63
git -C ../qmk-bridge75-ansi-norgb submodule update --init --recursive
BRIDGE75_TAG=via-v1.5-20261121 ./build.sh
```

- `build.sh` 把 `keyboards/shortcut/bridge75`、`keyboards/shortcut/wireless`、`tools/bridge75` 同步进 QMK checkout（默认 `../qmk-bridge75-ansi-norgb`，可用 `QMK_DIR` 指定），然后跑 lint、三组主机测试，编译正式版和 `_resetwake` 回退版。
- 产物在 QMK checkout 同级的 `firmware/`：两个 `.bin`、`SHA256SUMS-*`、`logs-*/`。
- **源码以本仓库为准**，QMK checkout 里这三个目录会被覆盖，不要在那边改。
- 也可以在 GitHub Actions 手动运行"构建固件与 VIA 定义"，不需要本机工具链。

只跑主机测试（不需要 ARM 工具链）：

```sh
sh keyboards/shortcut/wireless/tests/run.sh
python3 keyboards/shortcut/wireless/tests/lowpower/run.py
python3 keyboards/shortcut/wireless/tests/board/run.py
```

标签规则、核对清单和发布流程见[发版规范](docs/发版规范.md)。

## 目录

| 路径 | 内容 |
| --- | --- |
| `keyboards/shortcut/bridge75/` | 板级代码、`ansi_norgb` 变体、默认键图 |
| `keyboards/shortcut/wireless/` | 无线协议、传输切换、低功耗；`tests/` 为主机回归测试 |
| `tools/bridge75/build.sh` | 在 QMK checkout 内执行：lint、测试、编译、校验值 |
| `build.sh` | 同步本仓库到 QMK checkout 后调用上面的脚本 |
| `docs/` | 用户与开发文档；`docs/via/` 为 VIA 定义 |
| `.github/workflows/build.yml` | 手动触发的 CI 构建 |

## 许可

GPL-2.0-or-later，与 QMK 一致；各源文件保留原作者版权声明。
