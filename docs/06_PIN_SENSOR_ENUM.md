# 06 - Pin ID / Sensor Driver ID

## Pin ID

```text
PA0  = 1
PA1  = 2
PA4  = 5
PA5  = 6
PA6  = 7
PA7  = 8
PA8  = 9
PA11 = 12
PA12 = 13
PB5  = 21
PB8  = 24
PB9  = 25
PB13 = 29
PB14 = 30
PB15 = 31
```

系统固定资源不会暴露为通用 Sensor Pin：

- PA9/PA10 = ESP USART1
- PA2/PA3 = Debug USART2
- PA13/PA14 = SWD
- PB0/PB1 = ESP WAKE/READY
- PB6/PB7 = I2C1
- PB10/PB11/PB12 = RS485 USART3 + DE

## Driver ID

```text
1  GPIO_INPUT
2  ADC_RAW
3  ADC_LINEAR
4  4_20MA
5  0_10V
10 DHT11
11 DHT22
12 DS18B20
20 SHT3X
21 BME280
22 BH1750
23 SCD4X
24 ADS1115
30 I2C_GENERIC
40 MODBUS_GENERIC
50 PULSE_COUNTER
```

接口 ID：GPIO=1, ADC=2, I2C=3, 1-Wire=4, DHT=5, Modbus=6, Pulse=7。

Generic Modbus 当前默认使用 USART3 9600 8N1：

- `address` = Modbus slave address
- `reg` = register address
- `param0` = function code，0 时默认 0x03
- 每次读 1 个 16-bit register
- `measurement_id/scale/offset` 决定工程量解释

高频 Pulse Counter 当前版本为主循环软件边沿计数，适合低频雨量/慢速流量脉冲；高频编码器或高速流量计应改 TIM Input Capture/EXTI。
