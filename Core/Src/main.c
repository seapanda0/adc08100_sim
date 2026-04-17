/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

// INPUTS FROM FPGA
// A12 - ADC_CLK = tim1 ext

// OUTPUTS TO FPGA
// A0-A7 - ADC_IN
// C13 - RESET_N
// C14 - MODE
// B7 - 6khz PWM Clock (TIM4 CH1 PWM)

// OTHER PINS
// B1 - ANALOG IN


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
uint16_t val;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define N_TAPS  12
#define Q_SCALE 128

#define COEFF_0  0x01  /*    1 -> +0.007812  (float: +0.008260) */
#define COEFF_1  0x02  /*    2 -> +0.015625  (float: +0.019310) */
#define COEFF_2  0x07  /*    7 -> +0.054688  (float: +0.051067) */
#define COEFF_3  0x0D  /*   13 -> +0.101562  (float: +0.098457) */
#define COEFF_4  0x13  /*   19 -> +0.148438  (float: +0.146382) */
#define COEFF_5  0x17  /*   23 -> +0.179688  (float: +0.176525) */
#define COEFF_6  0x17  /*   23 -> +0.179688  (float: +0.176525) */
#define COEFF_7  0x13  /*   19 -> +0.148438  (float: +0.146382) */
#define COEFF_8  0x0D  /*   13 -> +0.101562  (float: +0.098457) */
#define COEFF_9  0x07  /*    7 -> +0.054688  (float: +0.051067) */
#define COEFF_10  0x02  /*    2 -> +0.015625  (float: +0.019310) */
#define COEFF_11  0x01  /*    1 -> +0.007812  (float: +0.008260) */

