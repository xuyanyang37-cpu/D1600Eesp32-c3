/*
 * pmbus_peek 的 ESP32/Arduino 移植版本，提供 PMBus 诊断功能。
 * 原始项目：https://github.com/jktjkt/pmbus_peek
 * 原始版权：David Brownell（2008），后由 Jan Kundrat 修改。
 *
 * 本衍生实现按 GPL-3.0-or-later 许可证发布。
 * 本程序不提供任何担保，详细条款请参阅 GNU GPL。
 */
#include "pmbus.h"
#include <string.h>
#include <math.h>

static const uint8_t PMB_CLEAR_FAULT=0x03, PMB_CAPABILITY=0x19,
    PMB_QUERY=0x1A, PMB_VOUT_MODE=0x20, PMB_COEFFICIENTS=0x30,
    PMB_STATUS_BYTE=0x78, PMB_STATUS_WORD=0x79, PMB_STATUS_VOUT=0x7A,
    PMB_STATUS_IOUT=0x7B, PMB_STATUS_INPUT=0x7C, PMB_STATUS_TEMP=0x7D,
    PMB_STATUS_CML=0x7E, PMB_STATUS_OTHER=0x7F, PMB_STATUS_MFR=0x80,
    PMB_STATUS_FANS12=0x81, PMB_STATUS_FANS34=0x82,
    PMB_PMBUS_REVISION=0x98, PMB_MFR_ID=0x99, PMB_MFR_MODEL=0x9A,
    PMB_MFR_REVISION=0x9B, PMB_MFR_LOCATION=0x9C, PMB_MFR_DATE=0x9D,
    PMB_MFR_SERIAL=0x9E, PMB_APP_PROFILES=0x9F, PMB_IC_DEVICE_ID=0xAD,
    PMB_IC_DEVICE_REV=0xAE;

#define F_STATUS 1
#define F_SHOW_P1 2
#define F_VOUT 4

PmbusPeek::PmbusPeek(TwoWire &bus)
 : _wire(bus), _address(0x58), _capability(0), _revision(0),
   _lastError(0), _writesEnabled(false), _queryAvailable(false),
   _scanComplete(false), _queryAllRequested(false), _commandCount(0) {
    initializeCommandTable();
}

