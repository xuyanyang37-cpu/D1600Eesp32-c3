# jktjkt/pmbus_peek 的 ESP32 移植版（按原版行为收敛）

原始项目：https://github.com/jktjkt/pmbus_peek

本目录将原版 Linux 工具的诊断流程移植到 Arduino/ESP32 的 `Wire`。本次收敛遵循“只实现原版已有行为，不自行扩展协议”的原则。

## 移植范围
- 单字节 PMBus 命令的 Byte、Word、Block 读写及 SEND BYTE。
- QUERY（`0x1A`）Process Call 事务。
- COEFFICIENTS（`0x30`）Block Process Call 事务。
- CAPABILITY、PMBus 版本、厂商库存字符串、应用配置块。
- 常见遥测、状态寄存器、命令表、LINEAR11 / LINEAR16 / DIRECT 换算；`READ_EIN` / `READ_EOUT` 按原版的 16 位累加器、回绕计数和 samples 字段解析。
- PAGE、CLEAR_FAULTS 和原版列出的厂商无数据命令入口。
- 写操作默认禁用，需调用方显式启用。

## 明确移除 / 不实现
- **移除自行增加的软件 PEC 开关和 PEC 处理**。原版对 QUERY、COEFFICIENTS 等特殊事务注明没有实现用户态 PEC；普通事务的 PEC 由 Linux SMBus 适配器处理。Arduino 移植不伪称与原版等价。
- **移除自行增加的 0xFExx / 0xFFxx 两字节扩展命令传输**。原版对扩展命令的相关读写函数返回 `ENOSYS`，因此本移植也不实现。
- 不宣称支持原版未实现的扩展协议或 D1600E-S0 厂商私有协议。
- Block Read 先单独读取长度，再重新发起块读取，参照原版的两阶段流程。为适配常见 32 字节 `Wire` 接收缓冲区，当前安全上限为 **31 字节数据 + 1 字节长度**；遇到 32 字节 payload 会明确失败，不会尝试接收 33 字节并假定成功。原版 Linux I²C fallback 的行为并未完全复制。

## 重要限制
- 默认地址为此前观测到的 7 位地址 `0x58`，不是由本程序自动证明的固定地址。
- SDA/SCL 引脚必须按实际接线修改；确保电平兼容、开漏上拉和信号地共地。
- 全量 QUERY 会发送大量事务；应先使用 `scan(false)`，再根据电源响应决定是否进行深入扫描。
- 写操作默认关闭。未验证电源行为前，不要启用写入或执行 PAGE/清故障等可能改变设备状态的命令。
- 已修正移除 PEC 后遗留的构造函数成员初始化，并修正 ENERGY 六字节字段的解码；尚未完成 ESP32 实际编译及 D1600E-S0 实机验证。Process Call 的 Wire 时序兼容性必须通过逻辑分析仪或实机读取进一步确认。

## 基本用法
```cpp
Wire.begin(SDA_PIN, SCL_PIN);
PmbusPeek pmbus(Wire);
pmbus.begin(0x58);
pmbus.scan(false);
pmbus.showAll(Serial, true, true);
```

原始项目按其仓库中的许可证和版权声明使用；本目录为衍生移植。


## PMBus 地址功能对照表（标准命令定义，不代表本电源已验证支持）

> **地址解释：** 下表的 `0xNN` 是 PMBus 命令码；I²C/PMBus 从设备地址是另一回事，本项目默认使用此前观测到的 7 位地址 `0x58`。  
> **读写说明：** R=读，W=写，R/W=标准允许读写；本程序仍默认禁用写操作。命令是否受 D1600E-S0 固件支持，应以 QUERY 响应和实机读取为准。带“厂商定义”的条目不能只凭标准名称断言其私有含义。

### 通信、页面和输出电压：0x00–0x3F

