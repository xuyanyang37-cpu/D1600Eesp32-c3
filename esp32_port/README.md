# jktjkt/pmbus_peek 的 ESP32 移植版

原始项目：https://github.com/jktjkt/pmbus_peek（David Brownell / Jan Kundrat；GPL-3.0-or-later）。本项目是在 Arduino/ESP32 上重新实现诊断流程，并非对 Linux ioctl 代码逐行翻译。

## 已实现的功能
- 字节、字（16 位）、Block 数据的读取；对应的写入接口；不带数据的 SEND BYTE 命令。
- PMBus QUERY 处理调用和 COEFFICIENTS 处理调用接口。
- 读取 CAPABILITY、PMBus 版本、厂商信息字符串和应用配置块。
- 常用标准 PMBus 命令、遥测、状态、厂商命令和用户数据区域的命令表。
- STATUS_WORD、STATUS_BYTE 及状态子寄存器的显示。
- 遥测值显示，以及 LINEAR11、VOUT_MODE/LINEAR16、DIRECT 格式转换辅助函数和系数显示。
- PAGE 选择、CLEAR_FAULTS 和厂商无数据命令。
- 扫描、设备信息、状态、测量值和命令列表显示。
- 写保护：默认禁止写入；只有调用方显式执行 setWritesEnabled(true) 后，写接口才允许发送命令。

## 与原项目的差异及限制
- Linux 命令行参数、/dev/i2c 设备发现、ioctl 能力标志、内核 PEC 协商和 Linux errno 行为，改由类接口及 Arduino Wire 实现。
- 已为 0xFExx 和 0xFFxx 数值命令加入扩展传输路径，按原始 I2C 字节 `[前缀, 子命令]` 发送。原始 Linux 项目对此返回 ENOSYS；这里的字节顺序只是 ESP32 端采用的传输约定，使用前必须对照目标电源协议确认。
- 受 Arduino Wire 缓冲区限制，当前 Block 数据最多 32 字节；原 Linux 版本的 Block 路径可处理最多 255 字节。
- 已为字节/字/Block 读写、SEND BYTE、QUERY 和 COEFFICIENTS 加入可选软件 PEC（CRC-8，多项式 0x07）。PEC 默认关闭；确认目标电源需要 PEC 后再开启。扩展命令的 PEC 规则也取决于具体设备。
- 命令元数据以精简表格表示，未逐条复刻原始项目中所有重复或特殊的描述符。
- QUERY 和 COEFFICIENTS 使用 Process Call 总线序列。Dell/Delta D1600E-S0 固件是否支持尚未确认；不能因为命令属于 PMBus 标准，就断定电源一定实现了它。
- 示例会进行基础 I2C 探测，并显示状态、设备信息、测量值和命令列表。深入扫描可能发出大量 QUERY 事务，不支持的命令可能导致电源产生通信告警，请谨慎执行。
- 默认地址为此前观测到的 7 位地址 0x58。SDA/SCL 引脚需按实际接线修改；请使用合适的电平转换、开漏上拉，并确保信号地共地。
- 尚未完成实机验证。连接电源前，应先使用目标 ESP32 Arduino Core 和开发板配置进行编译测试。

## 基本用法
```cpp
Wire.begin(SDA_PIN, SCL_PIN);
PmbusPeek pmbus(Wire);
pmbus.begin(0x58);
pmbus.scan(false);
pmbus.showAll(Serial, true, true);

// 写操作默认锁定；只有确认安全后才显式解锁：
pmbus.setWritesEnabled(true);
pmbus.writeWord(0x21, rawValue);
```