bool PmbusPeek::begin(uint8_t address) {
    if (address > 0x7F) { _lastError=-1; return false; }
    _address=address; _lastError=0;
    // I2C 总线由主程序初始化，避免覆盖用户设置的 SDA/SCL 引脚。
    return true;
}
bool PmbusPeek::writeAllowed() const {
    return _writesEnabled;
}
bool PmbusPeek::probe() {
    _wire.beginTransmission(_address);
    uint8_t rc=_wire.endTransmission(true);
    _lastError=rc;
    return rc==0;
}
// 原版只处理单字节 PMBus 命令；扩展命令在原版中返回 ENOSYS，故不实现。
uint8_t PmbusPeek::commandByteCount(uint16_t command) const {
    return command <= 0x00FFU ? 1 : 0;
}
// 发送单字节命令，并以 repeated-start 接续读取。
bool PmbusPeek::selectCommand(uint16_t command) {
    if (commandByteCount(command) != 1) { _lastError=-2; return false; }
    _wire.beginTransmission(_address);
    if (_wire.write((uint8_t)command) != 1) { _lastError=-3; return false; }
    uint8_t rc=_wire.endTransmission(false);
    if (rc) { _lastError=rc; return false; }
    return true;
}
// 读取固定长度数据，不附加或校验软件 PEC。
bool PmbusPeek::readRaw(uint8_t *buffer, size_t count) {
    if (!buffer || !count || count>32) { _lastError=-4; return false; }
    size_t got=_wire.requestFrom((uint8_t)_address, count, (uint8_t)true);
    if (got!=count) { while (_wire.available()) (void)_wire.read(); _lastError=-5; return false; }
    for (size_t i=0;i<count;i++) {
        if (!_wire.available()) { _lastError=-6; return false; }
        buffer[i]=(uint8_t)_wire.read();
    }
    _lastError=0; return true;
}
bool PmbusPeek::readByte(uint16_t command, uint8_t &value) {
    if (!selectCommand(command)) return false;
    return readRaw(&value,1);
}
bool PmbusPeek::readWord(uint16_t command, uint16_t &value) {
    uint8_t b[2];
    if (!selectCommand(command) || !readRaw(b,2)) return false;
    value=(uint16_t)b[0] | ((uint16_t)b[1]<<8);
    return true;
}
// SMBus Block Read。按原版先单独读取长度，再重新发起块读取。
// Arduino Wire 常见接收缓冲区为 32 字节（含长度字节），因此本实现
// 可靠支持最多 31 字节数据；32 字节数据需要底层支持至少 33 字节接收。
bool PmbusPeek::readBlock(uint16_t command, uint8_t *buffer, size_t capacity, size_t &length) {
    length=0;
    if (!buffer || capacity==0 || capacity>32) { _lastError=-7; return false; }

    uint8_t announced=0;
    if (!readByte(command, announced)) return false;
    if (announced==0) { _lastError=0; return true; }
    if (announced>31 || announced>capacity) {
        _lastError=-9; // 当前 Wire 适配层无法安全接收长度字节 + 32 字节数据
        return false;
    }

    // 第二次事务按 SMBus Block Read 格式读取 count + payload；
    // 总字节数不超过 Wire 常见的 32 字节缓冲区。
    if (!selectCommand(command)) return false;
    size_t expected=(size_t)announced+1U;
    size_t got=_wire.requestFrom((uint8_t)_address, expected, (uint8_t)true);
    if (got!=expected || !_wire.available()) {
        while (_wire.available()) (void)_wire.read();
        _lastError=-8;
        return false;
    }
    uint8_t actual=(uint8_t)_wire.read();
    if (actual!=announced) {
        while (_wire.available()) (void)_wire.read();
        _lastError=-11; // 两次读取间长度发生变化，拒绝使用不一致数据
        return false;
    }
    for (size_t i=0;i<announced;i++) {
        if (!_wire.available()) { _lastError=-10; return false; }
        buffer[i]=(uint8_t)_wire.read();
    }
    while (_wire.available()) (void)_wire.read();
    length=announced;
    _lastError=0;
    return true;
}
// 写操作默认锁定。
bool PmbusPeek::writeByte(uint16_t command,uint8_t value) {
    if (!writeAllowed()) { _lastError=-20; return false; }
    if (commandByteCount(command)!=1) { _lastError=-2; return false; }
    _wire.beginTransmission(_address);
    _wire.write((uint8_t)command); _wire.write(value);
    uint8_t rc=_wire.endTransmission(true); _lastError=rc; return rc==0;
}
bool PmbusPeek::writeWord(uint16_t command,uint16_t value) {
    if (!writeAllowed()) { _lastError=-20; return false; }
    if (commandByteCount(command)!=1) { _lastError=-2; return false; }
    _wire.beginTransmission(_address);
    _wire.write((uint8_t)command);
    _wire.write((uint8_t)value); _wire.write((uint8_t)(value>>8));
    uint8_t rc=_wire.endTransmission(true); _lastError=rc; return rc==0;
}
bool PmbusPeek::writeBlock(uint16_t command,const uint8_t *buffer,size_t length) {
    if (!writeAllowed()) { _lastError=-20; return false; }
    if (commandByteCount(command)!=1 || !buffer || !length || length>32) { _lastError=-21; return false; }
    _wire.beginTransmission(_address);
    _wire.write((uint8_t)command); _wire.write((uint8_t)length);
    for(size_t i=0;i<length;i++) _wire.write(buffer[i]);
    uint8_t rc=_wire.endTransmission(true); _lastError=rc; return rc==0;
}
bool PmbusPeek::sendByte(uint8_t command) {
    if (!writeAllowed()) { _lastError=-20; return false; }
    _wire.beginTransmission(_address);
    _wire.write(command);
    uint8_t rc=_wire.endTransmission(true); _lastError=rc; return rc==0;
}
// 按原版 QUERY 的 SMBus Process Call 语义发送：写入 0x01、命令号，返回 0x01、标志字节。
bool PmbusPeek::queryCommand(uint8_t command,uint8_t &queryFlags) {
    _wire.beginTransmission(_address);
    _wire.write(PMB_QUERY);
    _wire.write((uint8_t)0x01);
    _wire.write(command);
    uint8_t rc=_wire.endTransmission(false);
    if(rc) {_lastError=rc; return false;}
    uint8_t b[2];
    if(!readRaw(b,2)) return false;
    uint16_t response=(uint16_t)b[0]|((uint16_t)b[1]<<8);
    if((response&0xFF)!=0x01) {_lastError=-22;return false;}
    queryFlags=(uint8_t)(response>>8); _lastError=0; return true;
}
// 按原版 Block Process Call 请求 COEFFICIENTS；响应长度字节应为 5。
bool PmbusPeek::readCoefficients(uint8_t command,bool forRead,Coefficients &coeff) {
    coeff.valid=false;
    _wire.beginTransmission(_address);
    _wire.write(PMB_COEFFICIENTS);
    _wire.write((uint8_t)2); _wire.write(command); _wire.write(forRead?1:0);
    uint8_t rc=_wire.endTransmission(false);
    if(rc) {_lastError=rc;return false;}
    uint8_t b[6];
    if(!readRaw(b,6)) return false;
    if(b[0]!=5) {_lastError=-23;return false;}
    coeff.m=(int16_t)((uint16_t)b[1]|((uint16_t)b[2]<<8));
    coeff.b=(int16_t)((uint16_t)b[3]|((uint16_t)b[4]<<8));
    coeff.R=(int8_t)b[5]; coeff.valid=true; _lastError=0; return true;
}
bool PmbusPeek::readInventoryString(uint8_t command,char *out,size_t outSize) {
    if(!out || outSize<2) {_lastError=-24;return false;}
    uint8_t b[32]; size_t n=0;
    if(!readBlock(command,b,min((size_t)32,outSize-1),n)) {out[0]=0;return false;}
    for(size_t i=0;i<n;i++) out[i]=(char)b[i];
    out[n]=0; return true;
}
bool PmbusPeek::setPage(uint8_t page) { return writeByte(0x00,page); }
bool PmbusPeek::clearFaults() { return sendByte(PMB_CLEAR_FAULT); }
bool PmbusPeek::issueManufacturerNoData(uint8_t index) {
    if(index>45) {_lastError=-25;return false;}
    return sendByte((uint8_t)(0xD0+index));
}