| 命令地址 | 命令名 | 中文功能 / 参数含义 | 标准访问类型 |
|---|---|---|---|
| 0x00 | PAGE | 选择要访问的输出页面/通道 | R/W |
| 0x01 | OPERATION | 输出开关及运行控制 | R/W |
| 0x02 | ON_OFF_CONFIG | 开关机控制来源、极性等配置 | R/W |
| 0x03 | CLEAR_FAULTS | 清除故障状态锁存；会改变设备状态 | W |
| 0x04 | PHASE | 选择相位 | R/W |
| 0x05 | PAGE_PLUS_READ | 指定页面并读取一个命令 | 块读写事务 |
| 0x10 | WRITE_PROTECT | 写保护设置 | R/W |
| 0x11 | STORE_DEFAULT_ALL | 保存全部默认设置到非易失存储 | W |
| 0x12 | RESTORE_DEFAULT_ALL | 恢复全部默认设置 | W |
| 0x13 / 0x14 | STORE_DEFAULT_CODE / RESTORE_DEFAULT_CODE | 保存/恢复指定默认参数 | W |
| 0x15 / 0x16 | STORE_USER_ALL / RESTORE_USER_ALL | 保存/恢复全部用户设置 | W |
| 0x17 / 0x18 | STORE_USER_CODE / RESTORE_USER_CODE | 保存/恢复指定用户参数 | W |
| 0x19 | CAPABILITY | 能力位：总线/协议功能等 | R Byte |
| 0x1A | QUERY | 查询命令是否支持、读写属性及数据格式 | Process Call |
| 0x1B | SMBALERT_MASK | SMBALERT 告警屏蔽位 | 块读写 |
| 0x20 | VOUT_MODE | 输出电压数据格式/指数模式 | R/W Byte |
| 0x21 | VOUT_COMMAND | 输出电压目标设定值；**不是实测电压** | R/W Word |
| 0x22 | VOUT_TRIM | 输出电压微调量 | R/W Word |
| 0x23 | VOUT_CAL_OFFSET | 输出电压校准偏移 | R/W Word |
| 0x24 | VOUT_MAX | 输出电压最大限制值 | R/W Word |
| 0x25 / 0x26 | VOUT_MARGIN_HIGH / LOW | 高/低裕量目标电压 | R/W Word |
| 0x27 | VOUT_TRANSITION_RATE | 输出电压转换速率 | R/W Word |
| 0x28 | VOUT_DROOP | 输出负载线/下垂参数 | R/W Word |
| 0x29 | VOUT_SCALE_LOOP | 输出电压环路缩放系数 | R/W Word |
| 0x2A | VOUT_SCALE_MONITOR | 输出电压监测缩放系数 | R/W Word |
| 0x30 | COEFFICIENTS | DIRECT 格式的 m、b、R 换算系数 | Block Process Call |
| 0x31 | POUT_MAX | 最大输出功率限制 | R/W Word |
| 0x32 | MAX_DUTY | **最大占空比限制/设定值**，不是当前实际占空比 | R/W Word |
| 0x33 | FREQUENCY_SWITCH | 开关频率设置值 | R/W Word |
| 0x35 / 0x36 | VIN_ON / VIN_OFF | 输入电压启动/关闭阈值 | R/W Word |
| 0x37 | INTERLEAVE | 交错相位相关配置 | R/W Word |
| 0x38 | IOUT_CAL_GAIN | 输出电流测量增益校准 | R/W Word |
| 0x39 | IOUT_CAL_OFFSET | 输出电流测量偏移校准 | R/W Word |
| 0x3A / 0x3D | FAN_CONFIG_1_2 / FAN_CONFIG_3_4 | 风扇 1/2、3/4 配置 | R/W Byte |
| 0x3B / 0x3C / 0x3E / 0x3F | FAN_COMMAND_1..4 | 风扇 1–4 控制命令/目标值 | R/W Word |

### 故障阈值、输入输出保护和时序：0x40–0x6B

