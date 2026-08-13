# UICRM F103 clock baseline + current HSE timeout handling

## Hardware
Target board: STM32F103C8T6 minimum-system board with:
- 8 MHz passive HSE crystal on PD0/PD1
- 32.768 kHz LSE crystal on PC14/PC15
- SWD PA13/PA14 and NRST

The 32.768 kHz crystal is physically present but SmartAgri does not start/use LSE yet because RTC is not required for cluster bring-up.

## Primary clock (UICRM F103_Nano_general aligned)
The primary clock policy matches the UICRM F103 Nano clock tree:
- HSE = 8 MHz crystal
- PLL source = HSE
- PLL multiplier = x9
- SYSCLK/HCLK = 72 MHz
- APB1 = 36 MHz
- APB2 = 72 MHz
- ADC prescaler = /6 -> 12 MHz

UICRM also keeps HSI enabled during oscillator configuration; V2.1.6 does the same.

## Why the user's board stopped previously
The observed failure was inside STM32CubeF1 HAL while waiting for HSERDY:

    while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET) ... HAL_TIMEOUT

That means the software requested HSE ON, but the MCU never saw the external 8 MHz oscillator become ready before the timeout.

This is not caused by USART/ADC/I2C/ESP initialization because SystemClock_Config runs first.

## V2.1.6 behavior
Default development policy is AUTO:
1. Try HSE72 up to 3 times.
2. If HSE succeeds: run at 72 MHz (`g_board_clock_source = 1`).
3. If HSE still times out: preserve a warning and boot using HSI64 (`g_board_clock_source = 2`).

Debugger globals:
- `g_board_hse_last_status`
- `g_board_hse_attempts`
- `g_board_hse_on`
- `g_board_hse_ready`
- `g_board_pll_on`
- `g_board_pll_ready`
- `g_board_clock_source`
- `g_board_warning_code`
- `g_board_sysclk_hz`
- `g_board_hclk_hz`
- `g_board_pclk1_hz`
- `g_board_pclk2_hz`

Expected healthy board:
- `g_board_clock_source = 1`
- `g_board_hse_ready = 1`
- `g_board_pll_ready = 1`
- `g_board_sysclk_hz = 72000000`
- `g_board_warning_code = 0`

If HSE is still physically failing but firmware continues:
- `g_board_clock_source = 2`
- `g_board_hse_last_status = HAL_TIMEOUT`
- `g_board_warning_code = 0x1001`
- `g_board_sysclk_hz = 64000000`

## Strict UICRM-style mode
For final hardware validation, disable fallback in CLion CMake options:

    -DSMARTAGRI_CLOCK_STRICT_HSE=ON

Then an HSE timeout remains fatal, matching the normal CubeMX/UICRM behavior.
