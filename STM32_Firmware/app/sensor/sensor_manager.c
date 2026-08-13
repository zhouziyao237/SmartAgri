#include "sensor_manager.h"
#include "board.h"
#include "debug_log.h"
#include <string.h>

static sa_node_config_t g_cfg;
static sensor_telemetry_cb_t g_cb;
typedef struct {uint32_t last_sample;uint32_t last_report;uint8_t initialized;uint8_t last_gpio;uint32_t pulse_count;} slot_state_t;
static slot_state_t st[SA_MAX_SENSOR_SLOTS];

static uint8_t crc8_sensirion(const uint8_t*d,int n){uint8_t c=0xFF;for(int i=0;i<n;i++){c^=d[i];for(int b=0;b<8;b++)c=(c&0x80)?(uint8_t)((c<<1)^0x31):(uint8_t)(c<<1);}return c;}
static int32_t apply_scale(const sa_sensor_slot_config_t*s,int32_t q){int64_t v=(int64_t)q*s->scale_q16;if(s->scale_q16==0)v=(int64_t)q<<16;return (int32_t)(v>>16)+s->offset_q16;}
static void add(sa_telemetry_payload_t*t,uint16_t m,float v,uint8_t q){if(t->count>=4)return;t->values[t->count].measurement_id=m;t->values[t->count].value_q16=sa_float_to_q16(v);t->values[t->count].quality=q;t->count++;}

const char*SensorManager_DriverName(uint16_t d){switch(d){case SA_DRV_GPIO_INPUT:return"GPIO_INPUT";case SA_DRV_ADC_RAW:return"ADC_RAW";case SA_DRV_ADC_LINEAR:return"ADC_LINEAR";case SA_DRV_4_20MA:return"4_20MA";case SA_DRV_0_10V:return"0_10V";case SA_DRV_DHT11:return"DHT11";case SA_DRV_DHT22:return"DHT22";case SA_DRV_DS18B20:return"DS18B20";case SA_DRV_SHT3X:return"SHT3X";case SA_DRV_BME280:return"BME280";case SA_DRV_BH1750:return"BH1750";case SA_DRV_SCD4X:return"SCD4X";case SA_DRV_ADS1115:return"ADS1115";case SA_DRV_I2C_GENERIC:return"I2C_GENERIC";case SA_DRV_MODBUS_GENERIC:return"MODBUS_GENERIC";case SA_DRV_PULSE_COUNTER:return"PULSE_COUNTER";default:return"NONE";}}

bool SensorManager_ValidateSlot(const sa_sensor_slot_config_t*s,uint32_t*detail){if(detail)*detail=0;if(!s||s->slot_id>=SA_MAX_SENSOR_SLOTS)return false;if(!s->enabled)return true;if(s->sample_interval_ms<10){if(detail)*detail=1;return false;}uint32_t cap=Board_PinCapabilities(s->pin_id);
    switch(s->driver_id){
        case SA_DRV_GPIO_INPUT: if(!(cap&SA_PINCAP_GPIO_IN)){if(detail)*detail=2;return false;}break;
        case SA_DRV_ADC_RAW:case SA_DRV_ADC_LINEAR:case SA_DRV_4_20MA:case SA_DRV_0_10V:if(!(cap&SA_PINCAP_ADC)){if(detail)*detail=3;return false;}break;
        case SA_DRV_DHT11:case SA_DRV_DHT22:if(!(cap&SA_PINCAP_DHT)){if(detail)*detail=4;return false;}break;
        case SA_DRV_DS18B20:if(!(cap&SA_PINCAP_ONEWIRE)){if(detail)*detail=5;return false;}break;
        case SA_DRV_PULSE_COUNTER:if(!(cap&SA_PINCAP_PULSE)){if(detail)*detail=6;return false;}break;
        case SA_DRV_SHT3X:case SA_DRV_BME280:case SA_DRV_BH1750:case SA_DRV_SCD4X:case SA_DRV_ADS1115:case SA_DRV_I2C_GENERIC:break;
        case SA_DRV_MODBUS_GENERIC:break;
        default:if(detail)*detail=7;return false;
    }return true;}
bool SensorManager_ValidateConfig(const sa_node_config_t*c,uint32_t*detail){if(!c)return false;uint32_t used=0;for(unsigned i=0;i<SA_MAX_SENSOR_SLOTS;i++){const sa_sensor_slot_config_t*s=&c->slots[i];uint32_t d;if(!SensorManager_ValidateSlot(s,&d)){if(detail)*detail=(i<<16)|d;return false;}if(s->enabled&&s->pin_id){uint32_t bit=1u<<(s->pin_id&31u);if(used&bit){if(detail)*detail=(i<<16)|8;return false;}if(!(Board_PinCapabilities(s->pin_id)&SA_PINCAP_RESERVED))used|=bit;}}return true;}