| 命令地址 | 命令名 | 中文功能 / 参数含义 |
|---|---|---|
| 0x40 | VOUT_OV_FAULT_LIMIT | 输出过压故障阈值 |
| 0x41 | VOUT_OV_FAULT_RESPONSE | 输出过压时的故障响应动作 |
| 0x42 | VOUT_OV_WARN_LIMIT | 输出过压警告阈值 |
| 0x43 | VOUT_UV_WARN_LIMIT | 输出欠压警告阈值 |
| 0x44 | VOUT_UV_FAULT_LIMIT | 输出欠压故障阈值 |
| 0x45 | VOUT_UV_FAULT_RESPONSE | 输出欠压时的故障响应动作 |
| 0x46 | IOUT_OC_FAULT_LIMIT | 输出过流故障阈值 |
| 0x47 | IOUT_OC_FAULT_RESPONSE | 输出过流时的故障响应动作 |
| 0x48 | IOUT_OC_LV_FAULT_LIMIT | 低电压条件下的输出过流故障阈值 |
| 0x49 | IOUT_OC_LV_FAULT_RESPONSE | 低电压条件下过流故障响应 |
| 0x4A | IOUT_OC_WARN_LIMIT | 输出过流警告阈值 |
| 0x4B | IOUT_UC_FAULT_LIMIT | 输出欠流故障阈值 |
| 0x4C | IOUT_UC_FAULT_RESPONSE | 输出欠流故障响应 |
| 0x4F / 0x50 | OT_FAULT_LIMIT / RESPONSE | 过温故障阈值 / 响应动作 |
| 0x51 | OT_WARN_LIMIT | 过温警告阈值 |
| 0x52 | UT_WARN_LIMIT | 欠温警告阈值 |
| 0x53 / 0x54 | UT_FAULT_LIMIT / RESPONSE | 欠温故障阈值 / 响应动作 |
| 0x55 / 0x56 | VIN_OV_FAULT_LIMIT / RESPONSE | 输入过压故障阈值 / 响应 |
| 0x57 | VIN_OV_WARN_LIMIT | 输入过压警告阈值 |
| 0x58 | VIN_UV_WARN_LIMIT | 输入欠压警告阈值 |
| 0x59 / 0x5A | VIN_UV_FAULT_LIMIT / RESPONSE | 输入欠压故障阈值 / 响应 |
| 0x5B / 0x5C | IIN_OC_FAULT_LIMIT / RESPONSE | 输入过流故障阈值 / 响应 |
| 0x5D | IIN_OC_WARN_LIMIT | 输入过流警告阈值 |
| 0x5E / 0x5F | POWER_GOOD_ON / OFF | POWER_GOOD 信号有效/无效阈值 |
| 0x60 | TON_DELAY | 上电延迟时间 |
| 0x61 | TON_RISE | 输出上升时间 |
| 0x62 / 0x63 | TON_MAX_FAULT_LIMIT / RESPONSE | 最大上电时间故障阈值 / 响应 |
| 0x64 | TOFF_DELAY | 关断延迟时间 |
| 0x65 | TOFF_FALL | 输出下降时间 |
| 0x66 | TOFF_MAX_WARN_LIMIT | 最大关断时间警告阈值 |
| 0x68 / 0x69 | POUT_OP_FAULT_LIMIT / RESPONSE | 输出功率过载故障阈值 / 响应 |
| 0x6A | POUT_OP_WARN_LIMIT | 输出功率过载警告阈值 |
| 0x6B | PIN_OP_WARN_LIMIT | 输入功率警告阈值 |

> 上表中的“阈值/响应”是标准命令类别的解释。某些地址在本项目中仅作为候选命令列出；实际设备可能不支持、可能采用厂商私有定义，不能在没有 QUERY/实测依据时写入。

### 状态寄存器与实时遥测：0x78–0x9F

| 命令地址 | 命令名 | 中文功能 / 参数含义 |
|---|---|---|
| 0x78 | STATUS_BYTE | 汇总状态：故障/警告/忙/关断等位 |
| 0x79 | STATUS_WORD | 更完整的汇总状态位 |
| 0x7A | STATUS_VOUT | 输出电压相关故障/警告状态 |
| 0x7B | STATUS_IOUT | 输出电流/功率相关状态 |
| 0x7C | STATUS_INPUT | 输入电压/电流相关状态 |
| 0x7D | STATUS_TEMPERATURE | 温度故障/警告状态 |
| 0x7E | STATUS_CML | 通信、命令、存储器/逻辑错误状态 |
| 0x7F | STATUS_OTHER | 其他状态位 |
| 0x80 | STATUS_MFR_SPECIFIC | 厂商自定义状态位，需专用资料解码 |
| 0x81 / 0x82 | STATUS_FANS_1_2 / 3_4 | 风扇相关状态位 |
| 0x86 | READ_EIN | 输入能量累计读数；带回绕/采样字段，不能简单当成 48 位整数 |
| 0x87 | READ_EOUT | 输出能量累计读数；字段解释同上 |
| 0x88 | READ_VIN | 实测输入电压 |
| 0x89 | READ_IIN | 实测输入电流 |
| 0x8A | READ_VCAP | 电容电压读数 |
| 0x8B | READ_VOUT | **实测输出电压**，与设定值 0x21 不同 |
| 0x8C | READ_IOUT | 实测输出电流 |
| 0x8D / 0x8E / 0x8F | READ_TEMPERATURE_1/2/3 | 温度 1/2/3 实测值；传感器对应关系需实机确认 |
| 0x90 / 0x91 / 0x92 / 0x93 | READ_FAN_SPEED_1/2/3/4 | 风扇 1–4 转速读数；通常按设备定义解释为 RPM |
| 0x94 | READ_DUTY_CYCLE | **实际占空比遥测值**；与 0x32 MAX_DUTY 最大占空比限制不同 |
| 0x95 | READ_FREQUENCY | 实际开关频率读数 |
| 0x96 | READ_POUT | 实际输出功率 |
| 0x97 | READ_PIN | 实际输入功率 |
| 0x98 | PMBUS_REVISION | PMBus 协议版本 |
| 0x99 | MFR_ID | 厂商标识字符串 |
| 0x9A | MFR_MODEL | 厂商型号字符串 |
| 0x9B | MFR_REVISION | 厂商修订版本字符串 |
| 0x9C | MFR_LOCATION | 厂商/生产地点字符串 |
| 0x9D | MFR_DATE | 厂商日期字符串 |
| 0x9E | MFR_SERIAL | 厂商序列号字符串 |
| 0x9F | APP_PROFILE_SUPPORT | 应用配置档案信息；结构需按设备实现解读 |

