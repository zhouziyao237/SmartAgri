#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SA_MAX_SENSOR_SLOTS 8u
#define SA_CONFIG_SCHEMA_VERSION 0x0201u

typedef enum { SA_ROLE_UNCONFIGURED=0, SA_ROLE_MASTER=1, SA_ROLE_SLAVE=2 } sa_node_role_t;
typedef enum { SA_IF_NONE=0, SA_IF_GPIO=1, SA_IF_ADC=2, SA_IF_I2C=3, SA_IF_ONEWIRE=4, SA_IF_DHT=5, SA_IF_MODBUS=6, SA_IF_PULSE=7 } sa_interface_t;

typedef enum {
    SA_DRV_NONE=0,
    SA_DRV_GPIO_INPUT=1,
    SA_DRV_ADC_RAW=2,
    SA_DRV_ADC_LINEAR=3,
    SA_DRV_4_20MA=4,
    SA_DRV_0_10V=5,
    SA_DRV_DHT11=10,
    SA_DRV_DHT22=11,
    SA_DRV_DS18B20=12,
    SA_DRV_SHT3X=20,
    SA_DRV_BME280=21,
    SA_DRV_BH1750=22,
    SA_DRV_SCD4X=23,
    SA_DRV_ADS1115=24,
    SA_DRV_I2C_GENERIC=30,
    SA_DRV_MODBUS_GENERIC=40,
    SA_DRV_PULSE_COUNTER=50
} sa_sensor_driver_id_t;

typedef enum {
    SA_MEAS_NONE=0,
    SA_MEAS_TEMPERATURE=1,
    SA_MEAS_HUMIDITY=2,
    SA_MEAS_PRESSURE=3,
    SA_MEAS_CO2=10,
    SA_MEAS_LIGHT_LUX=11,
    SA_MEAS_SOIL_MOISTURE=20,
    SA_MEAS_SOIL_TEMPERATURE=21,
    SA_MEAS_SOIL_EC=22,
    SA_MEAS_SOIL_PH=23,
    SA_MEAS_NITROGEN=30,
    SA_MEAS_PHOSPHORUS=31,
    SA_MEAS_POTASSIUM=32,
    SA_MEAS_WATER_LEVEL=40,
    SA_MEAS_WATER_FLOW=41,
    SA_MEAS_WATER_PH=42,
    SA_MEAS_WATER_EC=43,
    SA_MEAS_DISSOLVED_OXYGEN=44,
    SA_MEAS_ORP=45,
    SA_MEAS_WIND_SPEED=50,
    SA_MEAS_WIND_DIRECTION=51,
    SA_MEAS_RAINFALL=52,
    SA_MEAS_DIGITAL_STATE=60,
    SA_MEAS_RAW=0x7FFF
} sa_measurement_t;

typedef enum {
    SA_PIN_NONE=0,
    SA_PIN_PA0=1, SA_PIN_PA1=2, SA_PIN_PA4=5, SA_PIN_PA5=6, SA_PIN_PA6=7, SA_PIN_PA7=8,
    SA_PIN_PA8=9, SA_PIN_PA11=12, SA_PIN_PA12=13,
    SA_PIN_PB5=21, SA_PIN_PB6=22, SA_PIN_PB7=23, SA_PIN_PB8=24, SA_PIN_PB9=25,
    SA_PIN_PB13=29, SA_PIN_PB14=30, SA_PIN_PB15=31
} sa_pin_id_t;

enum { SA_PINCAP_GPIO_IN=1u<<0, SA_PINCAP_GPIO_OUT=1u<<1, SA_PINCAP_ADC=1u<<2, SA_PINCAP_I2C=1u<<3, SA_PINCAP_ONEWIRE=1u<<4, SA_PINCAP_DHT=1u<<5, SA_PINCAP_PULSE=1u<<6, SA_PINCAP_RESERVED=1u<<31 };

#pragma pack(push,1)
typedef struct {
    uint8_t slot_id;
    uint8_t enabled;
    uint16_t driver_id;
    uint16_t measurement_id;
    uint8_t interface_type;
    uint8_t bus_id;
    uint16_t pin_id;
    uint16_t address;
    uint16_t reg;
    uint32_t sample_interval_ms;
    uint32_t report_interval_ms;
    int32_t scale_q16;
    int32_t offset_q16;
    int32_t threshold_q16;
    uint32_t flags;
    uint32_t param0;
    uint32_t param1;
} sa_sensor_slot_config_t;

typedef struct {
    uint16_t schema_version;
    uint8_t role;
    uint8_t reserved0;
    uint32_t device_id;
    uint16_t network_id;
    uint16_t group_id;
    uint8_t sensor_count;
    uint8_t reserved1[3];
    sa_sensor_slot_config_t slots[SA_MAX_SENSOR_SLOTS];
} sa_node_config_t;

typedef struct {
    uint16_t measurement_id;
    int32_t value_q16;
    uint8_t quality;
    uint8_t reserved;
} sa_measurement_value_t;

typedef struct {
    uint8_t slot_id;
    uint8_t count;
    uint16_t driver_id;
    uint32_t timestamp_ms;
    sa_measurement_value_t values[4];
} sa_telemetry_payload_t;

typedef struct {
    uint16_t pin_id;
    uint8_t value;
    uint8_t mode; /* 0=set, 1=pulse */
    uint32_t duration_ms;
} sa_gpio_control_t;

typedef struct {
    uint8_t slot_id;
    uint8_t quality;
    uint16_t driver_id;
    uint16_t measurement_id;
    uint16_t reserved;
    int32_t value_q16;
} sa_diag_telemetry_t;

typedef struct {
    uint16_t command;
    int16_t result;
    uint32_t detail;
} sa_control_result_t;
#pragma pack(pop)

static inline int32_t sa_float_to_q16(float v){ return (int32_t)(v*65536.0f); }
static inline float sa_q16_to_float(int32_t v){ return (float)v/65536.0f; }
