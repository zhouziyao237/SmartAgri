#pragma once
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;

/* Board bring-up error codes. Watch these globals with ST-LINK/GDB. */
typedef enum {
    BOARD_ERR_NONE       = 0x0000u,
    BOARD_ERR_CLOCK_OSC  = 0x0101u, /* HSE/PLL oscillator configuration */
    BOARD_ERR_CLOCK_TREE = 0x0102u, /* SYSCLK/AHB/APB switch */
    BOARD_ERR_UART1_INIT = 0x0201u,
    BOARD_ERR_UART2_INIT = 0x0202u,
    BOARD_ERR_UART3_INIT = 0x0203u,
    BOARD_ERR_I2C1_INIT  = 0x0301u,
    BOARD_ERR_ADC_CLOCK  = 0x0401u,
    BOARD_ERR_ADC1_INIT  = 0x0402u,
    BOARD_ERR_ADC1_CAL   = 0x0403u,
    BOARD_ERR_GENERIC    = 0xFFFFu
} board_error_code_t;

typedef enum {
    BOARD_CLOCK_UNKNOWN = 0u,
    BOARD_CLOCK_HSE72   = 1u, /* UICRM-style: external 8 MHz crystal x9 */
    BOARD_CLOCK_HSI64   = 2u  /* recovery: internal HSI/2 x16 */
} board_clock_source_t;

typedef enum {
    BOARD_WARN_NONE = 0u,
    BOARD_WARN_HSE_FALLBACK_HSI64 = 0x1001u
} board_warning_code_t;

extern volatile uint32_t g_board_error_code;
extern volatile HAL_StatusTypeDef g_board_last_hal_status;
extern volatile HAL_StatusTypeDef g_board_hse_last_status;
extern volatile uint32_t g_board_rcc_cr;
extern volatile uint32_t g_board_rcc_cfgr;
extern volatile uint32_t g_board_hse_on;
extern volatile uint32_t g_board_hse_ready;
extern volatile uint32_t g_board_pll_on;
extern volatile uint32_t g_board_pll_ready;
extern volatile uint32_t g_board_clock_source;
extern volatile uint32_t g_board_warning_code;
extern volatile uint32_t g_board_hse_attempts;
extern volatile uint32_t g_board_sysclk_hz;
extern volatile uint32_t g_board_hclk_hz;
extern volatile uint32_t g_board_pclk1_hz;
extern volatile uint32_t g_board_pclk2_hz;

void Board_Init(void);
uint32_t Board_Millis(void);
void Board_DelayUs(uint32_t us);

bool Board_PinResolve(uint16_t pin_id, GPIO_TypeDef **port, uint16_t *pin);
uint32_t Board_PinCapabilities(uint16_t pin_id);
bool Board_GPIO_ConfigInput(uint16_t pin_id, uint32_t pull);
bool Board_GPIO_ConfigOutput(uint16_t pin_id, GPIO_PinState initial);
bool Board_GPIO_Write(uint16_t pin_id, GPIO_PinState state);
bool Board_GPIO_Read(uint16_t pin_id, GPIO_PinState *state);
bool Board_ADC_ReadRaw(uint16_t pin_id, uint16_t *raw);

HAL_StatusTypeDef Board_I2C_Write(uint16_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef Board_I2C_Read(uint16_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef Board_I2C_MemRead(uint16_t addr7, uint16_t reg, uint16_t reg_size, uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef Board_I2C_MemWrite(uint16_t addr7, uint16_t reg, uint16_t reg_size, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef Board_RS485_Transmit(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef Board_RS485_Receive(uint8_t *data, uint16_t len, uint32_t timeout_ms);

void Error_Handler(void);

#ifdef __cplusplus
}
#endif
