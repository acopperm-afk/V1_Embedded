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

/* =============================================================================
 * WHAT THIS PROGRAM DOES (plain English)
 * =============================================================================
 * This firmware runs on a STM32G474RE Nucleo-64 board and does three things:
 *
 *  1. SOUND DETECTION
 *     Reads a sound sensor connected to pin D2 (PA10). The sensor pulls that
 *     pin LOW when it hears a sound loud enough to cross its threshold.
 *     It also reads the sensor's raw analog voltage on A0 (PA0) so we can
 *     see the actual signal level over UART.
 *
 *  2. SERVO CONTROL
 *     Every time a sound is detected (max once per second), a SG90 micro servo
 *     on pin D9 (PC7) rotates 45 degrees clockwise. When it hits the +90 degree
 *     limit it reverses direction, bouncing back and forth like a pendulum.
 *
 *  3. UART LOGGING
 *     Sends a human-readable status line over USB serial (COM3, 9600 baud)
 *     every ~200ms so you can watch what's happening in a terminal.
 *     Example output:  Analog: 2.186V, Sound: quiet
 *                      Analog: 2.318V, Sound: DETECTED
 *
 * WIRING SUMMARY
 * --------------
 *  Sound sensor VCC  → 3.3V  (CN6 pin 4)
 *  Sound sensor GND  → GND   (CN6 pin 6)
 *  Sound sensor DO   → D2    (CN9 pin 3  = PA10)
 *  Sound sensor AO   → A0    (CN8 pin 1  = PA0)
 *
 *  Servo red wire    → 5V    (CN6 pin 5)
 *  Servo black wire  → GND   (CN6 pin 6)
 *  Servo orange wire → D9    (CN5 pin 2  = PC7)
 * =============================================================================
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stm32g4xx_hal_flash.h>
#include <stm32g4xx_hal_rcc.h>
  /* USER CODE BEGIN SysInit */
