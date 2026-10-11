# HSTNS-PD44 源码参考：D1600E 反汇编追踪规则

> 参考仓库：<https://github.com/darwinbeing/hstns-pd44-firmware>
>
> 参考版本：main，读取时树 SHA `879b8fde5198bc6bd472e83360f2616a1245197f`。
>
> **边界：** HSTNS-PD44 是 800 W、dsPIC33FJ64GS606 的另一款电源；D1600E 是独立目标。本文件只复用分析方法和证据组织方式，不复制其 MCU 寄存器地址、RAM 地址、PMBus 命令映射、PWM 参数或控制系数到 D1600E。

## 1. 参考源码中值得跟踪的路径

| 目标 | 参考源码 | 参考符号 / 注释地址 | 对 D1600E 的用途 |
|---|---|---|---|
| I²C/PMBus 接收 | `claude/decompiled/pmbus.c` | `pmbusRxPacketDecode()`、`pmbusCommandDispatcher()`、`_SI2C2Interrupt()` | 对照查找 I²C ISR、收包缓冲区、命令字索引、长度/校验与读写状态机 |
| VOUT 命令处理 | 同上 | `pmbusCmdSetVout()`（源码注释标注参考地址 0x1A12） | 跟踪收到的数据是否写入设定变量，以及该变量是否被周期控制环读取 |
| IOUT/电流限制 | 同上 | `pmbusCmdSetIout()`（参考地址 0x1D12） | 跟踪限制值写入、单位换算、限流逻辑读取和保护分支 |
| 电压闭环 | `claude/decompiled/voltage_loop.c`、`claude/decompiled/isr_t1.c`、`claude/decompiled/isr_t2.c` | `voltageRegulation()`、`pidControl()`、`ocpFoldback()`、`pwmUpdate()` 等 | 对照建立“设定值 → 控制环 → 限幅/保护 → PWM”数据流 |
| 风扇 | `claude/decompiled/monitoring.c`、`claude/decompiled/pmbus.c` | `tempFanHandler()`、`OC2RS` 的参考描述 | 对照查找温度输入、风扇目标/占空比、输出比较外设寄存器及安全状态下的默认值 |
| 启动与保护 | `claude/decompiled/main.c`、`state_machine.c`、`protection.c` | 启动状态机、OVP/OCP、PWM enable/disable | 验证调节命令不会绕过启动条件、故障锁存或硬件关断 |

以上函数名和地址是**参考仓库对 HSTNS-PD44 的反编译注释**，不是 D1600E 地址。

## 2. 不能直接照搬的结论

1. 参考源码中的 PMBus 分发注释出现了 `0x24` → `pmbusCmdSetVout()`、`0x3B` → `pmbusCmdSetIout()` 等映射。这些只能视为该参考项目当前重建的命令路径，必须回到它的原始汇编/实测核对；不能据此推断 D1600E 的命令号。
2. 参考项目有不少反编译重建的 C 注释和人为命名。对 D1600E，只有在真实指令、跳转目标、交叉引用、RAM/SFR 访问和可重复实机观测相互吻合后，才能把函数或变量标成已确认。
3. HSTNS 的 dsPIC33FJ64GS606 外设配置、SFR 地址、PWM 频率、ADC 通道和控制系数均不得直接用于 D1600E。
4. 不能仅因某个 PMBus 写命令能读回，就认定它参与输出控制；必须继续追踪该值的所有写入者和读取者，并确认最终到达控制环或风扇输出路径。

## 3. D1600E 必须完成的证据链

### A. 解码与入口验证
- 确认 D1600E payload 记录格式、24 位指令字节序和 PC 地址单位。
- 实际解码复位入口及中断向量。
- 验证 CALL/GOTO 目标、函数边界和代码/数据区域。
- 输出真实反汇编，而不是把 24 位原始字列表命名为汇编。

### B. PMBus 分发
- 从 I²C 外设初始化和 ISR 入口开始。
- 找到接收缓冲区写入、命令字提取、长度检查、校验、读/写分支及响应构造。
- 每个候选命令都要给出命令比较指令地址、分支目标、处理函数入口和实际数据写入点。
- 对未知/厂商命令保留 unknown 状态，不按标准表补齐。

### C. 电压控制链
`PMBus RX → 命令处理 → 设定值/限值 RAM → 周期控制任务/ISR → 限幅与软启动 → PWM 更新 → OVP/故障关断`

每条箭头必须有反汇编或交叉引用证据；如果命令只改了通信回读缓存，不得称为实际输出设定。

### D. 电流限制链
`PMBus RX → 限流参数 → 电流采样/标定 → 限流或折返算法 → 电压参考/PWM 限制 → OCP 保护`

需区分“遥测电流”“软件限流阈值”“硬件快速过流阈值”，不能把三者混为同一个寄存器。

### E. 风扇控制链
`温度/风扇命令输入 → 风扇目标或模式 → 温度分段/占空比计算 → PWM/输出比较 SFR → 实际风扇响应`

需分别记录命令控制、温控自动控制、故障全速/停机策略；只有找到 SFR 写入及其上游数据依赖后，才确认风扇控制链。

## 4. 输出物与证据等级

- `disassembly.csv`：文件偏移、校准 PC、原始机器码、真实助记符、操作数。
- `functions.csv`：入口/结束地址、调用者、被调用者、置信度。
- `cross_references.csv`：调用/跳转/数据引用的源地址、目标地址和引用类型。
- `pmbus_dispatch_report.md`：命令比较、分支目标、读写函数和 RAM/SFR 数据流。
- `voltage_current_fan_control.md`：三条控制链及证据地址。
- `evidence_summary.csv`：每项结论对应的证据、实测与未决项。

统一证据等级：
- **已验证**：真实反汇编/交叉引用和可重复观测相互支持。
- **高可信推断**：指令与数据流支持，但尚缺独立实测或关键交叉引用。
- **待验证**：名称相似、标准命令编号、单次读回或时间相关性等弱证据。

## 5. 当前 D1600E 状态

目前已有候选 payload 与 24 位候选字记录，但复位入口、指令边界、命令分发和调节控制链仍未完成实际解码验证。因此，D1600E 的电压、电流和风扇控制地址仍应保持“待验证”，不得用 HSTNS 地址填充。
