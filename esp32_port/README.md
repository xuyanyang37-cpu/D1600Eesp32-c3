# jktjkt/pmbus_peek 的 ESP32 移植版（按原版行为收敛）

原始项目：https://github.com/jktjkt/pmbus_peek

本目录将原版 Linux 工具的诊断流程移植到 Arduino/ESP32 的 `Wire`。本次收敛遵循“只实现原版已有行为，不自行扩展协议”的原则。

## 移植范围
- 单字节 PMBus 命令的 Byte、Word、Block 读写及 SEND BYTE。
- QUERY（`0x1A`）Process Call 事务。
- COEFFICIENTS（`0x30`）Block Process Call 事务。
- CAPABILITY、PMBus 版本、厂商库存字符串、应用配置块。
- 常见遥测、状态寄存器、命令表、LINEAR11 / LINEAR16 / DIRECT 换算。
- PAGE、CLEAR_FAULTS 和原版列出的厂商无数据命令入口。
- 写操作默认禁用，需调用方显式启用。

## 明确移除 / 不实现
- **移除自行增加的软件 PEC 开关和 PEC 处理**。原版对 QUERY、COEFFICIENTS 等特殊事务注明没有实现用户态 PEC；普通事务的 PEC 由 Linux SMBus 适配器处理。Arduino 移植不伪称与原版等价。
- **移除自行增加的 0xFExx / 0xFFxx 两字节扩展命令传输**。原版对扩展命令的相关读写函数返回 `ENOSYS`，因此本移植也不实现。
- 不宣称支持原版未实现的扩展协议或 D1600E-S0 厂商私有协议。
- Arduino `Wire` 缓冲区及实现可能限制 Block 长度；当前接口按最多 32 字节处理，不等同于 Linux 的 I²C 原始事务回退路径。

## 重要限制
- 默认地址为此前观测到的 7 位地址 `0x58`，不是由本程序自动证明的固定地址。
- SDA/SCL 引脚必须按实际接线修改；确保电平兼容、开漏上拉和信号地共地。
- 全量 QUERY 会发送大量事务；应先使用 `scan(false)`，再根据电源响应决定是否进行深入扫描。
- 写操作默认关闭。未验证电源行为前，不要启用写入或执行 PAGE/清故障等可能改变设备状态的命令。
- 尚未完成 ESP32 实际编译及 D1600E-S0 实机验证；Process Call 的 Wire 时序兼容性必须通过逻辑分析仪或实机读取进一步确认。

## 基本用法
```cpp
Wire.begin(SDA_PIN, SCL_PIN);
PmbusPeek pmbus(Wire);
pmbus.begin(0x58);
pmbus.scan(false);
pmbus.showAll(Serial, true, true);
```

原始项目按其仓库中的许可证和版权声明使用；本目录为衍生移植。
