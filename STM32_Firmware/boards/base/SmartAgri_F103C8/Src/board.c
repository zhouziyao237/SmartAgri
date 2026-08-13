#include "board.h"
#include <string.h>

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart3_rx;
I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);

void Board_Init(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t Board_Millis(void) { return HAL_GetTick(); }
void Board_DelayUs(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq()/1000000u);
    while ((uint32_t)(DWT->CYCCNT-start) < ticks) { __NOP(); }
}

/*
 * Clock policy
 * ------------
 * Primary clock follows UICRM F103_Nano_general's F103 clock tree:
 *   HSE 8 MHz -> PLL x9 -> SYSCLK/HCLK 72 MHz
 *   APB1 36 MHz, APB2 72 MHz, ADC /6 = 12 MHz.
 *
 * The user's minimum-system board physically carries an 8 MHz HSE crystal and
 * a 32.768 kHz LSE crystal. LSE/RTC is intentionally not used by SmartAgri yet.
 *
 * Development resilience:
 *   - retry HSE startup a few times;
 *   - unless SMARTAGRI_CLOCK_STRICT_HSE=1, fall back to HSI/2 x16 = 64 MHz.
 * This does not hide an HSE fault: g_board_warning_code and clock-source globals
 * remain visible through ST-LINK/GDB.
 */
#ifndef SMARTAGRI_CLOCK_STRICT_HSE
#define SMARTAGRI_CLOCK_STRICT_HSE 0
#endif
#ifndef SMARTAGRI_HSE_RETRY_COUNT
#define SMARTAGRI_HSE_RETRY_COUNT 3
#endif

volatile uint32_t g_board_error_code = BOARD_ERR_NONE;
volatile HAL_StatusTypeDef g_board_last_hal_status = HAL_OK;
volatile HAL_StatusTypeDef g_board_hse_last_status = HAL_OK;
volatile uint32_t g_board_rcc_cr = 0;
volatile uint32_t g_board_rcc_cfgr = 0;
volatile uint32_t g_board_hse_on = 0;
volatile uint32_t g_board_hse_ready = 0;
volatile uint32_t g_board_pll_on = 0;
volatile uint32_t g_board_pll_ready = 0;
volatile uint32_t g_board_clock_source = BOARD_CLOCK_UNKNOWN;
volatile uint32_t g_board_warning_code = BOARD_WARN_NONE;
volatile uint32_t g_board_hse_attempts = 0;
volatile uint32_t g_board_sysclk_hz = 0;
volatile uint32_t g_board_hclk_hz = 0;
volatile uint32_t g_board_pclk1_hz = 0;
volatile uint32_t g_board_pclk2_hz = 0;

static void Board_SnapshotClock(void)
{
    g_board_rcc_cr = RCC->CR;
    g_board_rcc_cfgr = RCC->CFGR;
    g_board_hse_on = ((RCC->CR & RCC_CR_HSEON) != 0U) ? 1U : 0U;
    g_board_hse_ready = ((RCC->CR & RCC_CR_HSERDY) != 0U) ? 1U : 0U;
    g_board_pll_on = ((RCC->CR & RCC_CR_PLLON) != 0U) ? 1U : 0U;
    g_board_pll_ready = ((RCC->CR & RCC_CR_PLLRDY) != 0U) ? 1U : 0U;
    g_board_sysclk_hz = HAL_RCC_GetSysClockFreq();
    g_board_hclk_hz = HAL_RCC_GetHCLKFreq();
    g_board_pclk1_hz = HAL_RCC_GetPCLK1Freq();
    g_board_pclk2_hz = HAL_RCC_GetPCLK2Freq();
}

static void Board_FatalError(uint32_t code, HAL_StatusTypeDef status)
{
    g_board_error_code = code;
    g_board_last_hal_status = status;
    Board_SnapshotClock();
    __disable_irq();
    while (1) { __NOP(); }
}

static HAL_StatusTypeDef Board_TryHSE72(void)
{
    HAL_StatusTypeDef st = HAL_ERROR;

    for (uint32_t attempt = 1U; attempt <= (uint32_t)SMARTAGRI_HSE_RETRY_COUNT; ++attempt) {
        RCC_OscInitTypeDef osc = {0};
        g_board_hse_attempts = attempt;

        /* Keep HSI enabled during bring-up just like UICRM's F103 config does. */
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI;
        osc.HSEState = RCC_HSE_ON;
        osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
        osc.HSIState = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLState = RCC_PLL_ON;
        osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        osc.PLL.PLLMUL = RCC_PLL_MUL9;

        st = HAL_RCC_OscConfig(&osc);
        g_board_hse_last_status = st;
        Board_SnapshotClock();
        if (st == HAL_OK) {
            return HAL_OK;
        }

        /* HAL timed out waiting for HSERDY on the user's current board.  Power
         * cycle the oscillator before another attempt. SYSCLK is still HSI here.
         */
        __HAL_RCC_HSE_CONFIG(RCC_HSE_OFF);
        HAL_Delay(10U);
    }

    return st;
}

