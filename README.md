# SmartCar-KeyModes

基于 **STM32F103C8** 的双模式智能小车工程，使用 STM32F10x 标准外设库和 Keil MDK 开发。通过两个按键切换“循迹避障”和“自由避障”模式，OLED 实时显示当前模式及超声波测距信息。

## 主要功能

- **模式 1：循迹避障**
  - 四路底部传感器完成直线行驶、左右修正和定时转弯。
  - 任一前方红外传感器检测到障碍时停车，并驱动蜂鸣器报警。
  - 记录最近一次转向方向；无法匹配的传感器状态会停车。
- **模式 2：自由避障**
  - HC-SR04 测量正前方距离，前方畅通时低速前进。
  - 遇障后停车，舵机带动超声波模块扫描左右方向，再选择较畅通的一侧定时转向。
  - 超声波无回波、超时或测量值无效时停车报警，并在 OLED 显示 `Echo error`。
- **双按键模式选择**
  - 上电后保持停车，按 K1 或 K2 后才启动对应模式。
  - 20 ms 消抖；长按只触发一次；同时按下两个按键会被忽略。
  - 运行中切换模式时先停车、舵机回正，再执行新模式；扫描和分段等待期间也可响应按键。
- **OLED 状态显示**：显示模式编号、模式名称、正前方距离及左右扫描距离。

## 默认控制参数

参数集中在 `User/main.c` 顶部，实车使用前可按底盘和支架情况调整。

| 参数 | 默认值 | 含义 |
|---|---:|---|
| `HEAD_CENTER` | 90° | 舵机正前方角度 |
| `HEAD_LEFT` | 120° | 舵机向左扫描角度 |
| `HEAD_RIGHT` | 60° | 舵机向右扫描角度 |
| `STOP_DISTANCE` | 25 cm | 自由避障模式的停车距离阈值 |
| `HEAD_WAIT_MS` | 500 ms | 舵机转动后的稳定等待时间 |
| `TURN_TIME_MS` | 300 ms | 自由避障模式的定时转向时间 |

> 左右扫描角度与支架安装方向有关；若实物方向相反，请交换 `HEAD_LEFT` 和 `HEAD_RIGHT`。定时转向时间不对应固定转角，需要实车校准。

## 硬件与接线

### 主控及模块

- STM32F103C8（Cortex-M3）
- 四路底部循迹传感器、两路前方红外避障传感器
- HC-SR04 超声波模块及舵机支架
- 双路直流电机驱动板、左右直流电机
- 四行 OLED（软件 I²C，从机写地址 `0x78`）
- 两个常开按键、蜂鸣器

### 引脚分配

以下表格按当前源码整理。

| 功能 | STM32 引脚 | 说明 |
|---|---|---|
| K1 | PA9 | 上拉输入，按键另一端接 GND，低电平按下 |
| K2 | PA10 | 上拉输入，按键另一端接 GND，低电平按下 |
| 左电机 PWM | PA1 / TIM2_CH2 | 占空比范围 0～100 |
| 左电机方向 | PA4、PA5 | 由 `Motor.c` 控制 |
| 右电机 PWM | PA2 / TIM2_CH3 | 占空比范围 0～100 |
| 右电机方向 | PA6、PA7 | 由 `Motor.c` 控制 |
| 舵机信号 | PB1 / TIM3_CH4 | 50 Hz PWM |
| 超声波 Trig | PB14 | 推挽输出 |
| 超声波 Echo | PB15 | 浮空输入，必须分压后接入 |
| OLED SCL | PB8 | 软件 I²C |
| OLED SDA | PB9 | 软件 I²C |
| 循迹 L1 / L2 | PB10 / PB11 | 上拉输入 |
| 循迹 R1 / R2 | PB12 / PB13 | 上拉输入 |
| 前方红外左 / 右 | PC14 / PC15 | 上拉输入，低电平表示检测到障碍 |
| 蜂鸣器 | PA8 | 当前代码按高电平触发 |

**接线注意事项：**

1. 按键使用 **PA9、PA10**，当前 `Key.c` 的引脚配置和注释已保持一致。
2. PA9、PA10 同时是 USART1 的 TX、RX。当前 `main.c` 未调用 `Serial_Init()`；使用本按键方案时不要同时启用 `Hardware/Serial.c` 的 USART1 功能。
3. HC-SR04 使用 5 V 供电时，Echo 推荐按现有说明分压：`Echo → 10 kΩ → PB15`，`PB15 → 15 kΩ → GND`，使高电平约为 3 V。
4. 舵机应按自身额定电压使用外部电源，电机驱动也应使用合适电源；主控、传感器、舵机和驱动板必须共地。PB1 只接舵机信号线。
5. 四脚轻触按键应使用按下才导通的两个端子，可先用万用表通断档确认。