static const int8_t fir_coeffs[N_TAPS] = {
    (int8_t)COEFF_0  /*    1 */,
    (int8_t)COEFF_1  /*    2 */,
    (int8_t)COEFF_2  /*    7 */,
    (int8_t)COEFF_3  /*   13 */,
    (int8_t)COEFF_4  /*   19 */,
    (int8_t)COEFF_5  /*   23 */,
    (int8_t)COEFF_6  /*   23 */,
    (int8_t)COEFF_7  /*   19 */,
    (int8_t)COEFF_8  /*   13 */,
    (int8_t)COEFF_9  /*    7 */,
    (int8_t)COEFF_10  /*    2 */,
    (int8_t)COEFF_11  /*    1 */
};

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & (1 << 12))
    {
        EXTI->PR = (1 << 12);  // clear flag

        // Wait for ADC conversion to complete
        while (!(ADC1->SR & ADC_SR_EOC));

        val = ADC1->DR >> 4;

        // Write to PA0-PA7
        GPIOA->ODR = (GPIOA->ODR & 0xFF00) | (val & 0xFF);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_SYSCFGEN;

  // --- PC13: RESET_N, PC14: MODE as output ---
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;  // enable GPIOC clock

  GPIOC->MODER   &= ~((3 << (13 * 2)) | (3 << (14 * 2)));  // clear
  GPIOC->MODER   |=  ((1 << (13 * 2)) | (1 << (14 * 2)));  // output
  GPIOC->OTYPER  &= ~((1 << 13) | (1 << 14));               // push-pull
  GPIOC->OSPEEDR &= ~((3 << (13 * 2)) | (3 << (14 * 2)));  // low speed
  GPIOC->PUPDR   &= ~((3 << (13 * 2)) | (3 << (14 * 2)));  // no pull
  
  // PA0-PA7 as general purpose output
  GPIOA->MODER   &= ~(0x0000FFFF);  // clear bits 0-15 (PA0-PA7)
  GPIOA->MODER   |=  (0x00005555);  // 01 for each = output mode
  GPIOA->OTYPER  &= ~(0x00FF);      // push-pull
  GPIOA->OSPEEDR &= ~(0x0000FFFF);  // clear
  GPIOA->OSPEEDR |=  (0x00005555);  // medium speed
  GPIOA->PUPDR   &= ~(0x0000FFFF);  // no pull

  // --- PB7: GPIO output (before PWM clock use) ---
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

  GPIOB->MODER   &= ~(3 << (7 * 2));  // clear
  GPIOB->MODER   |=  (1 << (7 * 2));  // output
  GPIOB->OTYPER  &= ~(1 << 7);        // push-pull
  GPIOB->OSPEEDR &= ~(3 << (7 * 2));  // low speed
  GPIOB->PUPDR   &= ~(3 << (7 * 2));  // no pull

  // Toggle PB7 manually
  GPIOB->BSRR = (1 << (7 + 16));  // set low


  // COEFFICIENT LOADING START

  // Set to coefficient loading mode
  GPIOC->BSRR = (1 << 14);         // set mode pin to 1
  GPIOB->BSRR = (1 << (7 + 16));  // set clock low
  HAL_Delay(5);

  // Reset the fpga
  GPIOC->BSRR = (1 << (13 + 16));  // set reset pin low
  HAL_Delay(5);
  GPIOB->BSRR = (1 << 7);         // set clock high
  HAL_Delay(5);
  GPIOB->BSRR = (1 << (7 + 16));  // set clock low
  GPIOC->BSRR = (1 << 13);        // set reset pin high
  HAL_Delay(5);

  // Load coefficients
  for(int i = 0; i < N_TAPS; i ++){
    int8_t temp = fir_coeffs[i];
    GPIOA->ODR = (GPIOA->ODR & 0xFF00) | (temp & 0xFF);

    HAL_Delay(5);
    GPIOB->BSRR = (1 << 7);         // set clock high

    HAL_Delay(5);
    GPIOB->BSRR = (1 << (7 + 16));  // set clock low
  }
  GPIOC->BSRR = (1 << (14 + 16));  // MODE = 0 (normal operation)
  // COEFFICIENT LOADING END

  // --- PA12: TIM1_ETR, AF1 ---
  GPIOA->MODER &= ~(3 << (12 * 2));
  GPIOA->MODER |=  (2 << (12 * 2));
  GPIOA->AFR[1] &= ~(0xF << ((12 - 8) * 4));
  GPIOA->AFR[1] |=  (1   << ((12 - 8) * 4));

  // --- PB1: ADC1_IN9 analog input ---
  GPIOB->MODER |=  (3 << (1 * 2));
  GPIOB->PUPDR &= ~(3 << (1 * 2));

  // --- TIM1: ETR external clock → CC1 PWM output → ADC trigger ---
  TIM1->CR1   = 0;
  TIM1->CR2   = 0;
  TIM1->SMCR  = 0;
  TIM1->CCMR1 = 0;
  TIM1->CCER  = 0;

  // ETR: falling edge, no prescaler, no filter
  TIM1->SMCR |=  TIM_SMCR_ETP;        // invert ETR → falling edge on PA12
  TIM1->SMCR &= ~TIM_SMCR_ETPS;       // no prescaler
  TIM1->SMCR &= ~TIM_SMCR_ETF;        // no filter
  TIM1->SMCR |=  TIM_SMCR_ECE;        // external clock mode 2

  // CH1: PWM mode 1 → generates real edge for ADC trigger
  TIM1->CCMR1 &= ~TIM_CCMR1_CC1S;                    // CH1 = output
  TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
  TIM1->CCMR1 |=  (0b110 << TIM_CCMR1_OC1M_Pos);     // PWM mode 1
  TIM1->CCMR1 |=  TIM_CCMR1_OC1PE;                    // preload enable

  // CC1 output enable
  TIM1->CCER  |= TIM_CCER_CC1E;

  // Every ETR pulse → CNT 0→1→0 → OC1 toggles
  TIM1->ARR  = 1;
  TIM1->CCR1 = 1;

  TIM1->EGR  |= TIM_EGR_UG;    // load ARR/CCR1 into active registers
  TIM1->BDTR |= TIM_BDTR_MOE;  // required for TIM1
  TIM1->CR1  |= TIM_CR1_CEN;

  // --- ADC prescaler: /4 → 21 MHz ---
  ADC->CCR &= ~ADC_CCR_ADCPRE;
  ADC->CCR |=  ADC_CCR_ADCPRE_0;

  // --- ADC1: channel 9, TIM1_CC1 rising edge trigger ---
  ADC1->CR2 = 0;

  ADC1->SMPR2 &= ~(7 << (9 * 3));
  ADC1->SMPR2 |=  (0 << (9 * 3));    // 3 cycles

  ADC1->SQR1 = 0;
  ADC1->SQR3 = 9;

  ADC1->CR2 &= ~(ADC_CR2_EXTSEL_Msk | ADC_CR2_EXTEN_Msk);
  ADC1->CR2 |=  (0b0000 << ADC_CR2_EXTSEL_Pos);  // TIM1_CC1
  ADC1->CR2 |=  (0b01   << ADC_CR2_EXTEN_Pos);   // rising edge

  ADC1->CR2 |= ADC_CR2_ADON;
  for (volatile int i = 0; i < 1000; i++);

  // PA12 is already input via ETR, just add EXTI
  SYSCFG->EXTICR[3] &= ~(0xF << 0);   // EXTI12 → PA
  SYSCFG->EXTICR[3] |=  (0x0 << 0);

  EXTI->IMR  |=  (1 << 12);   // unmask line 12
  EXTI->RTSR |=  (1 << 12);   // rising edge
  EXTI->FTSR &= ~(1 << 12);   // not falling

  NVIC_SetPriority(EXTI15_10_IRQn, 0);
  NVIC_EnableIRQ(EXTI15_10_IRQn);

  // --- TIM4 PWM ---
  // Switch PB7 to alternate function mode
  GPIOB->MODER  &= ~(3 << (7 * 2));
  GPIOB->MODER  |=  (2 << (7 * 2));   // AF mode
  GPIOB->AFR[0] &= ~(0xF << (7 * 4));
  GPIOB->AFR[0] |=  (2   << (7 * 4)); // AF2 = TIM4
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  TIM4->CCR2 = 7000;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

//	    ADC1->CR2 |= ADC_CR2_SWSTART;  // force software trigger
//	    while (!(ADC1->SR & ADC_SR_EOC));  // wait for conversion
//	    val = ADC1->DR >> 4;
//	    HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 41999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
