/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    UART/UART_Printf/Src/main.c
  * @author  MCD Application Team
  * @brief   This example shows how to retarget the C library printf function
  *          to the UART.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "uwb_port.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"
#include <string.h>
#include "deca_version.h"
#include "deca_device_api.h"
#include "deca_interface.h"
#include "deca_probe_interface.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
#if defined(__ICCARM__)
__ATTRIBUTES size_t __write(int, const unsigned char *, size_t);
#endif /* __ICCARM__ */

#if defined(__ICCARM__)
/* New definition from EWARM V9, compatible with EWARM8 */
int iar_fputc(int ch);
#define PUTCHAR_PROTOTYPE int iar_fputc(int ch)
#elif defined ( __CC_ARM ) || defined(__ARMCC_VERSION)
/* ARM Compiler 5/6*/
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#elif defined(__GNUC__)
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#endif /* __ICCARM__ */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static volatile uint32_t spirdy_cb_count;
static volatile uint32_t spirdy_cb_status;   /* SYS_STATUS as seen by the driver when it called the callback */

static void cb_spirdy(const dwt_cb_data_t *d)
{
  spirdy_cb_count++;
  spirdy_cb_status = d->status;
}

/* read-only look at the chip; the driver's interrupt service is kept away from the SPI frame */
static void show_state(const char *tag)
{
  uint32_t st = 0, en = 0;
  decaIrqStatus_t m = decamutexon();
  int ok = uwb_port_read_status(&st, &en);
  decamutexoff(m);
  printf("%s: IRQ=%d STATUS=0x%08lX ENABLE_LO=0x%08lX%s\r\n", tag, uwb_port_irq_level(),
         (unsigned long)st, (unsigned long)en, (ok == UWB_OK) ? "" : "  (SPI ERROR)");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* STM32N6xx HAL library initialization:
       - Systick timer is configured by default as source of time base, but user
         can eventually implement his proper time base source (a general purpose
         timer for example or other time source), keeping in mind that Time base
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* Initialize BSP Led for LED2 */
  BSP_LED_Init(LED2);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* Task 2 Lv2: DW3000 driver + interrupt test. The driver's dwt_isr() is connected to the IRQ pin interrupt
     (EXTI12); we check that the SPIRDY start-up event is handled, IRQ goes low and the handler is not re-entered. */
  SystemCoreClockUpdate();
  printf("\r\n== T2 Lv2: DW3000 driver + interrupt test (dwt_isr, SPIRDY, EXTI12) ==\r\n");
  printf("%s\r\n", DRIVER_VERSION_STR);
  printf("DWT_SUCCESS=%d DWT_ERROR=%d (the adapters return 0 on success and -1 on error)\r\n", (int)DWT_SUCCESS, (int)DWT_ERROR);

  int steps_ok = 1;
  int rc = dw3000_hw_init();
  printf("dw3000_hw_init: %d (0 = RSTn released within 1 s)\r\n", rc);
  steps_ok &= (rc == 0);

  dw3000_spi_speed_slow();
  dw3000_hw_reset();
  rc = dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);
  printf("dwt_probe: %d (DWT_SUCCESS = %d)\r\n", rc, (int)DWT_SUCCESS);
  steps_ok &= (rc == (int)DWT_SUCCESS);

  /* Register reads through the driver (dwt_readdevid, dwt_checkidlerc, ...) read dw->priv, which only
     dwt_initialise() sets. Before it they dereference NULL, so wait for IDLE_RC with our own SPI read
     (SYS_STATUS bit 24 = RCINIT) and call the driver's register reads after dwt_initialise(). */
  uint32_t t0 = HAL_GetTick();
  uint32_t st = 0, en = 0;
  do
  {
    (void)uwb_port_read_status(&st, &en);
  } while (((st & (1UL << 24)) == 0U) && (HAL_GetTick() - t0) < 200U);
  printf("RCINIT (SYS_STATUS bit24) = %lu after %lu ms\r\n", (unsigned long)((st >> 24) & 1U), (unsigned long)(HAL_GetTick() - t0));

  rc = dwt_initialise(DWT_DW_INIT);
  printf("dwt_initialise: %d (DWT_SUCCESS = %d), SPI clock now %lu Hz\r\n", rc, (int)DWT_SUCCESS, (unsigned long)uwb_port_spi_hz());
  steps_ok &= (rc == (int)DWT_SUCCESS);

  uint32_t id = dwt_readdevid();
  printf("dwt_readdevid: 0x%08lX  %s\r\n", (unsigned long)id, (id == 0xDECA0302UL) ? "OK" : "UNEXPECTED");
  steps_ok &= (id == 0xDECA0302UL);
  printf("dwt_checkidlerc: %u\r\n", (unsigned)dwt_checkidlerc());

  if (steps_ok)
  {
    show_state("after dwt_initialise");

    dwt_callbacks_s cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.cbSPIRdy = cb_spirdy;
    dwt_setcallbacks(&cbs);
    dw3000_hw_isr_hook = dwt_isr;

    /* A) the IRQ pin is already high when the interrupt is enabled: a rising edge interrupt would never fire */
    printf("\r\n-- A) interrupt enabled while IRQ is already high (software pending path)\r\n");
    dw3000_spi_speed_slow();
    dw3000_hw_reset();   /* the interrupt is not enabled yet; the chip raises IRQ with its SPIRDY event */
    HAL_Delay(20);
    show_state("before enabling");
    memset((void *)&dw3000_hw_stats, 0, sizeof(dw3000_hw_stats));
    spirdy_cb_count = 0;
    dw3000_hw_init_interrupt();
    HAL_Delay(20);
    show_state("after enabling ");
    printf("handler entries=%lu, dwt_isr calls=%lu, SPIRDY callbacks=%lu (STATUS in callback 0x%08lX), max depth=%lu, ISR %lu cycles\r\n",
           (unsigned long)dw3000_hw_stats.isr_entries, (unsigned long)dw3000_hw_stats.hook_calls,
           (unsigned long)spirdy_cb_count, (unsigned long)spirdy_cb_status,
           (unsigned long)dw3000_hw_stats.max_depth, (unsigned long)dw3000_hw_stats.last_isr_cycles);
    int a_ok = (dw3000_hw_stats.isr_entries >= 1U) && (spirdy_cb_count == 1U) && (uwb_port_irq_level() == 0) &&
               (dw3000_hw_stats.max_depth == 1U);
    printf("A: %s\r\n", a_ok ? "PASS" : "FAIL");

    /* B) real rising edges: hardware reset with the interrupt enabled */
    printf("\r\n-- B) real rising edge: hardware reset with the interrupt enabled, 20 times\r\n");
    memset((void *)&dw3000_hw_stats, 0, sizeof(dw3000_hw_stats));
    uint32_t b_ok = 0, max_entries_per_reset = 0;
    for (uint32_t n = 1; n <= 20U; n++)
    {
      uint32_t e0 = dw3000_hw_stats.isr_entries, h0 = dw3000_hw_stats.hook_calls, c0 = spirdy_cb_count;
      dw3000_spi_speed_slow();
      dw3000_hw_reset();
      HAL_Delay(30);
      uint32_t de = dw3000_hw_stats.isr_entries - e0, dh = dw3000_hw_stats.hook_calls - h0, dc = spirdy_cb_count - c0;
      int irq = uwb_port_irq_level();
      uint32_t st = 0, en = 0;
      decaIrqStatus_t m = decamutexon();
      (void)uwb_port_read_status(&st, &en);
      decamutexoff(m);
      int ok = (de >= 1U) && (dc == 1U) && (irq == 0);
      if (ok)
      {
        b_ok++;
      }
      if (de > max_entries_per_reset)
      {
        max_entries_per_reset = de;
      }
      printf("#%02lu entries+%lu isr+%lu SPIRDY+%lu IRQ=%d STATUS=0x%08lX ISR=%lu cycles  %s\r\n", (unsigned long)n,
             (unsigned long)de, (unsigned long)dh, (unsigned long)dc, irq, (unsigned long)st,
             (unsigned long)dw3000_hw_stats.last_isr_cycles, ok ? "OK" : "FAIL");
    }
    printf("B: %lu/20 resets handled (one SPIRDY callback, IRQ low). Max handler entries per reset=%lu, max depth=%lu, loop limit hits=%lu, longest ISR=%lu cycles (%lu us)\r\n",
           (unsigned long)b_ok, (unsigned long)max_entries_per_reset, (unsigned long)dw3000_hw_stats.max_depth,
           (unsigned long)dw3000_hw_stats.loop_limit_hits, (unsigned long)dw3000_hw_stats.max_isr_cycles,
           (unsigned long)(dw3000_hw_stats.max_isr_cycles / (SystemCoreClock / 1000000U)));
    int b_pass = (b_ok == 20U) && (dw3000_hw_stats.max_depth == 1U) && (dw3000_hw_stats.loop_limit_hits == 0U);
    printf("B: %s\r\n", b_pass ? "PASS" : "FAIL");
    printf("\r\nverdict: %s\r\n", (a_ok && b_pass) ? "PASS" : "FAIL");
  }
  else
  {
    printf("\r\ndriver start-up failed, the interrupt tests were skipped\r\nverdict: FAIL\r\n");
  }
  printf("== done ==\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */
}
/* USER CODE BEGIN CLK 1 */
/* USER CODE END CLK 1 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the System Power Supply
  */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable HSI */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Get current CPU/System buses clocks configuration and if necessary switch
 to intermediate HSI clock to ensure target clock can be set
  */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
     (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
      /* Initialization Error */
      Error_Handler();
    }
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 4;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
                              |RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 4;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_ODD;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

#if defined(__ICCARM__)
size_t __write(int file, unsigned char const *ptr, size_t len)
{
  size_t idx;
  unsigned char const *pdata = ptr;

  for (idx = 0; idx < len; idx++)
  {
    iar_fputc((int)*pdata);
    pdata++;
  }
  return len;
}
#endif /* __ICCARM__ */



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  /* Turn LED2 on */
  BSP_LED_On(LED2);
  while (1);
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

  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