/* ---------- 1-Wire DS18B20 ---------- */
static void ow_mode(uint16_t id,bool out){GPIO_TypeDef*p;uint16_t n;if(!Board_PinResolve(id,&p,&n))return;GPIO_InitTypeDef g={0};g.Pin=n;g.Pull=GPIO_PULLUP;g.Speed=GPIO_SPEED_FREQ_HIGH;g.Mode=out?GPIO_MODE_OUTPUT_OD:GPIO_MODE_INPUT;HAL_GPIO_Init(p,&g);}
static void ow_write_pin(uint16_t id,int v){GPIO_TypeDef*p;uint16_t n;if(Board_PinResolve(id,&p,&n))HAL_GPIO_WritePin(p,n,v?GPIO_PIN_SET:GPIO_PIN_RESET);}static int ow_read_pin(uint16_t id){GPIO_TypeDef*p;uint16_t n;return Board_PinResolve(id,&p,&n)?HAL_GPIO_ReadPin(p,n):1;}
static bool ow_reset(uint16_t id){ow_mode(id,true);ow_write_pin(id,0);Board_DelayUs(480);ow_mode(id,false);Board_DelayUs(70);bool present=!ow_read_pin(id);Board_DelayUs(410);return present;}
static void ow_write_bit(uint16_t id,int b){ow_mode(id,true);ow_write_pin(id,0);if(b){Board_DelayUs(6);ow_mode(id,false);Board_DelayUs(64);}else{Board_DelayUs(60);ow_mode(id,false);Board_DelayUs(10);}}
static int ow_read_bit(uint16_t id){ow_mode(id,true);ow_write_pin(id,0);Board_DelayUs(3);ow_mode(id,false);Board_DelayUs(10);int b=ow_read_pin(id);Board_DelayUs(53);return b;}
static void ow_write_byte(uint16_t id,uint8_t v){for(int i=0;i<8;i++)ow_write_bit(id,(v>>i)&1);}static uint8_t ow_read_byte(uint16_t id){uint8_t v=0;for(int i=0;i<8;i++)v|=(uint8_t)(ow_read_bit(id)<<i);return v;}
static bool read_ds18(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){if(!ow_reset(s->pin_id))return false;ow_write_byte(s->pin_id,0xCC);ow_write_byte(s->pin_id,0x44);HAL_Delay(750);if(!ow_reset(s->pin_id))return false;ow_write_byte(s->pin_id,0xCC);ow_write_byte(s->pin_id,0xBE);uint8_t l=ow_read_byte(s->pin_id),h=ow_read_byte(s->pin_id);int16_t raw=(int16_t)((h<<8)|l);add(t,SA_MEAS_TEMPERATURE,(float)raw/16.0f,100);return true;}

/* ---------- DHT11/22 ---------- */
static bool wait_level(uint16_t id,int level,uint32_t us,uint32_t*elapsed){uint32_t e=0;while(ow_read_pin(id)==level){Board_DelayUs(1);if(++e>=us)return false;}if(elapsed)*elapsed=e;return true;}
static bool read_dht(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t,bool dht22){ow_mode(s->pin_id,true);ow_write_pin(s->pin_id,0);HAL_Delay(dht22?2:20);ow_mode(s->pin_id,false);Board_DelayUs(30);if(!wait_level(s->pin_id,0,100,0)||!wait_level(s->pin_id,1,100,0))return false;uint8_t b[5]={0};for(int k=0;k<40;k++){if(!wait_level(s->pin_id,0,80,0))return false;uint32_t hi;if(!wait_level(s->pin_id,1,100,&hi))return false;b[k/8]=(uint8_t)((b[k/8]<<1)|(hi>40));}if((uint8_t)(b[0]+b[1]+b[2]+b[3])!=b[4])return false;float h,temp;if(dht22){uint16_t rh=(uint16_t)((b[0]<<8)|b[1]);uint16_t rt=(uint16_t)(((b[2]&0x7F)<<8)|b[3]);h=rh/10.0f;temp=rt/10.0f;if(b[2]&0x80)temp=-temp;}else{h=(float)b[0]+b[1]/10.0f;temp=(float)b[2]+b[3]/10.0f;}add(t,SA_MEAS_TEMPERATURE,temp,95);add(t,SA_MEAS_HUMIDITY,h,95);return true;}