static HAL_StatusTypeDef Board_EnableHSI64(void)
{
    RCC_OscInitTypeDef osc = {0};

    /* STM32F103: PLL input from HSI is HSI/2 = 4 MHz; x16 => 64 MHz. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_OFF;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    osc.PLL.PLLMUL = RCC_PLL_MUL16;

    return HAL_RCC_OscConfig(&osc);
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    HAL_StatusTypeDef st = Board_TryHSE72();
    if (st == HAL_OK) {
        g_board_clock_source = BOARD_CLOCK_HSE72;
    } else {
#if SMARTAGRI_CLOCK_STRICT_HSE
        Board_FatalError(BOARD_ERR_CLOCK_OSC, st);
#else
        g_board_warning_code = BOARD_WARN_HSE_FALLBACK_HSI64;
        st = Board_EnableHSI64();
        g_board_last_hal_status = st;
        if (st != HAL_OK) {
            Board_FatalError(BOARD_ERR_CLOCK_OSC, st);
        }
        g_board_clock_source = BOARD_CLOCK_HSI64;
#endif
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    st = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
    g_board_last_hal_status = st;
    if (st != HAL_OK) {
        Board_FatalError(BOARD_ERR_CLOCK_TREE, st);
    }

    /* HSE72 -> 12 MHz ADC; HSI64 -> 10.667 MHz ADC. Both are within F1 limits. */
    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    st = HAL_RCCEx_PeriphCLKConfig(&periph);
    g_board_last_hal_status = st;
    if (st != HAL_OK) {
        Board_FatalError(BOARD_ERR_ADC_CLOCK, st);
    }

    Board_SnapshotClock();
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); /* keep SWD, release JTAG-only pins */
    GPIO_InitTypeDef g={0};
    /* ESP WAKE PB0 output, ESP READY PB1 input pulldown */
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);
    g.Pin=GPIO_PIN_0; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB,&g);
    g.Pin=GPIO_PIN_1; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLDOWN; HAL_GPIO_Init(GPIOB,&g);
    /* RS485 DE PB12 */
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET);
    g.Pin=GPIO_PIN_12; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB,&g);
}

static void MX_DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn,2,0); HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn,2,0); HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn,2,0); HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn,2,0); HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