void PmbusPeek::addCommand(uint16_t cmd,const char *tag,CommandType type,UnitType unit,uint8_t flags) {
    if(_commandCount>=sizeof(_commands)/sizeof(_commands[0])) return;
    CommandInfo &c=_commands[_commandCount++];
    c.command=cmd;c.tag=tag;c.type=type;c.units=unit;c.flags=flags;c.query=0;
    c.writeCoefficients={false,0,0,0};c.readCoefficients={false,0,0,0};
    c.supportedKnown=false;c.supported=true;
}
PmbusPeek::CommandInfo *PmbusPeek::findCommand(uint16_t command) {
    for(size_t i=0;i<_commandCount;i++) if(_commands[i].command==command) return &_commands[i];
    return nullptr;
}
const PmbusPeek::CommandInfo *PmbusPeek::findCommand(uint16_t command) const {
    for(size_t i=0;i<_commandCount;i++) if(_commands[i].command==command) return &_commands[i];
    return nullptr;
}
void PmbusPeek::initializeCommandTable() {
    _commandCount=0;
    addCommand(0x00,"page",CMD_RW1); addCommand(0x01,"operation",CMD_RW1);
    addCommand(0x02,"on_off_config",CMD_RW1); addCommand(0x03,"clear_faults",CMD_W0);
    addCommand(0x04,"phase",CMD_RW1); addCommand(0x05,"page_plus_read",CMD_RWB);
    addCommand(0x10,"write_protect",CMD_RW1);
    addCommand(0x11,"store_default_all",CMD_W0); addCommand(0x12,"restore_default_all",CMD_W0);
    addCommand(0x13,"store_default_code",CMD_W1); addCommand(0x14,"restore_default_code",CMD_W1);
    addCommand(0x15,"store_user_all",CMD_W0); addCommand(0x16,"restore_user_all",CMD_W0);
    addCommand(0x17,"store_user_code",CMD_W1); addCommand(0x18,"restore_user_code",CMD_W1);
    addCommand(0x19,"capability",CMD_R1,UNIT_BITS,F_SHOW_P1);
    addCommand(0x1A,"query",CMD_QUERY); addCommand(0x1B,"smbalert_mask",CMD_RWB);
    addCommand(0x20,"vout_mode",CMD_RW1); addCommand(0x21,"vout_command",CMD_RW2,UNIT_VOLTS,F_VOUT);
    addCommand(0x22,"vout_trim",CMD_RW2,UNIT_VOLTS); addCommand(0x23,"vout_cal_offset",CMD_RW2,UNIT_VOLTS);
    addCommand(0x24,"vout_max",CMD_RW2,UNIT_VOLTS,F_VOUT); addCommand(0x25,"vout_margin_high",CMD_RW2,UNIT_VOLTS,F_VOUT);
    addCommand(0x26,"vout_margin_low",CMD_RW2,UNIT_VOLTS,F_VOUT); addCommand(0x27,"vout_transition_rate",CMD_RW2);
    addCommand(0x28,"vout_droop",CMD_RW2); addCommand(0x29,"vout_scale_loop",CMD_RW2);
    addCommand(0x2A,"vout_scale_monitor",CMD_RW2); addCommand(0x30,"coefficients",CMD_COEFFICIENTS);
    addCommand(0x31,"pout_max",CMD_RW2,UNIT_WATTS); addCommand(0x32,"max_duty",CMD_RW2);
    addCommand(0x33,"frequency_switch",CMD_RW2); addCommand(0x35,"vin_on",CMD_RW2,UNIT_VOLTS);
    addCommand(0x36,"vin_off",CMD_RW2,UNIT_VOLTS); addCommand(0x37,"interleave",CMD_RW2);
    addCommand(0x38,"iout_cal_gain",CMD_RW2); addCommand(0x39,"iout_cal_offset",CMD_RW2,UNIT_AMPERES);
    addCommand(0x3A,"fan_config_1_2",CMD_RW1); addCommand(0x3B,"fan_command_1",CMD_RW2);
    addCommand(0x3C,"fan_command_2",CMD_RW2); addCommand(0x3D,"fan_config_3_4",CMD_RW1);
    addCommand(0x3E,"fan_command_3",CMD_RW2); addCommand(0x3F,"fan_command_4",CMD_RW2);
    const char *names[]={"vout_ov_fault_limit","vout_ov_fault_response","vout_ov_warn_limit","vout_uv_warn_limit","vout_uv_fault_limit","vout_uv_fault_response","iout_oc_fault_limit","iout_oc_fault_response","iout_oc_lv_fault_limit","iout_oc_lv_fault_response","iout_oc_warn_limit","iout_uc_fault_limit","iout_uc_fault_response"};
    for(uint8_t i=0;i<13;i++) addCommand((uint16_t)(0x40+i),names[i],(i==1||i==5||i==7||i==9||i==12)?CMD_RW1:CMD_RW2, (i<=5||i==8)?UNIT_VOLTS:(i>=6&&i<=11?UNIT_AMPERES:UNIT_NONE),(i==0||i==2||i==3||i==4||i==8)?F_VOUT:0);
    addCommand(0x4F,"ot_fault_limit",CMD_RW2,UNIT_DEGREES_C); addCommand(0x50,"ot_fault_response",CMD_RW1);
    addCommand(0x51,"ot_warn_limit",CMD_RW2,UNIT_DEGREES_C); addCommand(0x52,"ut_warn_limit",CMD_RW2,UNIT_DEGREES_C);
    addCommand(0x53,"ut_fault_limit",CMD_RW2,UNIT_DEGREES_C); addCommand(0x54,"ut_fault_response",CMD_RW1);
    const char *vinNames[]={"vin_ov_fault_limit","vin_ov_fault_response","vin_ov_warn_limit","vin_uv_warn_limit","vin_uv_fault_limit","vin_uv_fault_response","iin_oc_fault_limit","iin_oc_fault_response","iin_oc_warn_limit","power_good_on","power_good_off","ton_delay","ton_rise","ton_max_fault_limit","ton_max_fault_response","toff_delay","toff_fall","toff_max_warn_limit"};
    for(uint8_t i=0;i<18;i++){uint8_t cmd=(uint8_t)(0x55+i); bool one=(cmd==0x56||cmd==0x5A||cmd==0x5C||cmd==0x63); addCommand(cmd,vinNames[i],one?CMD_RW1:CMD_RW2,cmd>=0x60?UNIT_MILLISECONDS:(cmd<=0x5A?UNIT_VOLTS:UNIT_AMPERES),(cmd==0x5E||cmd==0x5F)?F_VOUT:0);}
    addCommand(0x68,"pout_op_fault_limit",CMD_RW2,UNIT_WATTS); addCommand(0x69,"pout_op_fault_response",CMD_RW1);
    addCommand(0x6A,"pout_op_warn_limit",CMD_RW2,UNIT_WATTS); addCommand(0x6B,"pin_op_warn_limit",CMD_RW2,UNIT_WATTS);
    addCommand(0x78,"status_byte",CMD_R1,UNIT_BITS,F_STATUS); addCommand(0x79,"status_word",CMD_R2,UNIT_BITS,F_STATUS);
    const char *statusNames[]={"status_vout","status_iout","status_input","status_temperature","status_cml","status_other","status_mfr_specific","status_fans_1_2","status_fans_3_4"};
    for(uint8_t i=0;i<9;i++) addCommand((uint8_t)(0x7A+i),statusNames[i],CMD_R1,UNIT_BITS,F_STATUS);
    addCommand(0x86,"read_ein",CMD_ENERGY); addCommand(0x87,"read_eout",CMD_ENERGY);
    addCommand(0x88,"read_vin",CMD_R2,UNIT_VOLTS); addCommand(0x89,"read_iin",CMD_R2,UNIT_AMPERES);
    addCommand(0x8A,"read_vcap",CMD_R2,UNIT_VOLTS); addCommand(0x8B,"read_vout",CMD_R2,UNIT_VOLTS,F_VOUT);
    addCommand(0x8C,"read_iout",CMD_R2,UNIT_AMPERES);
    addCommand(0x8D,"read_temperature_1",CMD_R2,UNIT_DEGREES_C); addCommand(0x8E,"read_temperature_2",CMD_R2,UNIT_DEGREES_C);
    addCommand(0x8F,"read_temperature_3",CMD_R2,UNIT_DEGREES_C);
    for(uint8_t i=0;i<4;i++){char *n=(char*)""; (void)n; addCommand((uint8_t)(0x90+i),"read_fan_speed",CMD_R2);}
    addCommand(0x94,"read_duty_cycle",CMD_R2); addCommand(0x95,"read_frequency",CMD_R2);
    addCommand(0x96,"read_pout",CMD_R2,UNIT_WATTS); addCommand(0x97,"read_pin",CMD_R2,UNIT_WATTS);
    addCommand(0x98,"pmbus_revision",CMD_R1,UNIT_BITS,F_SHOW_P1);
    addCommand(0x99,"mfr_id",CMD_RWB,UNIT_STRING,F_SHOW_P1); addCommand(0x9A,"mfr_model",CMD_RWB,UNIT_STRING,F_SHOW_P1);
    addCommand(0x9B,"mfr_revision",CMD_RWB,UNIT_STRING,F_SHOW_P1); addCommand(0x9C,"mfr_location",CMD_RWB,UNIT_STRING,F_SHOW_P1);
    addCommand(0x9D,"mfr_date",CMD_RWB,UNIT_STRING,F_SHOW_P1); addCommand(0x9E,"mfr_serial",CMD_RWB,UNIT_STRING,F_SHOW_P1);
    addCommand(0x9F,"app_profile_support",CMD_APP_PROFILE,F_SHOW_P1);
    const char *mf[]={"mfr_vin_min","mfr_vin_max","mfr_iin_max","mfr_pin_max","mfr_vout_min","mfr_vout_max","mfr_iout_max","mfr_pout_max","mfr_tambient_max","mfr_tambient_min","mfr_efficiency_ll","mfr_efficiency_hl","mfr_pin_accuracy","ic_device_id","ic_device_rev"};
    for(uint8_t i=0;i<15;i++) addCommand((uint8_t)(0xA0+i),mf[i],i>=10&&i<=11?CMD_RWB14:(i>=13?CMD_RWB:CMD_R2),i==3||i==7?UNIT_WATTS:(i==2||i==6?UNIT_AMPERES:(i==8||i==9?UNIT_DEGREES_C:UNIT_VOLTS)),i>=13?F_SHOW_P1:0);
    for(uint8_t i=0;i<16;i++){ static const char *u[]={"user_data_00","user_data_01","user_data_02","user_data_03","user_data_04","user_data_05","user_data_06","user_data_07","user_data_08","user_data_09","user_data_10","user_data_11","user_data_12","user_data_13","user_data_14","user_data_15"}; addCommand((uint8_t)(0xB0+i),u[i],CMD_RWB);}
    addCommand(0xC0,"mfr_max_temp_1",CMD_RW2,UNIT_DEGREES_C); addCommand(0xC1,"mfr_max_temp_2",CMD_RW2,UNIT_DEGREES_C); addCommand(0xC2,"mfr_max_temp_3",CMD_RW2,UNIT_DEGREES_C);
    for(uint8_t i=0;i<=45;i++){static const char *unused="mfr_specific"; addCommand((uint8_t)(0xD0+i),unused,CMD_UNKNOWN);}
}
bool PmbusPeek::queryInternal(CommandInfo &info) {
    if(info.command>0xFF) return false;
    uint8_t flags=0;
    if(!queryCommand((uint8_t)info.command,flags)) return false;
    info.query=flags; info.supportedKnown=true; info.supported=(flags&0x80)!=0;
    if(!info.supported) return true;
    if(((flags>>2)&7)==3) {
        if(flags&(1<<5)) readCoefficients((uint8_t)info.command,true,info.readCoefficients);
        if(flags&(1<<6)) readCoefficients((uint8_t)info.command,false,info.writeCoefficients);
    }
    if(info.command==PMB_VOUT_MODE){uint8_t mode=0;if(readByte(PMB_VOUT_MODE,mode)){info.readCoefficients.R=(int8_t)mode;info.readCoefficients.valid=true;info.writeCoefficients.R=(int8_t)mode;info.writeCoefficients.valid=true;}}
    delay(1); return true;
}
bool PmbusPeek::scan(bool queryAllCommands) {
    _queryAvailable=false; _scanComplete=false; _queryAllRequested=queryAllCommands;
    uint8_t q=0;
    if(queryCommand(PMB_QUERY,q)) _queryAvailable=true;
    uint8_t cap=0;
    if(readByte(PMB_CAPABILITY,cap)) _capability=cap;
    uint8_t rev=0;
    if(readByte(PMB_PMBUS_REVISION,rev)) _revision=rev;
    if(queryAllCommands && _queryAvailable) {
        for(size_t i=0;i<_commandCount;i++) {
            if(_commands[i].command==PMB_QUERY) continue;
            queryInternal(_commands[i]);
        }
    }
    _scanComplete=true; return true;
}
const char *PmbusPeek::unitName(UnitType u) {
    switch(u){case UNIT_VOLTS:return "V";case UNIT_AMPERES:return "A";case UNIT_MILLISECONDS:return "ms";case UNIT_DEGREES_C:return "C";case UNIT_WATTS:return "W";case UNIT_BITS:return "bits";case UNIT_STRING:return "string";default:return "";}
}
double PmbusPeek::decodeLinear11(uint16_t raw) {
    int16_t exp=(int16_t)(raw>>11); if(exp&0x10) exp|=(int16_t)0xFFE0;
    int16_t mant=(int16_t)(raw&0x7FF); if(mant&0x400) mant|=(int16_t)0xF800;
    return (double)mant*pow(2.0,exp);
}
double PmbusPeek::decodeLinear16(uint16_t raw,int8_t exponent) { return (double)raw*pow(2.0,exponent); }
double PmbusPeek::decodeDirect(int16_t raw,const Coefficients &c) {
    if(!c.valid || c.m==0) return NAN;
    return ((double)raw*pow(10.0,-c.R)-(double)c.b)/(double)c.m;
}
bool PmbusPeek::readInventory(Stream &out,uint8_t cmd,const char *label) {
    char b[33]; if(!readInventoryString(cmd,b,sizeof(b))) return false;
    if(b[0]){out.print("  ");out.print(label);out.print(": ");out.println(b);return true;}return false;
}
void PmbusPeek::showDeviceInfo(Stream &out) {
    out.print("PMBus device address 0x");out.println(_address,HEX);
    out.print("CAPABILITY=0x");out.println(_capability,HEX);
    out.print("PMBUS_REVISION=0x");out.println(_revision,HEX);
    readInventory(out,PMB_MFR_ID,"Manufacturer"); readInventory(out,PMB_MFR_MODEL,"Model");
    readInventory(out,PMB_MFR_REVISION,"Revision"); readInventory(out,PMB_MFR_LOCATION,"Built at");
    readInventory(out,PMB_MFR_DATE,"Date"); readInventory(out,PMB_MFR_SERIAL,"Serial");
    readInventory(out,PMB_IC_DEVICE_ID,"IC device"); readInventory(out,PMB_IC_DEVICE_REV,"IC revision");
    uint8_t p[32];size_t n=0;
    if(readBlock(PMB_APP_PROFILES,p,sizeof(p),n)&&n>=2){out.println("Application profiles:");for(size_t i=0;i+1<n;i+=2){out.print("  ID ");out.print(p[i]);out.print(" revision ");out.print(p[i+1]>>4);out.print('.');out.println(p[i+1]&0x0F);}}
}
bool PmbusPeek::statusByte(Stream &out,uint8_t cmd,const char *label,const char *const bits[8]) {
    uint8_t v=0;if(!readByte(cmd,v))return false;
    out.print("  ");out.print(label);out.print(" 0x");if(v<16)out.print('0');out.print(v,HEX);out.print(": ");
    bool first=true;for(int i=7;i>=0;i--)if(v&(1<<i)){if(!first)out.print(", ");out.print(bits[i]);first=false;}
    out.println();return true;
}
void PmbusPeek::showStatus(Stream &out) {
    static const char *wordBits[16]={"unspecified","comm/memory/logic","temperature","vin_underflow","iout_overflow","vout_overflow","off","busy","unknown","other","fan","power_good#","mfr","vin","iout","vout"};
    static const char *byteBits[8]={"unspecified","comm/memory/logic","temperature","vin_underflow","iout_overflow","vout_overflow","off","busy"};
    uint16_t w=0;
    if(readWord(PMB_STATUS_WORD,w)){out.print("STATUS_WORD 0x");if(w<0x1000)out.print('0');if(w<0x100)out.print('0');if(w<0x10)out.print('0');out.print(w,HEX);out.print(": ");bool f=true;for(int i=15;i>=0;i--)if(w&(1<<i)){if(!f)out.print(", ");out.print(wordBits[i]);f=false;}out.println();}
    static const char *vout[8]={"VOUT tracking error","TOFF_MAX warning","TON_MAX fault","exceeded VOUT_MAX","undervoltage fault","undervoltage warning","overvoltage warning","overvoltage fault"};
    static const char *iout[8]={"overpower warning","overpower fault","power limiting","current share fault","undercurrent fault","overcurrent warning","overcurrent/low-voltage fault","overcurrent fault"};
    static const char *input[8]={"input overpower warning","input overcurrent warning","input overcurrent fault","insufficient input voltage","input undervoltage fault","input undervoltage warning","input overvoltage warning","input overvoltage fault"};
    static const char *temp[8]={"reserved","reserved","reserved","reserved","overtemp warning","overtemp fault","undertemp warning","undertemp fault"};
    static const char *cml[8]={"logic fault","communication fault","reserved","processor fault","memory fault","PEC error","invalid data","invalid command"};
    static const char *other[8]={"reserved","output OR-ing fault","input B OR-ing fault","input A OR-ing fault","input B fuse fault","input A fuse fault","reserved","reserved"};
    static const char *mfr[8]={"mfr bit 0","mfr bit 1","mfr bit 2","mfr bit 3","mfr bit 4","mfr bit 5","mfr bit 6","mfr bit 7"};
    static const char *fans[8]={"fan 1 fault","fan 1 warning","fan 2 fault","fan 2 warning","fan 1 override","fan 2 override","airflow fault","airflow warning"};
    statusByte(out,PMB_STATUS_BYTE,"STATUS_BYTE",byteBits); statusByte(out,PMB_STATUS_VOUT,"STATUS_VOUT",vout);
    statusByte(out,PMB_STATUS_IOUT,"STATUS_IOUT",iout); statusByte(out,PMB_STATUS_INPUT,"STATUS_INPUT",input);
    statusByte(out,PMB_STATUS_TEMP,"STATUS_TEMPERATURE",temp); statusByte(out,PMB_STATUS_CML,"STATUS_CML",cml);
    statusByte(out,PMB_STATUS_OTHER,"STATUS_OTHER",other); statusByte(out,PMB_STATUS_MFR,"STATUS_MFR_SPECIFIC",mfr);
    statusByte(out,PMB_STATUS_FANS12,"STATUS_FANS_1_2",fans);
}
void PmbusPeek::showValues(Stream &out) {
    out.println("Attribute values (queries are attempted only if QUERY is available):");
    for(size_t i=0;i<_commandCount;i++){
        CommandInfo &c=_commands[i]; if(c.flags&(F_SHOW_P1|F_STATUS))continue;
        if(_queryAllRequested && _queryAvailable && !c.supportedKnown) queryInternal(c);
        if(c.supportedKnown&&!c.supported)continue;
        uint16_t w=0;uint8_t b=0;double val=NAN;bool ok=false;
        if(c.type==CMD_R1 || c.type==CMD_RW1){ok=readByte(c.command,b);val=b;}
        else if(c.type==CMD_R2||c.type==CMD_RW2){ok=readWord(c.command,w);if(ok){
            if(c.flags&F_VOUT){uint8_t mode=0;if(readByte(PMB_VOUT_MODE,mode)&&!(mode&0xE0)){int8_t exp=(int8_t)(mode&0x1F);if(exp&0x10)exp=(int8_t)(exp|0xE0);val=decodeLinear16(w,exp);}else val=w;}
            else if(((c.query>>2)&7)==3)val=decodeDirect((int16_t)w,c.readCoefficients);
            else val=decodeLinear11(w);
        }}
        uint8_t energyRaw[6]={0,0,0,0,0,0};
        if(c.type==CMD_ENERGY) {
            size_t n=0;
            ok=readBlock(c.command,energyRaw,sizeof(energyRaw),n)&&n==sizeof(energyRaw);
            if(ok) {
                // 按原版 ENERGY 格式：byte0..1 为 16 位累加器，byte2 为回绕计数；
                // byte3..5 是 samples 字段，不属于一个简单的 48 位整数。
                uint16_t accumulator=(uint16_t)energyRaw[0]|((uint16_t)energyRaw[1]<<8);
                uint8_t rollovers=energyRaw[2];
                uint8_t format=(uint8_t)((c.query>>2)&7);
                if(format==0) {
                    const double maxLinearCount=33521664.0; // 原版 ((2^10)-1) * 2^15
                    val=(double)rollovers*maxLinearCount+(double)accumulator;
                } else if(format==3 && c.readCoefficients.valid && c.readCoefficients.m!=0) {
                    const Coefficients &cf=c.readCoefficients;
                    double scale=pow(10.0,-cf.R);
                    double maxValue=((double)cf.m*32767.0+(double)cf.b)*scale;
                    double currentValue=((double)accumulator*scale-(double)cf.b)/(double)cf.m;
                    val=(double)rollovers*maxValue+currentValue;
                } else {
                    val=NAN; // 格式或 DIRECT 系数未确认，不能猜测解码
                }
            }
        } else if(c.type!=CMD_R1 && c.type!=CMD_RW1 && c.type!=CMD_R2 && c.type!=CMD_RW2) {
            continue;
        }
        out.print("  0x");out.print(c.command,HEX);out.print(" ");out.print(c.tag);out.print(": ");
        if(!ok)out.print("read failed");
        else {
            out.print("raw=0x");
            if(c.type==CMD_ENERGY) {
                for(size_t k=0;k<sizeof(energyRaw);k++) { if(energyRaw[k]<16)out.print('0');out.print(energyRaw[k],HEX); }
            } else if(c.type==CMD_R1||c.type==CMD_RW1) {
                if(b<16)out.print('0');out.print(b,HEX);
            } else {
                if(w<0x1000)out.print('0');if(w<0x100)out.print('0');if(w<0x10)out.print('0');out.print(w,HEX);
            }
            out.print(" value=");
            if(isnan(val)) out.print("unknown (QUERY format/coefficients unavailable)");
            else out.print(val,4);
            out.print(" ");out.print(unitName(c.units));
        }
        out.println();
    }
}
void PmbusPeek::showCommands(Stream &out) {
    out.println("Command catalogue / QUERY capabilities (r=read, w=write):");
    for(size_t i=0;i<_commandCount;i++){
        CommandInfo &c=_commands[i];if(_queryAllRequested && _queryAvailable && !c.supportedKnown)queryInternal(c);
        if(c.supportedKnown&&!c.supported)continue;
        out.print("  0x");if(c.command<16)out.print('0');out.print(c.command,HEX);out.print(" ");out.print(c.tag);
        out.print(" ");out.print((c.query&(1<<5))?'r':'-');out.print((c.query&(1<<6))?'w':'-');
        out.print(" type=");out.print((int)c.type);out.print(" units=");out.print(unitName(c.units));
        if(c.readCoefficients.valid){out.print(" READ(m=");out.print(c.readCoefficients.m);out.print(",b=");out.print(c.readCoefficients.b);out.print(",R=");out.print(c.readCoefficients.R);out.print(')');}
        if(c.writeCoefficients.valid){out.print(" WRITE(m=");out.print(c.writeCoefficients.m);out.print(",b=");out.print(c.writeCoefficients.b);out.print(",R=");out.print(c.writeCoefficients.R);out.print(')');}
        out.println();
    }
}
void PmbusPeek::showAll(Stream &out,bool values,bool commands) {
    showDeviceInfo(out);showStatus(out);if(values)showValues(out);if(commands)showCommands(out);
}
void PmbusPeek::printKnownReadOnly(Stream &out){showValues(out);}