/* ---------- SHT3x ---------- */
static bool read_sht3x(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address?s->address:0x44;uint8_t cmd[2]={0x24,0x00},r[6];if(Board_I2C_Write(a,cmd,2,50)!=HAL_OK)return false;HAL_Delay(16);if(Board_I2C_Read(a,r,6,50)!=HAL_OK)return false;if(crc8_sensirion(r,2)!=r[2]||crc8_sensirion(r+3,2)!=r[5])return false;uint16_t rt=(r[0]<<8)|r[1],rh=(r[3]<<8)|r[4];add(t,SA_MEAS_TEMPERATURE,-45.0f+175.0f*rt/65535.0f,100);add(t,SA_MEAS_HUMIDITY,100.0f*rh/65535.0f,100);return true;}

/* ---------- BH1750 ---------- */
static bool read_bh1750(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address?s->address:0x23;uint8_t c=0x10,r[2];if(Board_I2C_Write(a,&c,1,30)!=HAL_OK)return false;HAL_Delay(180);if(Board_I2C_Read(a,r,2,30)!=HAL_OK)return false;uint16_t raw=(r[0]<<8)|r[1];add(t,SA_MEAS_LIGHT_LUX,(float)raw/1.2f,100);return true;}

/* ---------- ADS1115 ---------- */
static bool read_ads1115(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address?s->address:0x48;uint8_t ch=(uint8_t)(s->param0&3u);uint16_t mux=(uint16_t)(4u+ch);uint16_t cfg=(uint16_t)(0x8000u|(mux<<12)|0x0200u|0x0100u|0x0080u|0x0003u);uint8_t w[3]={0x01,(uint8_t)(cfg>>8),(uint8_t)cfg};if(Board_I2C_Write(a,w,3,30)!=HAL_OK)return false;HAL_Delay(10);uint8_t ptr=0,r[2];if(Board_I2C_Write(a,&ptr,1,30)!=HAL_OK||Board_I2C_Read(a,r,2,30)!=HAL_OK)return false;int16_t raw=(int16_t)((r[0]<<8)|r[1]);float mv=(float)raw*0.125f;add(t,s->measurement_id?s->measurement_id:SA_MEAS_RAW,mv,100);return true;}

/* ---------- SCD4x ---------- */
static bool scd_cmd(uint16_t a,uint16_t cmd){uint8_t b[2]={(uint8_t)(cmd>>8),(uint8_t)cmd};return Board_I2C_Write(a,b,2,50)==HAL_OK;}
static bool init_scd4x(const sa_sensor_slot_config_t*s){uint16_t a=s->address?s->address:0x62;if(!scd_cmd(a,0x21B1))return false;HAL_Delay(5);return true;}
static bool read_scd4x(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address?s->address:0x62;if(!scd_cmd(a,0xEC05))return false;HAL_Delay(2);uint8_t r[9];if(Board_I2C_Read(a,r,9,50)!=HAL_OK)return false;for(int i=0;i<3;i++)if(crc8_sensirion(r+i*3,2)!=r[i*3+2])return false;uint16_t co2=(r[0]<<8)|r[1],rt=(r[3]<<8)|r[4],rh=(r[6]<<8)|r[7];add(t,SA_MEAS_CO2,(float)co2,100);add(t,SA_MEAS_TEMPERATURE,-45.0f+175.0f*rt/65535.0f,100);add(t,SA_MEAS_HUMIDITY,100.0f*rh/65535.0f,100);return true;}