static void uart_gpio_init(UART_HandleTypeDef *h) {
    GPIO_InitTypeDef g={0};
    if (h->Instance==USART1) {
        __HAL_RCC_USART1_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOA,&g);
        g.Pin=GPIO_PIN_10; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL; HAL_GPIO_Init(GPIOA,&g);
        hdma_usart1_tx.Instance=DMA1_Channel4; hdma_usart1_tx.Init.Direction=DMA_MEMORY_TO_PERIPH; hdma_usart1_tx.Init.PeriphInc=DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc=DMA_MINC_ENABLE; hdma_usart1_tx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE; hdma_usart1_tx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode=DMA_NORMAL; hdma_usart1_tx.Init.Priority=DMA_PRIORITY_HIGH; HAL_DMA_Init(&hdma_usart1_tx); __HAL_LINKDMA(h,hdmatx,hdma_usart1_tx);
        hdma_usart1_rx.Instance=DMA1_Channel5; hdma_usart1_rx.Init.Direction=DMA_PERIPH_TO_MEMORY; hdma_usart1_rx.Init.PeriphInc=DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc=DMA_MINC_ENABLE; hdma_usart1_rx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE; hdma_usart1_rx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode=DMA_CIRCULAR; hdma_usart1_rx.Init.Priority=DMA_PRIORITY_VERY_HIGH; HAL_DMA_Init(&hdma_usart1_rx); __HAL_LINKDMA(h,hdmarx,hdma_usart1_rx);
        HAL_NVIC_SetPriority(USART1_IRQn,2,0); HAL_NVIC_EnableIRQ(USART1_IRQn);
    } else if (h->Instance==USART2) {
        __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin=GPIO_PIN_2; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOA,&g);
        g.Pin=GPIO_PIN_3; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL; HAL_GPIO_Init(GPIOA,&g);
        hdma_usart2_rx.Instance=DMA1_Channel6; hdma_usart2_rx.Init.Direction=DMA_PERIPH_TO_MEMORY; hdma_usart2_rx.Init.PeriphInc=DMA_PINC_DISABLE;
        hdma_usart2_rx.Init.MemInc=DMA_MINC_ENABLE; hdma_usart2_rx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE; hdma_usart2_rx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
        hdma_usart2_rx.Init.Mode=DMA_CIRCULAR; hdma_usart2_rx.Init.Priority=DMA_PRIORITY_VERY_HIGH; HAL_DMA_Init(&hdma_usart2_rx); __HAL_LINKDMA(h,hdmarx,hdma_usart2_rx);
        HAL_NVIC_SetPriority(USART2_IRQn,2,0); HAL_NVIC_EnableIRQ(USART2_IRQn);
    } else if (h->Instance==USART3) {
        __HAL_RCC_USART3_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin=GPIO_PIN_10; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB,&g);
        g.Pin=GPIO_PIN_11; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL; HAL_GPIO_Init(GPIOB,&g);
        hdma_usart3_rx.Instance=DMA1_Channel3; hdma_usart3_rx.Init.Direction=DMA_PERIPH_TO_MEMORY; hdma_usart3_rx.Init.PeriphInc=DMA_PINC_DISABLE;
        hdma_usart3_rx.Init.MemInc=DMA_MINC_ENABLE; hdma_usart3_rx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE; hdma_usart3_rx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
        hdma_usart3_rx.Init.Mode=DMA_CIRCULAR; hdma_usart3_rx.Init.Priority=DMA_PRIORITY_VERY_HIGH; HAL_DMA_Init(&hdma_usart3_rx); __HAL_LINKDMA(h,hdmarx,hdma_usart3_rx);
        HAL_NVIC_SetPriority(USART3_IRQn,2,0); HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *h) { uart_gpio_init(h); }

static void init_uart(UART_HandleTypeDef *h, USART_TypeDef *inst, uint32_t baud)
{
    h->Instance=inst; h->Init.BaudRate=baud; h->Init.WordLength=UART_WORDLENGTH_8B; h->Init.StopBits=UART_STOPBITS_1;
    h->Init.Parity=UART_PARITY_NONE; h->Init.Mode=UART_MODE_TX_RX; h->Init.HwFlowCtl=UART_HWCONTROL_NONE; h->Init.OverSampling=UART_OVERSAMPLING_16;
    HAL_StatusTypeDef st = HAL_UART_Init(h);
    if (st != HAL_OK) {
        uint32_t code = (inst == USART1) ? BOARD_ERR_UART1_INIT :
                        (inst == USART2) ? BOARD_ERR_UART2_INIT : BOARD_ERR_UART3_INIT;
        Board_FatalError(code, st);
    }
}
static void MX_USART1_UART_Init(void){ init_uart(&huart1,USART1,115200); }
static void MX_USART2_UART_Init(void){ init_uart(&huart2,USART2,921600); }
static void MX_USART3_UART_Init(void){ init_uart(&huart3,USART3,921600); }

void HAL_I2C_MspInit(I2C_HandleTypeDef *h) {
    if(h->Instance!=I2C1) return;
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_I2C1_CLK_ENABLE();
    GPIO_InitTypeDef g={0}; g.Pin=GPIO_PIN_6|GPIO_PIN_7; g.Mode=GPIO_MODE_AF_OD; g.Speed=GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB,&g);
}
static void MX_I2C1_Init(void) {
    hi2c1.Instance=I2C1; hi2c1.Init.ClockSpeed=100000; hi2c1.Init.DutyCycle=I2C_DUTYCYCLE_2; hi2c1.Init.OwnAddress1=0;
    hi2c1.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT; hi2c1.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE; hi2c1.Init.OwnAddress2=0;
    hi2c1.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE; hi2c1.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
    HAL_StatusTypeDef st = HAL_I2C_Init(&hi2c1);
    if (st != HAL_OK) Board_FatalError(BOARD_ERR_I2C1_INIT, st);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *h) {
    if (h->Instance != ADC1) return;
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Dynamic ADC sensor inputs: PA0/PA1/PA4/PA5/PA6/PA7. */
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
}
static void MX_ADC1_Init(void)
{
    hadc1.Instance=ADC1; hadc1.Init.ScanConvMode=ADC_SCAN_DISABLE; hadc1.Init.ContinuousConvMode=DISABLE; hadc1.Init.DiscontinuousConvMode=DISABLE;
    hadc1.Init.ExternalTrigConv=ADC_SOFTWARE_START; hadc1.Init.DataAlign=ADC_DATAALIGN_RIGHT; hadc1.Init.NbrOfConversion=1;
    HAL_StatusTypeDef st = HAL_ADC_Init(&hadc1);
    if (st != HAL_OK) Board_FatalError(BOARD_ERR_ADC1_INIT, st);
    st = HAL_ADCEx_Calibration_Start(&hadc1);
    if (st != HAL_OK) Board_FatalError(BOARD_ERR_ADC1_CAL, st);
}