## 使用方法

| 操作 | OLED | 小车行为 |
|---|---|---|
| 上电 | `Mode:1`，提示 `Press K1 or K2` | 停车等待按键 |
| 按 K1 | `Mode:1` / `Line tracking` | 执行循迹，前方红外检测到障碍时停车报警 |
| 按 K2 | `Mode:2` / `Free avoidance` | 执行超声波自由避障 |
| 运行中按另一模式键 | 显示新模式 | 立即停车、舵机回正，等待后进入新模式 |
| 长按同一按键 | 保持当前模式 | 一次按下只处理一次 |
| 同时按 K1、K2 | 保持当前模式 | 忽略本次输入，松开后再选择 |

模式 1 使用前方红外作为停车保护；模式 2 使用超声波扫描左右方向。丢线不会自动切换模式，自由避障也不是连续路径规划算法。

## 构建与烧录

### 开发环境

- Keil MDK 5
- ARM Compiler 5（工程记录版本为 5.06 update 5）
- Keil STM32F1xx Device Family Pack（工程记录为 2.2.0）
- 与下载器匹配的调试工具，例如 ST-Link

### 构建步骤

1. 用 Keil 打开根目录的 `Project.uvprojx`。
2. 在 `Options for Target → C/C++ → Define` 中确认包含：
   ```text
   STM32F10X_MD,USE_STDPERIPH_DRIVER
   ```
   若编译提示 `Please select first the target STM32F10x device`，说明缺少 `STM32F10X_MD` 宏。
3. 执行 **Rebuild**，生成的 HEX 文件位于 `Objects/Project.hex`。
4. 选择正确的下载器和 Flash 算法后烧录到 STM32F103C8。

`Objects/`、`Listings/` 等编译输出及 `*.uvoptx`、`*.uvguix.*` 用户配置已通过 `.gitignore` 排除，克隆后需在本机重新构建。

## 目录结构

```text
SmartCar-KeyModes/
├── Hardware/              # 按键、电机、PWM、OLED、超声波、舵机等驱动
├── Library/               # STM32F10x 标准外设库
├── Start/                 # CMSIS、启动文件和系统时钟配置
├── System/                # 延时函数
├── User/                  # main.c、中断及库配置
├── Tests/                 # ARMCC 编译与 Cortex-M3 模拟测试脚本
├── Project.uvprojx        # Keil 工程文件
├── 按键与模式讲解.md       # 补充学习说明
└── README.md
```

核心控制流程位于 `User/main.c`：`LineStep()` 实现循迹，`FreeStep()` 实现自由避障，`CheckModeKey()` 和 `WaitWithKey()` 负责模式切换及可中断等待。

## 测试说明

`Tests/test_modes.c` 直接包含实际的 `main.c` 和 `Key.c`，通过 mock 硬件接口测试按键消抖、长按、双键、模式切换、障碍停车及左右择路等逻辑。`Tests/run_emulated.py` 使用 Unicorn 执行已链接的 Cortex-M3 测试程序。

运行脚本需要本机 ARMCC、Python，以及 `unicorn`、`pyelftools`：

```powershell
python -m pip install unicorn pyelftools
powershell -ExecutionPolicy Bypass -File .\Tests\run_tests.ps1 -CompilerRoot "D:\Keil_v5\ARM\ARMCC"
```

请按本机 Keil 安装路径修改 `CompilerRoot`。修改 GPIO 后还需同步测试中的硬件 mock；当前 `test_modes.c` 的按键 mock 仍使用 `GPIO_Pin_11` 映射，与当前 PA9/PA10 按键实现不一致，运行按键测试前应先校正。模拟测试不能验证真实电气连接、电机方向、舵机角度、制动距离或传感器安装效果。

补充说明可参阅 `按键与模式讲解.md`；若其中引脚描述与当前源码不一致，以本 README 的引脚表和当前源码为准。

## 实车调试建议

1. 首次上电前让车轮悬空，确认左右电机方向、停车逻辑和按键模式没有异常。
2. 低速验证循迹传感器的排列与高低电平逻辑，再调整转弯速度和 700 ms 定时转弯参数。
3. 检查舵机左右方向、超声波安装高度和分压接线，然后校准扫描角度、停车距离和转向时间。
4. 最后在空旷区域低速测试，逐步验证障碍报警、无回波处理和运行中切换模式。

## 许可证

本仓库采用 [MIT License](LICENSE)。