/* ---------- BME280 ---------- */
typedef struct{uint16_t T1;int16_t T2,T3;uint16_t P1;int16_t P2,P3,P4,P5,P6,P7,P8,P9;uint8_t H1;int16_t H2;uint8_t H3;int16_t H4,H5;int8_t H6;int32_t tf;} bme_cal_t;
static uint16_t le16u(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static int16_t le16s(const uint8_t*p){return (int16_t)le16u(p);}
static bool bme_read(uint16_t a,uint8_t r,uint8_t*d,uint16_t n){return Board_I2C_MemRead(a,r,I2C_MEMADD_SIZE_8BIT,d,n,50)==HAL_OK;}
static bool bme_write(uint16_t a,uint8_t r,uint8_t v){return Board_I2C_MemWrite(a,r,I2C_MEMADD_SIZE_8BIT,&v,1,50)==HAL_OK;}
static bool read_bme280(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address?s->address:0x76;uint8_t id;if(!bme_read(a,0xD0,&id,1)||id!=0x60)return false;uint8_t c1[26],c2[7];if(!bme_read(a,0x88,c1,26)||!bme_read(a,0xE1,c2,7))return false;bme_cal_t c={0};
    c.T1=le16u(c1);c.T2=le16s(c1+2);c.T3=le16s(c1+4);c.P1=le16u(c1+6);c.P2=le16s(c1+8);c.P3=le16s(c1+10);c.P4=le16s(c1+12);c.P5=le16s(c1+14);c.P6=le16s(c1+16);c.P7=le16s(c1+18);c.P8=le16s(c1+20);c.P9=le16s(c1+22);c.H1=c1[25];c.H2=le16s(c2);c.H3=c2[2];c.H4=(int16_t)((c2[3]<<4)|(c2[4]&0x0F));c.H5=(int16_t)((c2[5]<<4)|(c2[4]>>4));c.H6=(int8_t)c2[6];
    if(!bme_write(a,0xF2,0x01)||!bme_write(a,0xF4,0x27)||!bme_write(a,0xF5,0xA0))return false;HAL_Delay(10);uint8_t r[8];if(!bme_read(a,0xF7,r,8))return false;int32_t ap=(int32_t)((uint32_t)r[0]<<12|(uint32_t)r[1]<<4|r[2]>>4),at=(int32_t)((uint32_t)r[3]<<12|(uint32_t)r[4]<<4|r[5]>>4),ah=(r[6]<<8)|r[7];int32_t v1=((((at>>3)-((int32_t)c.T1<<1)))*c.T2)>>11;int32_t v2=(((((at>>4)-(int32_t)c.T1)*((at>>4)-(int32_t)c.T1))>>12)*c.T3)>>14;c.tf=v1+v2;float temp=(c.tf*5+128)/25600.0f;
    int64_t p1=(int64_t)c.tf-128000,p2=p1*p1*c.P6;p2+=((p1*c.P5)<<17);p2+=((int64_t)c.P4<<35);p1=((p1*p1*c.P3)>>8)+((p1*c.P2)<<12);p1=((((int64_t)1<<47)+p1)*c.P1)>>33;float press=0;if(p1){int64_t p=1048576-ap;p=(((p<<31)-p2)*3125)/p1;p1=((int64_t)c.P9*(p>>13)*(p>>13))>>25;p2=((int64_t)c.P8*p)>>19;p=((p+p1+p2)>>8)+((int64_t)c.P7<<4);press=(float)p/256.0f;}
    int32_t h=c.tf-76800;h=(((((ah<<14)-((int32_t)c.H4<<20)-((int32_t)c.H5*h))+16384)>>15)*(((((((h*c.H6)>>10)*(((h*c.H3)>>11)+32768))>>10)+2097152)*c.H2+8192)>>14));h-=((((h>>15)*(h>>15))>>7)*c.H1)>>4;if(h<0)h=0;if(h>419430400)h=419430400;float hum=(h>>12)/1024.0f;
    add(t,SA_MEAS_TEMPERATURE,temp,100);add(t,SA_MEAS_PRESSURE,press,100);add(t,SA_MEAS_HUMIDITY,hum,100);return true;}

/* ---------- Generic I2C register ---------- */
static bool read_i2c_generic(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint16_t a=s->address;uint8_t n=(uint8_t)(s->param0&0xFFu);if(n<1||n>4)n=2;uint8_t b[4]={0};if(Board_I2C_MemRead(a,s->reg,I2C_MEMADD_SIZE_8BIT,b,n,50)!=HAL_OK)return false;int32_t raw=0;if(s->flags&1u){for(uint8_t i=0;i<n;i++)raw|=(int32_t)b[i]<<(8*i);}else{for(uint8_t i=0;i<n;i++)raw=(raw<<8)|b[i];}if((s->flags&2u)&&n<4){int shift=32-n*8;raw=(raw<<shift)>>shift;}int32_t q=apply_scale(s,raw<<16);add(t,s->measurement_id?s->measurement_id:SA_MEAS_RAW,sa_q16_to_float(q),90);return true;}

/* ---------- Generic Modbus RTU holding/input register ---------- */
static uint16_t mod_crc(const uint8_t*d,int n){uint16_t c=0xFFFF;for(int i=0;i<n;i++){c^=d[i];for(int b=0;b<8;b++)c=(c&1)?(c>>1)^0xA001:c>>1;}return c;}
static bool read_modbus(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){uint8_t addr=s->address?s->address:1,func=(uint8_t)(s->param0?s->param0:3);uint8_t q[8]={addr,func,(uint8_t)(s->reg>>8),(uint8_t)s->reg,0,1,0,0};uint16_t c=mod_crc(q,6);q[6]=(uint8_t)c;q[7]=(uint8_t)(c>>8);__HAL_UART_FLUSH_DRREGISTER(&huart3);if(Board_RS485_Transmit(q,8,100)!=HAL_OK)return false;uint8_t r[7];if(Board_RS485_Receive(r,7,250)!=HAL_OK)return false;if(r[0]!=addr||r[1]!=func||r[2]!=2||mod_crc(r,5)!=(uint16_t)(r[5]|(r[6]<<8)))return false;int32_t raw=(int16_t)((r[3]<<8)|r[4]);int32_t q16=apply_scale(s,raw<<16);add(t,s->measurement_id?s->measurement_id:SA_MEAS_RAW,sa_q16_to_float(q16),95);return true;}

static bool read_slot(const sa_sensor_slot_config_t*s,sa_telemetry_payload_t*t){t->slot_id=s->slot_id;t->driver_id=s->driver_id;t->timestamp_ms=Board_Millis();t->count=0;
    switch(s->driver_id){
        case SA_DRV_GPIO_INPUT:{GPIO_PinState v;if(!Board_GPIO_Read(s->pin_id,&v))return false;add(t,s->measurement_id?s->measurement_id:SA_MEAS_DIGITAL_STATE,v==GPIO_PIN_SET?1.0f:0.0f,100);return true;}
        case SA_DRV_ADC_RAW:case SA_DRV_ADC_LINEAR:case SA_DRV_4_20MA:case SA_DRV_0_10V:{uint16_t raw;if(!Board_ADC_ReadRaw(s->pin_id,&raw))return false;int32_t q=apply_scale(s,(int32_t)raw<<16);add(t,s->measurement_id?s->measurement_id:SA_MEAS_RAW,sa_q16_to_float(q),100);return true;}
        case SA_DRV_DHT11:return read_dht(s,t,false);case SA_DRV_DHT22:return read_dht(s,t,true);case SA_DRV_DS18B20:return read_ds18(s,t);case SA_DRV_SHT3X:return read_sht3x(s,t);case SA_DRV_BME280:return read_bme280(s,t);case SA_DRV_BH1750:return read_bh1750(s,t);case SA_DRV_SCD4X:return read_scd4x(s,t);case SA_DRV_ADS1115:return read_ads1115(s,t);case SA_DRV_I2C_GENERIC:return read_i2c_generic(s,t);case SA_DRV_MODBUS_GENERIC:return read_modbus(s,t);case SA_DRV_PULSE_COUNTER:{add(t,s->measurement_id?s->measurement_id:SA_MEAS_WATER_FLOW,(float)st[s->slot_id].pulse_count,100);st[s->slot_id].pulse_count=0;return true;}default:return false;}
}
static void init_slot(const sa_sensor_slot_config_t*s){if(!s->enabled)return;switch(s->driver_id){case SA_DRV_GPIO_INPUT:Board_GPIO_ConfigInput(s->pin_id,GPIO_PULLUP);break;case SA_DRV_DHT11:case SA_DRV_DHT22:case SA_DRV_DS18B20:ow_mode(s->pin_id,false);break;case SA_DRV_SCD4X:init_scd4x(s);break;case SA_DRV_PULSE_COUNTER:Board_GPIO_ConfigInput(s->pin_id,GPIO_PULLUP);break;default:break;}}
void SensorManager_Init(const sa_node_config_t*c){memset(st,0,sizeof(st));if(c)g_cfg=*c;for(unsigned i=0;i<SA_MAX_SENSOR_SLOTS;i++)init_slot(&g_cfg.slots[i]);}
void SensorManager_Reconfigure(const sa_node_config_t*c){SensorManager_Init(c);}void SensorManager_SetTelemetryCallback(sensor_telemetry_cb_t cb){g_cb=cb;}
void SensorManager_Process(void){uint32_t now=Board_Millis();for(unsigned i=0;i<SA_MAX_SENSOR_SLOTS;i++){sa_sensor_slot_config_t*s=&g_cfg.slots[i];if(!s->enabled)continue;if(s->driver_id==SA_DRV_PULSE_COUNTER){GPIO_PinState v;if(Board_GPIO_Read(s->pin_id,&v)){uint8_t x=(v==GPIO_PIN_SET);if(x&&!st[i].last_gpio)st[i].pulse_count++;st[i].last_gpio=x;}}
        if(now-st[i].last_sample<s->sample_interval_ms)continue;st[i].last_sample=now;sa_telemetry_payload_t t;if(read_slot(s,&t)&&t.count&&g_cb)g_cb(&t);}}