bool Board_PinResolve(uint16_t id, GPIO_TypeDef **port, uint16_t *pin){
    if(!port||!pin) return false; *port=0; *pin=0;
    switch(id){
        case SA_PIN_PA0:*port=GPIOA;*pin=GPIO_PIN_0;break; case SA_PIN_PA1:*port=GPIOA;*pin=GPIO_PIN_1;break;
        case SA_PIN_PA4:*port=GPIOA;*pin=GPIO_PIN_4;break; case SA_PIN_PA5:*port=GPIOA;*pin=GPIO_PIN_5;break;
        case SA_PIN_PA6:*port=GPIOA;*pin=GPIO_PIN_6;break; case SA_PIN_PA7:*port=GPIOA;*pin=GPIO_PIN_7;break;
        case SA_PIN_PA8:*port=GPIOA;*pin=GPIO_PIN_8;break; case SA_PIN_PA11:*port=GPIOA;*pin=GPIO_PIN_11;break; case SA_PIN_PA12:*port=GPIOA;*pin=GPIO_PIN_12;break;
        case SA_PIN_PB5:*port=GPIOB;*pin=GPIO_PIN_5;break; case SA_PIN_PB6:*port=GPIOB;*pin=GPIO_PIN_6;break; case SA_PIN_PB7:*port=GPIOB;*pin=GPIO_PIN_7;break;
        case SA_PIN_PB8:*port=GPIOB;*pin=GPIO_PIN_8;break; case SA_PIN_PB9:*port=GPIOB;*pin=GPIO_PIN_9;break;
        case SA_PIN_PB13:*port=GPIOB;*pin=GPIO_PIN_13;break; case SA_PIN_PB14:*port=GPIOB;*pin=GPIO_PIN_14;break; case SA_PIN_PB15:*port=GPIOB;*pin=GPIO_PIN_15;break;
        default:return false;
    } return true;
}
uint32_t Board_PinCapabilities(uint16_t id){
    switch(id){
        case SA_PIN_PA0: case SA_PIN_PA1: case SA_PIN_PA4: case SA_PIN_PA5: case SA_PIN_PA6: case SA_PIN_PA7:
            return SA_PINCAP_GPIO_IN|SA_PINCAP_GPIO_OUT|SA_PINCAP_ADC|SA_PINCAP_ONEWIRE|SA_PINCAP_DHT|SA_PINCAP_PULSE;
        case SA_PIN_PB6: case SA_PIN_PB7: return SA_PINCAP_I2C|SA_PINCAP_RESERVED;
        case SA_PIN_PA8: case SA_PIN_PA11: case SA_PIN_PA12: case SA_PIN_PB5: case SA_PIN_PB8: case SA_PIN_PB9: case SA_PIN_PB13: case SA_PIN_PB14: case SA_PIN_PB15:
            return SA_PINCAP_GPIO_IN|SA_PINCAP_GPIO_OUT|SA_PINCAP_ONEWIRE|SA_PINCAP_DHT|SA_PINCAP_PULSE;
        default:return 0;
    }
}

bool Board_GPIO_ConfigInput(uint16_t id,uint32_t pull){ GPIO_TypeDef *p;uint16_t n;if(!Board_PinResolve(id,&p,&n))return false; GPIO_InitTypeDef g={0};g.Pin=n;g.Mode=GPIO_MODE_INPUT;g.Pull=pull;HAL_GPIO_Init(p,&g);return true; }
bool Board_GPIO_ConfigOutput(uint16_t id,GPIO_PinState initial){ GPIO_TypeDef *p;uint16_t n;if(!Board_PinResolve(id,&p,&n))return false; HAL_GPIO_WritePin(p,n,initial);GPIO_InitTypeDef g={0};g.Pin=n;g.Mode=GPIO_MODE_OUTPUT_PP;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(p,&g);return true; }
bool Board_GPIO_Write(uint16_t id,GPIO_PinState s){ GPIO_TypeDef *p;uint16_t n;if(!Board_PinResolve(id,&p,&n))return false;HAL_GPIO_WritePin(p,n,s);return true; }
bool Board_GPIO_Read(uint16_t id,GPIO_PinState *s){ GPIO_TypeDef *p;uint16_t n;if(!s||!Board_PinResolve(id,&p,&n))return false;*s=HAL_GPIO_ReadPin(p,n);return true; }

bool Board_ADC_ReadRaw(uint16_t id,uint16_t *raw){
    if(!raw) return false; uint32_t ch;
    switch(id){case SA_PIN_PA0:ch=ADC_CHANNEL_0;break;case SA_PIN_PA1:ch=ADC_CHANNEL_1;break;case SA_PIN_PA4:ch=ADC_CHANNEL_4;break;case SA_PIN_PA5:ch=ADC_CHANNEL_5;break;case SA_PIN_PA6:ch=ADC_CHANNEL_6;break;case SA_PIN_PA7:ch=ADC_CHANNEL_7;break;default:return false;}
    GPIO_TypeDef *p;uint16_t n;Board_PinResolve(id,&p,&n);GPIO_InitTypeDef g={0};g.Pin=n;g.Mode=GPIO_MODE_ANALOG;HAL_GPIO_Init(p,&g);
    ADC_ChannelConfTypeDef c={0};c.Channel=ch;c.Rank=ADC_REGULAR_RANK_1;c.SamplingTime=ADC_SAMPLETIME_55CYCLES_5;
    if(HAL_ADC_ConfigChannel(&hadc1,&c)!=HAL_OK||HAL_ADC_Start(&hadc1)!=HAL_OK||HAL_ADC_PollForConversion(&hadc1,10)!=HAL_OK)return false;
    *raw=(uint16_t)HAL_ADC_GetValue(&hadc1);HAL_ADC_Stop(&hadc1);return true;
}

HAL_StatusTypeDef Board_I2C_Write(uint16_t a,const uint8_t*d,uint16_t l,uint32_t t){return HAL_I2C_Master_Transmit(&hi2c1,(uint16_t)(a<<1),(uint8_t*)d,l,t);} 
HAL_StatusTypeDef Board_I2C_Read(uint16_t a,uint8_t*d,uint16_t l,uint32_t t){return HAL_I2C_Master_Receive(&hi2c1,(uint16_t)(a<<1),d,l,t);} 
HAL_StatusTypeDef Board_I2C_MemRead(uint16_t a,uint16_t r,uint16_t rs,uint8_t*d,uint16_t l,uint32_t t){return HAL_I2C_Mem_Read(&hi2c1,(uint16_t)(a<<1),r,rs,d,l,t);} 
HAL_StatusTypeDef Board_I2C_MemWrite(uint16_t a,uint16_t r,uint16_t rs,const uint8_t*d,uint16_t l,uint32_t t){return HAL_I2C_Mem_Write(&hi2c1,(uint16_t)(a<<1),r,rs,(uint8_t*)d,l,t);} 
HAL_StatusTypeDef Board_RS485_Transmit(const uint8_t*d,uint16_t l,uint32_t t){HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET);Board_DelayUs(20);HAL_StatusTypeDef s=HAL_UART_Transmit(&huart3,(uint8_t*)d,l,t);while(__HAL_UART_GET_FLAG(&huart3,UART_FLAG_TC)==RESET){}Board_DelayUs(20);HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET);return s;}
HAL_StatusTypeDef Board_RS485_Receive(uint8_t*d,uint16_t l,uint32_t t){HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET);return HAL_UART_Receive(&huart3,d,l,t);} 

void DMA1_Channel4_IRQHandler(void){HAL_DMA_IRQHandler(&hdma_usart1_tx);}
void DMA1_Channel5_IRQHandler(void){HAL_DMA_IRQHandler(&hdma_usart1_rx);}
void DMA1_Channel6_IRQHandler(void){HAL_DMA_IRQHandler(&hdma_usart2_rx);}
void DMA1_Channel3_IRQHandler(void){HAL_DMA_IRQHandler(&hdma_usart3_rx);}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&huart1);}
void USART2_IRQHandler(void){HAL_UART_IRQHandler(&huart2);}
void USART3_IRQHandler(void){HAL_UART_IRQHandler(&huart3);}

void SysTick_Handler(void){HAL_IncTick();HAL_SYSTICK_IRQHandler();}
void Error_Handler(void)
{
    Board_FatalError(BOARD_ERR_GENERIC, HAL_ERROR);
}