### 厂商/用户专用区：0xA0–0xFD

| 命令地址 | 当前代码标签 | 当前注释/可信度 |
|---|---|---|
| 0xA0 | MFR_VIN_MIN | 厂商定义的输入电压最小值；需设备资料确认 |
| 0xA1 | MFR_VIN_MAX | 厂商定义的输入电压最大值；需设备资料确认 |
| 0xA2 | MFR_IIN_MAX | 厂商定义的输入电流最大值；需设备资料确认 |
| 0xA3 | MFR_PIN_MAX | 厂商定义的输入功率最大值；需设备资料确认 |
| 0xA4 | MFR_VOUT_MIN | 厂商定义的输出电压最小值；需设备资料确认 |
| 0xA5 | MFR_VOUT_MAX | 厂商定义的输出电压最大值；需设备资料确认 |
| 0xA6 | MFR_IOUT_MAX | 厂商定义的输出电流最大值；需设备资料确认 |
| 0xA7 | MFR_POUT_MAX | 厂商定义的输出功率最大值；需设备资料确认 |
| 0xA8 / 0xA9 | MFR_TAMBIENT_MAX / MIN | 厂商定义的环境温度范围；需设备资料确认 |
| 0xAA / 0xAB | MFR_EFFICIENCY_LL / HL | 厂商定义的低/高负载效率信息；需设备资料确认 |
| 0xAC | MFR_PIN_ACCURACY | 厂商定义的输入功率精度信息；需设备资料确认 |
| 0xAD / 0xAE | IC_DEVICE_ID / IC_DEVICE_REV | PMBus 命令集中常见的芯片设备 ID/修订信息；具体响应格式仍需验证 |
| 0xB0–0xBF | USER_DATA_00..15 | 用户/厂商数据槽位；地址本身不能说明实际含义 |
| 0xC0–0xC2 | MFR_MAX_TEMP_1..3 | 厂商定义的温度最大值；需电源固件/实测确认 |
| 0xD0–0xFD | MFR_SPECIFIC | 厂商专用命令占位符；目前未建立 D1600E 私有命令映射，不应试写 |

## 关键参数的区别

- **0x32 MAX_DUTY**：最大占空比限制/配置类参数。它是可配置命令，是否支持、数据格式和有效范围都必须通过 QUERY、设备资料或安全的只读证据确认。
- **0x94 READ_DUTY_CYCLE**：实际占空比遥测读数。它用于观察运行状态，不等于允许设置的最大占空比。
- **0x21 VOUT_COMMAND**：输出电压目标设定值；**0x8B READ_VOUT**：实测输出电压。不能混用。
- **0x31 POUT_MAX**：最大输出功率限制；**0x96 READ_POUT**：实际输出功率。
- **0x33 FREQUENCY_SWITCH**：开关频率设置；**0x95 READ_FREQUENCY**：实际频率读数。
- **0x1A QUERY**：用于确认命令支持情况和数据格式。LINEAR11、LINEAR16、DIRECT 等解码不能只根据命令名称猜测。
- **0xA0 以后厂商命令**：当前名称是候选标签或标准/常见命名，不是 D1600E-S0 的反编译结论。后续应把原始读取值、QUERY 结果、固件反汇编证据和推测分栏记录。

