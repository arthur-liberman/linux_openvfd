# linux_openvfd

[English](#english) | [中文](#中文)

---

## English

This repository contains the Linux kernel driver and userspace service for FD628 and similar LED/LCD display controllers commonly found on the front panel of Android TV boxes and ARM-based NAS devices.

- [FD628 / PT6964 Controller Datasheet](http://pdf1.alldatasheet.com/datasheet-pdf/view/232882/PTC/PT6964.html)
- Driver version: **V1.4.4**

### Supported Controllers

| Controller | Interface | Type |
|-----------|-----------|------|
| FD628 | SPI (SW) | 7-segment LED, 4–7 digits |
| FD620 | SPI (SW) | 7-segment LED, 4–5 digits |
| TM1618 | SPI (SW) | 7-segment LED, 4–7 digits |
| FD650 | I²C variant | 7-segment LED |
| HBS658 | SPI (SW) | 7-segment LED |
| FD655 / FD6551 | SPI (SW) | 7-segment LED |
| SSD1306 / SH1106 | I²C (HW/SW) | OLED graphic display |
| PCD8544 | SPI (SW) | 84×48 graphic LCD (Nokia 5110) |
| IL3829 | SPI (SW) | E-Paper display |
| HD44780 | I²C backpack | Character LCD (16×2, 20×4…) |

### Supported Display Types (7-segment)

| Value | Name | Device examples |
|-------|------|-----------------|
| `0x00` | 5D_7S_NORMAL | T95U |
| `0x01` | 5D_7S_T95 | T95K |
| `0x02` | 5D_7S_X92 | X92 |
| `0x03` | 5D_7S_ABOX | A-Box |
| `0x04` | FD620_REF | FD620 reference board |
| `0x05` | 4D_7S_COL | 4-digit + colon |
| `0x06` | 5D_7S_M9_PRO | M9 Pro |
| `0x07` | 5D_7S_G9SX | G9SX |
| `0x08` | 4D_7S_FREESATGTC | Freesat GTC |
| `0x09` | 5D_7S_TAP1 | TAP1 |
| `0x0A` | 5D_X96_X9 | X96/X9 |

### Display Modes

- **CLOCK** — Time (HH:MM), 12h/24h selectable, blinking colon
- **DATE** — Date (DD.MM or MM.DD), configurable format
- **TEMPERATURE** — CPU temperature (°C), auto-ranging
- **CHANNEL** — Channel number
- **TITLE** — Scrolling text string
- **PLAYBACK_TIME** — Media playback time (MM:SS or HH:MM)

### OpenVFDService — Userspace Daemon

`OpenVFDService` is the display daemon that drives the kernel module via `/dev/openvfd`.

```
Usage: vfdservice [OPTIONS] <display_type>

Options:
  -carousel       Enable carousel mode (CLOCK → DATE → TEMP rotation)
  -cd S,S,S       Carousel durations in seconds (CLOCK,DATE,TEMP). Default: 5,3,3
  -mdf            Date format: month-day-first (MM.DD). Default is DD.MM
  -12h            Use 12-hour clock format
  -s  <string>    Display a fixed text string
  -ss <string>    Secondary string (e.g. label)
  -dt <hex>       Set display type (e.g. -dt 0x01000000)
  -dm             Demo mode: cycle through all display modes
  -t              Test mode: hardware LED test pattern
  -v              Verbose output
```

**Carousel mode** automatically rotates between Clock, Date, and CPU Temperature,
with individually configurable durations. Set any duration to `0` to skip that state.

**CPU temperature** is read from `/sys/class/thermal/thermal_zone0~7`, taking the
highest reported value to handle SoCs where CPU temp is not in zone0.

### fnnas-openvfd — Control Script

`fnnas-openvfd` is a shell wrapper for managing `OpenVFDService` on FnNAS / Armbian-based systems.

```bash
# Start display for a known device (box ID 99 = GT-King Pro)
fnnas-openvfd -c -cd 5,3,3 99

# Interactive configuration and autostart setup
fnnas-openvfd
```

Key options:

| Flag | Description |
|------|-------------|
| `-c` / `--carousel` | Enable carousel mode |
| `-cd N,N,N` | Set carousel durations (seconds) |
| `-a` / `--autostart` | Configure autostart in startup script |

### Building the Kernel Module

```bash
# Cross-compile for arm64
cd driver
make KERNELDIR=/path/to/kernel/source

# Or native compile (on target board)
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

### Configuration — vfd.conf

Copy `vfd.conf_sample-t95m` to `/storage/.config/vfd.conf` and edit:

```bash
vfd_gpio_clk='1,5,0'       # CLK GPIO: bank,pin,reserved
vfd_gpio_dat='1,4,0'       # DAT GPIO
vfd_gpio_stb='1,3,0'       # STB GPIO
vfd_chars='0,1,2,3,4'      # Digit slot mapping: dots,d1,d2,d3,d4
vfd_dot_bits='0,1,2,3,4,5,6'  # Indicator LED bit order
vfd_display_type='0x01,0x00,0x00,0x00'  # type,reserved,flags,controller
```

`vfd_display_type` byte breakdown:
- Byte 0: Display type (see table above)
- Byte 1: Reserved (`0x00`)
- Byte 2: Flags (`0x01` = common anode, `0x40` = low frequency)
- Byte 3: Controller (`0x00`=FD628, `0x01`=FD620, `0x02`=TM1618, `0x03`=FD650…)

---

## 中文

本仓库包含 FD628 及同类 LED/LCD 显示控制器的 Linux 内核驱动和用户态服务程序，广泛应用于安卓电视盒子和 ARM 架构 NAS 设备的前面板显示。

- [FD628 / PT6964 控制器数据手册](http://pdf1.alldatasheet.com/datasheet-pdf/view/232882/PTC/PT6964.html)
- 驱动版本：**V1.4.4**

### 支持的控制器

| 控制器 | 通信接口 | 显示类型 |
|--------|----------|----------|
| FD628 | SPI（软件模拟）| 7段数码管，4–7位 |
| FD620 | SPI（软件模拟）| 7段数码管，4–5位 |
| TM1618 | SPI（软件模拟）| 7段数码管，4–7位 |
| FD650 | I²C 变体协议 | 7段数码管 |
| HBS658 | SPI（软件模拟）| 7段数码管 |
| FD655 / FD6551 | SPI（软件模拟）| 7段数码管 |
| SSD1306 / SH1106 | I²C（硬件/软件）| OLED 图形屏 |
| PCD8544 | SPI（软件模拟）| 84×48 图形 LCD（Nokia 5110）|
| IL3829 | SPI（软件模拟）| 电子墨水屏 |
| HD44780 | I²C 转接板 | 字符型 LCD（16×2、20×4 等）|

### 支持的显示类型（7段数码管）

| 值 | 名称 | 适用设备 |
|----|------|----------|
| `0x00` | 5D_7S_NORMAL | T95U |
| `0x01` | 5D_7S_T95 | T95K |
| `0x02` | 5D_7S_X92 | X92 |
| `0x03` | 5D_7S_ABOX | A-Box |
| `0x04` | FD620_REF | FD620 参考板 |
| `0x05` | 4D_7S_COL | 4位数字 + 冒号 |
| `0x06` | 5D_7S_M9_PRO | M9 Pro |
| `0x07` | 5D_7S_G9SX | G9SX |
| `0x08` | 4D_7S_FREESATGTC | Freesat GTC |
| `0x09` | 5D_7S_TAP1 | TAP1 |
| `0x0A` | 5D_X96_X9 | X96/X9 |

### 显示模式

- **时钟（CLOCK）** — 显示时间（HH:MM），支持 12/24 小时制，冒号闪烁
- **日期（DATE）** — 显示日期（DD.MM 或 MM.DD），格式可配置
- **温度（TEMPERATURE）** — 显示 CPU 温度（°C），自动适配位数
- **频道（CHANNEL）** — 显示频道号
- **标题（TITLE）** — 显示自定义文字
- **播放时间（PLAYBACK_TIME）** — 显示媒体播放进度（MM:SS 或 HH:MM）

### OpenVFDService — 用户态显示守护进程

`OpenVFDService`（即 `vfdservice`）通过 `/dev/openvfd` 驱动内核模块，是实际控制显示内容的守护进程。

```
用法：vfdservice [选项] <display_type>

选项说明：
  -carousel       启用走马灯模式（时钟 → 日期 → 温度 轮播）
  -cd S,S,S       各阶段持续时间（秒），格式：时钟,日期,温度，默认 5,3,3
                  某项设为 0 表示跳过该阶段
  -mdf            日期格式：月在前（MM.DD），默认为日在前（DD.MM）
  -12h            使用 12 小时制
  -s  <字符串>    显示固定文字
  -ss <字符串>    辅助字符串（如标签）
  -dt <十六进制>  设置显示类型（例：-dt 0x01000000）
  -dm             演示模式：循环展示所有显示模式
  -t              测试模式：硬件 LED 点亮测试
  -v              详细输出
```

**走马灯模式**：自动在时钟、日期、CPU 温度三个状态之间轮转，每个状态的持续时间可单独设置。

**CPU 温度读取**：依次扫描 `/sys/class/thermal/thermal_zone0~7`，取最高值，兼容 CPU 温度不在 zone0 的 SoC。

### fnnas-openvfd — 控制脚本

`fnnas-openvfd` 是专为 FnNAS / Armbian 系统设计的 shell 封装脚本，用于管理 `OpenVFDService`。

```bash
# 启动指定设备的显示（boxid 99 = GT-King Pro，走马灯模式，时长 5,3,3 秒）
fnnas-openvfd -c -cd 5,3,3 99

# 交互式配置（含开机自启设置）
fnnas-openvfd
```

常用参数：

| 参数 | 说明 |
|------|------|
| `-c` / `--carousel` | 启用走马灯模式 |
| `-cd N,N,N` | 设置走马灯各阶段时长（秒）|
| `-a` / `--autostart` | 配置开机自启（写入启动脚本）|

> **注意**：`fnnas-openvfd` 使用 `-c` 参数启用走马灯，该参数内部会转换为传给 `vfdservice` 的 `-carousel` 参数。
> 请勿直接在调用 `fnnas-openvfd` 时使用 `-carousel`，否则会被识别为未知参数，导致进入交互菜单。

### 编译内核模块

```bash
# 交叉编译（目标架构 arm64）
cd driver
make KERNELDIR=/path/to/kernel/source

# 在目标板上本地编译
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

### 配置文件 — vfd.conf

将 `vfd.conf_sample-t95m` 复制为 `/storage/.config/vfd.conf` 并按实际硬件修改：

```bash
vfd_gpio_clk='1,5,0'              # CLK 引脚：GPIO bank, pin, reserved
vfd_gpio_dat='1,4,0'              # DAT 引脚
vfd_gpio_stb='1,3,0'              # STB 引脚（片选）
vfd_chars='0,1,2,3,4'             # 数字槽位映射：dots,位1,位2,位3,位4
vfd_dot_bits='0,1,2,3,4,5,6'      # 指示灯 bit 顺序
vfd_display_type='0x01,0x00,0x00,0x00'  # 类型,保留,标志位,控制器
```

`vfd_display_type` 各字节含义：
- 字节 0：显示类型（见上表）
- 字节 1：保留，固定填 `0x00`
- 字节 2：标志位（`0x01` = 共阳极，`0x40` = 低频模式）
- 字节 3：控制器型号（`0x00`=FD628，`0x01`=FD620，`0x02`=TM1618，`0x03`=FD650，`0xFC`=SSD1306，`0xFE`=HD44780）