#include <stdio.h>               // snprintf
#include <string.h>              // strlen
#include <stm32g4xx_hal_uart.h>  // UART (serial communication)
#include <stm32g4xx_hal_adc.h>   // ADC  (analog-to-digital converter)
#include <stm32g4xx_hal_tim.h>   // TIM  (timer, used to generate servo PWM)
#include <stm32g4xx_hal_pwr_ex.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* HAL_Init sets up the HAL library, enables the SysTick timer (which gives
   * us HAL_GetTick() / HAL_Delay()), and configures the NVIC priority grouping.
   * Always the first thing called. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* SystemClock_Config sets the CPU to run from the internal 16 MHz HSI
   * oscillator. All peripheral clocks derive from this. See the function
   * at the bottom of this file for details. */
  SystemClock_Config();

  /* USER CODE BEGIN PV */

  /* -------------------------------------------------------------------------
   * PERIPHERAL HANDLES
   * -------------------------------------------------------------------------
   * A "handle" is a struct that holds all the configuration and state for one
   * peripheral. We declare them static so they live in RAM for the entire
   * program lifetime and are zero-initialised at startup (no garbage values).
   * ------------------------------------------------------------------------- */
  static ADC_HandleTypeDef  hadc1;   // ADC1  — reads analog voltage on PA0
  static UART_HandleTypeDef huart2;  // LPUART1 — sends text to your PC over USB
  static TIM_HandleTypeDef  htim3;   // TIM3  — generates the servo PWM signal

  /* =========================================================================
   * GPIO SETUP — PA5 (onboard LED, D13)
   * =========================================================================
   * Before we can use any GPIO pin we must turn on the clock for that GPIO
   * bank. Think of it like flipping the power switch for that group of pins.
   * Then we fill in a config struct and call HAL_GPIO_Init.
   *
   * PA5 is the green USER LED (LD2) on the Nucleo board.
   * Mode OUTPUT_PP = push-pull output (can drive HIGH or LOW).
   * We toggle it every time a sound is detected as a visual indicator.
   * ========================================================================= */
  __HAL_RCC_GPIOA_CLK_ENABLE();   // turn on the clock for GPIO port A

  GPIO_InitTypeDef GPIO_InitStruct = {0};  // zero-init to avoid stale values
  GPIO_InitStruct.Pin   = GPIO_PIN_5;           // PA5
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  // push-pull digital output
  GPIO_InitStruct.Pull  = GPIO_NOPULL;          // no internal resistor needed
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // slow slew rate, fine for LED
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* =========================================================================
   * GPIO SETUP — PA10 (sound sensor digital output, D2)
   * =========================================================================
   * The sound sensor has a comparator inside. When the mic signal crosses the
   * threshold set by the pot, the DO pin is pulled LOW (active-low logic).
   * At rest (quiet) DO floats or is driven HIGH.
   *
   * We configure PA10 as an input with the internal pull-up resistor enabled.
   * The pull-up ensures the pin reads HIGH when the sensor is idle, so we
   * only see LOW when the sensor actually fires.
   * ========================================================================= */
  GPIO_InitStruct.Pin   = GPIO_PIN_10;      // PA10 = Arduino D2
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;  // digital input
  GPIO_InitStruct.Pull  = GPIO_PULLUP;      // pull HIGH at rest (sensor is active-low)
  GPIO_InitStruct.Speed = 0;                // speed field is ignored for inputs
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* =========================================================================
   * ADC SETUP — ADC1 channel 1 on PA0 (Arduino A0)
   * =========================================================================
   * The ADC (Analog-to-Digital Converter) measures a voltage and converts it
   * to a number. With 12-bit resolution the result is 0–4095, where:
   *   0    = 0 V
   *   4095 = 3.3 V (our reference voltage)
   *
   * We use this to read the sound sensor's analog output (AO), which shows
   * the raw amplified microphone signal as a voltage. This lets us see the
   * actual sound level even when it hasn't crossed the DO threshold.
   *
   * Steps:
   *  1. Enable the ADC clock
   *  2. Configure PA0 as analog (no pull, no drive — just measure)
   *  3. Configure ADC1 itself (resolution, trigger source, etc.)
   *  4. Configure which channel to sample (channel 1 = PA0)
   *  5. Run the self-calibration routine for accuracy
   * ========================================================================= */
  __HAL_RCC_ADC12_CLK_ENABLE();   // turn on the clock for ADC1 and ADC2

  GPIO_InitStruct.Pin  = GPIO_PIN_0;       // PA0 = Arduino A0
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG; // analog mode — no digital logic
  GPIO_InitStruct.Pull = GPIO_NOPULL;      // no pull resistor on analog pins
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;      // ADC clock = PCLK/2 = 8 MHz
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;             // 12-bit = 0 to 4095
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;            // result in the low bits of the register
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;               // only one channel, no scan needed
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;            // flag when one conversion is done
  hadc1.Init.ContinuousConvMode    = DISABLE;                        // convert once per software trigger
  hadc1.Init.NbrOfConversion       = 1;                              // just one channel
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;             // we start it manually in the loop
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.OversamplingMode      = DISABLE;
  HAL_ADC_Init(&hadc1);

  // Tell ADC1 which physical pin/channel to sample
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel      = ADC_CHANNEL_1;          // PA0 maps to ADC channel 1
  sConfig.Rank         = ADC_REGULAR_RANK_1;     // first (and only) in the sequence
  sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5; // sample for ~47 clock cycles (more = more accurate)
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);

  // Self-calibration: the ADC measures its own internal offset and corrects for it.
  // Call this once after init for better accuracy.
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

  /* =========================================================================
   * UART SETUP — LPUART1 on PA2(TX)/PA3(RX) → USB Virtual COM port
   * =========================================================================
   * UART is a simple serial protocol that sends text one bit at a time.
   * The Nucleo's onboard ST-Link chip bridges LPUART1 to a USB "Virtual COM
   * port" on your PC — that's the COM3 port you open in your terminal.
   *
   * Settings must match your terminal exactly:
   *   Baud rate : 9600  (bits per second)
   *   Word length: 8 bits
   *   Stop bits : 1
   *   Parity    : none
   *
   * On the STM32G4, LPUART1 needs its own clock source configured explicitly.
   * We route it to the HSI (16 MHz internal oscillator) so the baud rate
   * divider calculation is based on a known, stable frequency.
   *
   * PA2 and PA3 are set to "Alternate Function" mode — instead of being
   * plain GPIO they are internally connected to the LPUART1 peripheral.
   * GPIO_AF12_LPUART1 tells the pin mux which peripheral to route to.
   * ========================================================================= */
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  PeriphClkInit.PeriphClockSelection  = RCC_PERIPHCLK_LPUART1;      // configure LPUART1's clock source
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_HSI;   // use the 16 MHz internal oscillator
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  __HAL_RCC_LPUART1_CLK_ENABLE();   // turn on the LPUART1 peripheral clock

  // Configure PA2 (TX) and PA3 (RX) as alternate function pins for LPUART1
  GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;  // both pins at once
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;           // alternate function, push-pull
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;         // connect these pins to LPUART1
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart2.Instance          = LPUART1;
  huart2.Init.BaudRate     = 9600;                  // must match your terminal
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;    // 8 data bits per byte
  huart2.Init.StopBits     = UART_STOPBITS_1;       // 1 stop bit
  huart2.Init.Parity       = UART_PARITY_NONE;      // no parity bit
  huart2.Init.Mode         = UART_MODE_TX_RX;       // enable both transmit and receive
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;   // no hardware flow control
  HAL_UART_Init(&huart2);

  /* =========================================================================
   * PWM / TIMER SETUP — TIM3 CH2 on PC7 (Arduino D9) for SG90 servo
   * =========================================================================
   * A servo is controlled by PWM (Pulse Width Modulation): a repeating pulse
   * where the WIDTH of the HIGH portion tells the servo what angle to go to.
   *
   * SG90 servo PWM spec:
   *   Period    : 20 ms  (the pulse repeats 50 times per second)
   *   -90 deg   : 1.0 ms HIGH pulse  (CCR = 1000)
   *     0 deg   : 1.5 ms HIGH pulse  (CCR = 1500)  ← centre
   *   +90 deg   : 2.0 ms HIGH pulse  (CCR = 2000)
   *
   * We use TIM3 (a general-purpose timer) to generate this signal on PC7.
   *
   * Timer maths:
   *   System clock = 16 MHz (HSI)
   *   Prescaler    = 15  →  timer clock = 16 MHz / (15+1) = 1 MHz
   *   At 1 MHz, each timer tick = 1 microsecond (µs)
   *   Period       = 19999  →  timer resets every 20,000 ticks = 20 ms ✓
   *   CCR value    = pulse width in µs (e.g. 1500 = 1.5 ms = centre)
   *
   * PC7 is set to "Alternate Function" mode with GPIO_AF2_TIM3, which
   * internally connects the pin to TIM3's channel 2 output.
   * ========================================================================= */
  __HAL_RCC_GPIOC_CLK_ENABLE();   // turn on the clock for GPIO port C (PC7 is on port C)
  __HAL_RCC_TIM3_CLK_ENABLE();    // turn on the clock for TIM3

  // Configure PC7 as a PWM output pin connected to TIM3
  GPIO_InitStruct.Pin       = GPIO_PIN_7;        // PC7 = Arduino D9
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;   // alternate function, push-pull
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;     // route PC7 to TIM3
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // Configure TIM3 base: how fast it counts and when it resets
  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 15;                          // divide 16 MHz by 16 → 1 MHz tick
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;          // count upward 0 → Period
  htim3.Init.Period            = 19999;                       // reset at 20,000 → 20 ms frame
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;      // no extra clock division
  htim3.Init.RepetitionCounter = 0;                           // not used on general-purpose timers
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&htim3);

  // Configure channel 2 of TIM3 as a PWM output
  // PWM1 mode: pin is HIGH while counter < CCR, LOW while counter >= CCR
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode     = TIM_OCMODE_PWM1;        // standard PWM mode
  sConfigOC.Pulse      = 1500;                   // start at centre (0 degrees)
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;    // pulse is active-HIGH
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);

  // Start the PWM signal — from this point the servo receives pulses continuously
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* =========================================================================
   * STARTUP BANNER
   * =========================================================================
   * Send a one-time message over UART as soon as init is complete.
   * If you see this in your terminal, you know:
   *   - The MCU booted successfully
   *   - UART is working
   *   - All peripherals initialised without crashing
   * If you DON'T see it, something failed before this line.
   * ========================================================================= */
  const char *banner =
      "\r\n=== Blink Firmware Ready ===\r\n"
      "Servo : TIM3 CH2 -> PC7 (D9)\r\n"
      "Sound : PA10 (D2) digital, PA0 (A0) analog\r\n"
      "UART  : 9600 8N1\r\n"
      "============================\r\n\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)banner, strlen(banner), 500);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  /* =========================================================================
   * SERVO STATE VARIABLES
   * =========================================================================
   * These track where the servo currently is and which way it's moving.
   * They live outside the while(1) loop so they persist between iterations.
   *
   * servo_pos_us : current pulse width in microseconds
   *                1000 = -90°, 1500 = 0° (centre), 2000 = +90°
   *                Each 45° step = 125 µs
   *
   * servo_dir    : +1 means moving clockwise (increasing pulse width)
   *                -1 means moving anticlockwise (decreasing pulse width)
   *                Flips when the servo hits either end stop.
   *
   * last_move_ms : timestamp (in ms) of the last servo move.
   *                Used to enforce the 1-move-per-second rate limit.
   * ========================================================================= */
  int32_t  servo_pos_us = 1500;  // start at centre (0 degrees)
  int8_t   servo_dir    = 1;     // start moving clockwise
  uint32_t last_move_ms = 0;     // no move yet (HAL_GetTick() starts at 0)

  while (1)
  {
    /* USER CODE END WHILE */

    /* -----------------------------------------------------------------------
     * STEP 1: READ THE ADC (analog sound level)
     * -----------------------------------------------------------------------
     * HAL_ADC_Start kicks off one conversion.
     * HAL_ADC_PollForConversion waits up to 10 ms for it to finish.
     * HAL_ADC_GetValue returns the raw 12-bit result (0–4095).
     *
     * We convert raw counts to millivolts using integer arithmetic:
     *   voltage_mv = raw * 3300 / 4095
     * This avoids floating point (which is slow and needs extra library code).
     * Example: raw=1861 → 1861*3300/4095 ≈ 1500 mV = 1.500 V
     * ----------------------------------------------------------------------- */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);          // wait up to 10 ms
    uint32_t raw        = HAL_ADC_GetValue(&hadc1); // 0–4095
    uint32_t voltage_mv = (raw * 3300UL) / 4095UL;  // convert to millivolts

    /* -----------------------------------------------------------------------
     * STEP 2: READ THE DIGITAL SOUND DETECTION PIN (PA10 / D2)
     * -----------------------------------------------------------------------
     * GPIO_PIN_RESET means the pin is LOW  → sound detected (active-low)
     * GPIO_PIN_SET   means the pin is HIGH → quiet
     * ----------------------------------------------------------------------- */
    GPIO_PinState digital = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);

    /* -----------------------------------------------------------------------
     * STEP 3: REACT TO SOUND DETECTION
     * -----------------------------------------------------------------------
     * Only runs when the sensor pulls DO low (sound detected).
     * ----------------------------------------------------------------------- */
    if (digital == GPIO_PIN_RESET)
    {
      // Toggle the onboard LED as a visual indicator
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

      /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
       * SERVO MOVE — max once per second
       * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
       * HAL_GetTick() returns milliseconds since boot.
       * We only move the servo if at least 1000 ms have passed since the
       * last move. This prevents rapid-fire triggers from spinning the servo
       * continuously on sustained noise.
       * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
      uint32_t now = HAL_GetTick();
      if ((now - last_move_ms) >= 1000)
      {
        last_move_ms = now;

        // Move 45 degrees in the current direction (125 µs = 45°)
        servo_pos_us += servo_dir * 125;

        /* Boundary check: if we've hit either end stop, clamp the position
         * and reverse direction so the servo bounces back the other way.
         *
         *  +90° limit = 2000 µs → reverse to anticlockwise (dir = -1)
         *  -90° limit = 1000 µs → reverse to clockwise     (dir = +1)  */
        if (servo_pos_us >= 2000)
        {
          servo_pos_us = 2000;
          servo_dir    = -1;
        }
        else if (servo_pos_us <= 1000)
        {
          servo_pos_us = 1000;
          servo_dir    = 1;
        }

        /* Write the new pulse width to TIM3 channel 2.
         * __HAL_TIM_SET_COMPARE updates the CCR register directly — the
         * timer picks up the new value on the next PWM cycle (within 20 ms). */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, (uint32_t)servo_pos_us);
      }

      // Debounce: wait 500 ms before checking the sensor again.
      // Without this, one loud clap could trigger dozens of reads in a row.
      HAL_Delay(500);
    }

    /* -----------------------------------------------------------------------
     * STEP 4: SEND STATUS OVER UART
     * -----------------------------------------------------------------------
     * Build a string like "Analog: 1.426V, Sound: quiet\r\n" and transmit it.
     *
     * snprintf writes into buf (max 80 chars) and returns the number of
     * characters written (stored in len).
     *
     * We split voltage_mv into whole volts and millivolt remainder:
     *   voltage_mv = 1426
     *   1426 / 1000 = 1      (whole volts)
     *   1426 % 1000 = 426    (millivolts remainder)
     *   → "1.426V"
     *
     * %03u pads the remainder with leading zeros so 1.006V prints correctly
     * instead of 1.6V.
     *
     * HAL_UART_Transmit sends len bytes from buf, blocking for up to 100 ms.
     * ----------------------------------------------------------------------- */
    char buf[80];
    int len = snprintf(buf, sizeof(buf),
        "Analog: %u.%03uV, Sound: %s\r\n",
        (unsigned int)(voltage_mv / 1000UL),   // whole volts
        (unsigned int)(voltage_mv % 1000UL),   // millivolt remainder (3 digits)
        (digital == GPIO_PIN_RESET) ? "DETECTED" : "quiet");
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, len, 100);

    // Wait 200 ms before the next loop iteration (~5 readings per second)
    HAL_Delay(200);

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* =============================================================================
 * SYSTEM CLOCK CONFIGURATION
 * =============================================================================
 * Configures the STM32G4 to run from the internal HSI oscillator at 16 MHz.
 *
 * The STM32G4 has several clock sources. We use HSI (High Speed Internal),
 * a factory-trimmed RC oscillator built into the chip. It's not as accurate
 * as an external crystal but is good enough for UART and servo timing.
 *
 * PWR_REGULATOR_VOLTAGE_SCALE1 sets the internal voltage regulator to its
 * highest performance mode, required when running at full speed.
 *
 * FLASH_LATENCY_0 means zero wait states when reading flash — fine at 16 MHz
 * (would need more wait states at higher frequencies like 170 MHz).
 * ============================================================================= */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Set the internal voltage regulator to full performance mode
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  // Turn on the HSI oscillator (16 MHz internal RC)
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT; // factory trim
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;               // PLL not used
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();  // if the oscillator fails to start, halt
  }

  // Route HSI to SYSCLK, and set all bus dividers to 1 (no division)
  // HCLK = SYSCLK = PCLK1 = PCLK2 = 16 MHz
  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;  // use HSI as system clock
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;       // AHB  = 16 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;         // APB1 = 16 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;         // APB2 = 16 MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();  // if clock config fails, halt
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* =============================================================================
 * ERROR HANDLER
 * =============================================================================
 * Called whenever a HAL function returns HAL_ERROR (e.g. clock config fails).
 * Disables all interrupts and loops forever so the problem doesn't go unnoticed.
 * In a production system you'd log the error or blink an LED here.
 * ============================================================================= */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();  // stop all interrupts so nothing else runs
  while (1)
  {
    // Stuck here forever — attach a debugger to find out what went wrong
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
