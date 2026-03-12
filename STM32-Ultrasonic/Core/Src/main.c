/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — Ultrasonic + I2C to ESP32
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "stm32l4xx_hal_uart.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char msg[50];
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN 0 */

// ===== Ultrasonic Sensor =====
#define TRIG_PORT        GPIOA
#define TRIG_PIN         GPIO_PIN_0
#define ECHO_PORT        GPIOA
#define ECHO_PIN         GPIO_PIN_1
#define ECHO_TIMEOUT_US  38000
#define MAX_DISTANCE_CM  400    // HC-SR04 max range

// ===== I2C to ESP32 =====
#define ESP32_I2C_ADDR   (0x08 << 1)

// ===== Moving Average Filter =====
#define FILTER_SIZE  5
static uint32_t filter_buf[FILTER_SIZE] = {0};
static uint8_t  filter_idx = 0;

// -----------------------------------------------------------------------------

void delay_us(uint32_t us)
{
    uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
    while ((__HAL_TIM_GET_COUNTER(&htim2) - start) < us);
}

uint32_t Ultrasonic_Read(void)
{
    uint32_t start, stop, t0;

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    // รอ ECHO ขึ้น HIGH
    t0 = __HAL_TIM_GET_COUNTER(&htim2);
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET)
        if ((__HAL_TIM_GET_COUNTER(&htim2) - t0) >= ECHO_TIMEOUT_US) return 0;

    start = __HAL_TIM_GET_COUNTER(&htim2);

    // รอ ECHO ลง LOW
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
        if ((__HAL_TIM_GET_COUNTER(&htim2) - start) >= ECHO_TIMEOUT_US) return 0;

    stop = __HAL_TIM_GET_COUNTER(&htim2);
    return stop - start;
}

uint32_t Filter_Distance(uint32_t new_val)
{
    filter_buf[filter_idx] = new_val;
    filter_idx = (filter_idx + 1) % FILTER_SIZE;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < FILTER_SIZE; i++)
        sum += filter_buf[i];

    return sum / FILTER_SIZE;
}

void I2C_SendDistance(uint32_t distance)
{
    uint8_t buf[4];
    buf[0] = (distance)       & 0xFF;
    buf[1] = (distance >> 8)  & 0xFF;
    buf[2] = (distance >> 16) & 0xFF;
    buf[3] = (distance >> 24) & 0xFF;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &hi2c1, ESP32_I2C_ADDR, buf, 4, 100
    );

    if (status != HAL_OK)
        HAL_UART_Transmit(&huart2, (uint8_t*)"I2C TX Error\r\n", 14, HAL_MAX_DELAY);
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();

    /* USER CODE BEGIN 2 */
    HAL_TIM_Base_Start(&htim2);
    HAL_UART_Transmit(&huart2, (uint8_t*)"System Ready\r\n", 14, HAL_MAX_DELAY);

    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c1, ESP32_I2C_ADDR, 3, 100);
    if (ret == HAL_OK)
        HAL_UART_Transmit(&huart2, (uint8_t*)"ESP32 Found!\r\n", 14, HAL_MAX_DELAY);
    else
        HAL_UART_Transmit(&huart2, (uint8_t*)"ESP32 NOT Found\r\n", 17, HAL_MAX_DELAY);
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint32_t echo_time = Ultrasonic_Read();
        uint32_t distance  = (echo_time == 0) ? 0 : (echo_time / 58);

        // clamp ค่าที่เกิน max range
        if (distance > MAX_DISTANCE_CM) distance = MAX_DISTANCE_CM;

        // moving average filter
        uint32_t filtered = Filter_Distance(distance);

        if (filtered == 0 || filtered > MAX_DISTANCE_CM)
        {
            sprintf(msg, "Distance: OUT OF RANGE\r\n");
            I2C_SendDistance(9999);
        }
        else
        {
            sprintf(msg, "Distance: %lu cm\r\n", filtered);
            I2C_SendDistance(filtered);
        }

        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
        HAL_Delay(100);
        /* USER CODE END 3 */
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
        Error_Handler();

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.LSEState            = RCC_LSE_ON;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;
    RCC_OscInitStruct.PLL.PLLN            = 16;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
        Error_Handler();

    HAL_RCCEx_EnableMSIPLLMode();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance              = I2C1;
    hi2c1.Init.Timing           = 0x00B07CB4;
    hi2c1.Init.OwnAddress1      = 0;
    hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2      = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
        Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 79;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 4294967295;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
        Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
        Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
        Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance                    = USART2;
    huart2.Init.BaudRate               = 115200;
    huart2.Init.WordLength             = UART_WORDLENGTH_8B;
    huart2.Init.StopBits               = UART_STOPBITS_1;
    huart2.Init.Parity                 = UART_PARITY_NONE;
    huart2.Init.Mode                   = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_5, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_4;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_3 | GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif