/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include <time.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */n

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//--------enums
enum state {
	MENU,
	ENTERING_USERNAME,
	ENTERING_PASSWORD,
	NEW_USERNAME,
	NEW_PASSWORD,
	ENTERING_STATE,
	SYSTEM_ON,
	DOZD_DETECTED_USERNAME,
	DOZD_DETECTED_PASSWORD

};

enum daytime {
	DAY,
	NIGHT
};

enum buzzer_sound {
	SIGN,
	SQUARE,
	TRIANGLE
};




//-----------Global variables-----------\\


//--------general controlling variables
uint8_t current_state = MENU;
uint8_t daytime = NIGHT;
char username[4] = "----";
char username_to_check[4] = "----";
uint8_t password[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t pass_index = 0;
uint8_t password_to_check[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t pass_tck_index = 0;

//--------UART
char received_data[50];
uint8_t data_index = 0;
char receive;
char transmit_data[50];
uint8_t uart_mode = 1;
uint8_t log_state = 0;


//--------Buzzer
TIM_HandleTypeDef *buzzer_pwm_timer = &htim4;	// Point to PWM Timer configured in CubeMX
uint32_t buzzer_pwm_channel = TIM_CHANNEL_4;   // Select configured PWM channel number
volatile uint16_t volume = 100; // (0 - 1000)
uint32_t sound_timer = 0;
uint8_t alarm_on = 0;
uint32_t freq = 0;

//--------LED
TIM_HandleTypeDef *led_pwm_timer = &htim2;
uint32_t led_pwm_channel = TIM_CHANNEL_2;   // Select configured PWM channel number
uint8_t led_blink = 0;




uint8_t strcmpwithlength(const char * str1, const char * str2, const uint8_t len)
{
	for(int i = 0 ; i < len; ++i) {
		if(str1[i] != str2[i])
			return 0;
	}
	return 1;
}

uint8_t passcmp()
{
	for(int i = 0 ; i < pass_tck_index; ++i) {
		if(password[i] != password_to_check[i])
			return 0;
	}
	return 1;
}

uint8_t check_username() {
	for(int i = 0 ; i < 4; ++i) {
			if(username[i] != username_to_check[i])
				return 0;
		}
		return 1;
}



void PWM_Start()
{
    HAL_TIM_PWM_Start(buzzer_pwm_timer, buzzer_pwm_channel);
    HAL_TIM_PWM_Start(led_pwm_timer, led_pwm_channel);
}

void PWM_Change_Tone(uint16_t pwm_freq, uint16_t volume) // pwm_freq (1 - 20000), volume (0 - 1000)
{
    if (pwm_freq == 0 || pwm_freq > 20000)
    {
        __HAL_TIM_SET_COMPARE(buzzer_pwm_timer, buzzer_pwm_channel, 0);
    }
    else
    {
        const uint32_t internal_clock_freq = HAL_RCC_GetSysClockFreq();
        const uint16_t prescaler = 1;
        const uint32_t timer_clock = internal_clock_freq / prescaler;
        const uint32_t period_cycles = timer_clock / pwm_freq;
        const uint32_t pulse_width = volume * period_cycles / 1000 / 2;

        buzzer_pwm_timer->Instance->PSC = prescaler - 1;
        buzzer_pwm_timer->Instance->ARR = period_cycles - 1;
        buzzer_pwm_timer->Instance->EGR = TIM_EGR_UG;
        __HAL_TIM_SET_COMPARE(buzzer_pwm_timer, buzzer_pwm_channel, pulse_width); // buzzer_pwm_timer->Instance->CCR2 = pulse_width;
    }
}

void update_tone() {
	static int8_t i = 1;
	if(sound_timer < 1000) { //Square
		freq = (sound_timer / 100) % 2 ? 500 : 2000;
	} else if(sound_timer < 2000) { //Triangle
		freq = freq + (10 * i);
//		if(freq == 5000 || freq == 100)
//			i = i * -1;
		if(freq == 5000)
			freq = 1000;
	} else if(sound_timer < 3000) {
		freq = 5000;
	} else {
		sound_timer = 0;
	}
	++sound_timer;
	PWM_Change_Tone(freq, volume);
}



//  //--------Interrupts Functions--------\\

  //TIMERS
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	static int i = 1;
	if(htim->Instance == TIM2) {
		if(led_blink == 1) {
			htim2.Instance->CCR2 = htim2.Instance->CCR2 + (i * 100);
			if(htim2.Instance->CCR2 == 10000 || htim2.Instance->CCR2 == 0)
				i = i * -1;
		} else if(led_blink == 2) {
			if(sound_timer / 100 % 2 && htim2.Instance->CCR2 == 10000)
				htim2.Instance->CCR2 = 0;
			else if(sound_timer / 100 % 2 == 0)
				htim2.Instance->CCR2 = 10000;
		}
	}
	else if(htim->Instance == TIM6) {
		HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_10);
		switch(current_state) {
			case MENU :
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, 1);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, 0);
				htim2.Instance->CCR2 = 0;
				led_blink = 0;
				break;
			case ENTERING_PASSWORD:
			case ENTERING_STATE :
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, 0);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, 1);
				htim2.Instance->CCR2 = 0;
				led_blink = 0;
				break;
			case SYSTEM_ON :
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, 0);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, 0);
				led_blink = 1;
				break;
			case DOZD_DETECTED_PASSWORD:
			case DOZD_DETECTED_USERNAME:
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, 0);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, 0);
				led_blink = 2;
				if(daytime == DAY)
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, 1);
				else if(daytime == NIGHT)
					update_tone();
				break;
		}
	}
}

  //--------Buttons
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == GPIO_PIN_0 && current_state == SYSTEM_ON) { // PIR
		HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_8);
		current_state = DOZD_DETECTED_USERNAME;
		uart_log(11);
		return;
	}
	static last_interrupt = 0;
	if(HAL_GetTick() - last_interrupt < 250)
		return;

	last_interrupt = HAL_GetTick();

	if(current_state == NEW_PASSWORD) {
		if(GPIO_Pin == GPIO_PIN_1) { //[] [] [.]
			password[pass_index] = 3;
			++pass_index;
		} else if(GPIO_Pin == GPIO_PIN_2) { //[] [.] []
			password[pass_index] = 2;
			++pass_index;
		} else if(GPIO_Pin == GPIO_PIN_3) { //[.] [] []
			password[pass_index] = 1;
			++pass_index;
		}
		if(pass_index > 19)
			pass_index = 0;

	} else if (current_state == ENTERING_PASSWORD || current_state == DOZD_DETECTED_PASSWORD) {
		if(GPIO_Pin == GPIO_PIN_1) { //[] [] [.]
			password_to_check[pass_tck_index] = 3;
			++pass_tck_index;
		} else if(GPIO_Pin == GPIO_PIN_2) { //[] [.] []
			password_to_check[pass_tck_index] = 2;
			++pass_tck_index;
		} else if(GPIO_Pin == GPIO_PIN_3) { //[.] [] []
			password_to_check[pass_tck_index] = 1;
			++pass_tck_index;
		}
		if(pass_index > 19)
			pass_tck_index = 0;

	}
}

  //--------ADC
//  void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//  	if(hadc->Instance == ADC1) {
//  		static uint8_t sample_no = 0;
//  		static uint32_t samples_sum = 0;
//  		static uint16_t val;
//  		val = HAL_ADC_GetValue(&hadc1);
//  		samples_sum += val;
//  		++sample_no;
//  		if(sample_no == MAX_SAMPLE_NUMBER) {
//  			if(current_state == CHANGING_VOLUME) {
//  				potensiometer_value = samples_sum / MAX_SAMPLE_NUMBER / 40;
//  				volume = potensiometer_value;
//  				uart_log(4);
//  			} else if(current_state == CHANGING_SONG) {
//  				potensiometer_value = samples_sum / MAX_SAMPLE_NUMBER / 40;
//  				uart_log(2);
//  			} else if(current_state == CHANGING_SEVEN_SEGMENT_LIGHT_VOLUME) {
//  				potensiometer_value = 29 + (samples_sum / MAX_SAMPLE_NUMBER / 56);
//  		        __HAL_TIM_SET_COMPARE(seven_segment_light_timer, seven_segment_light_channel, potensiometer_value);
//  			}
//  			sample_no = 0;
//  			samples_sum = 0;
//  		}
//  	} else if(hadc->Instance == ADC2) {
//  		static uint8_t sample_no = 0;
//  		static uint32_t samples_sum = 0;
//  		static uint16_t val;
//  		val = HAL_ADC_GetValue(&hadc2);
//  		samples_sum += val;
//  		++sample_no;
//  		if(sample_no == MAX_SAMPLE_NUMBER) {
//  			potensiometer_value = 100 - (samples_sum / MAX_SAMPLE_NUMBER / 56);
//  	        __HAL_TIM_SET_COMPARE(seven_segment_light_timer, seven_segment_light_channel, potensiometer_value);
//  			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, 1);
//  			sample_no = 0;
//  			samples_sum = 0;
//  		}
//  	}
//  }
//



void uart_log(uint8_t state) {

	switch (state) {
		case 0:
			sprintf(transmit_data, "[SYSTM][Menu]\r\n[Sign in -> 1]\r\n[Log in  -> 2]\r\n");
			break;
		case 1:
			sprintf(transmit_data, "[ERROR][Wrong command]\r\n");
			break;
		case 2:
			sprintf(transmit_data, "[SYSTM][Enter new username:]\r\n");
			break;
		case 3:
			sprintf(transmit_data, "[SYSTM][Enter username:]\r\n");
			break;
		case 4:
			sprintf(transmit_data, "[SYSTM][Enter new password:]\r\n");
			break;
		case 5:
			sprintf(transmit_data, "[SYSTM][Enter password:]\r\n");
			break;
		case 6:
			sprintf(transmit_data, "[SYSTM][Enter on to turn on system:]\r\n");
			break;
		case 7:
			sprintf(transmit_data, "[ERROR][Usename not valid]\r\n[SYSTM][Menu]\r\n[Sign in -> 1]\r\n[Log in  -> 2]\r\n");
			break;
		case 8:
			sprintf(transmit_data, "[ERROR][Wrong password!]\r\n");
			break;
		case 9:
			sprintf(transmit_data, "[ERROR][Password not valid]\r\n");
			break;
		case 10:
			sprintf(transmit_data, "[SYSTM][System activated successfully.]\r\n[detecting DOZD!...]\r\n");
			break;
		case 11:
			sprintf(transmit_data, "[WRNNG][!!!DOZD DETECTED!!!]\r\n[!!!Enter username and password!!!]\r\n");
			break;
		default:
			break;
	}
	HAL_UART_Transmit(&huart1, transmit_data, strlen(transmit_data), 200);
}


//  --------UART
  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
  	if(huart->Instance == USART1) {
  		HAL_UART_Receive_IT(&huart1, &receive, 1);
		received_data[data_index] = receive;
		++data_index;

		if(receive == '\r') {
			if(current_state == MENU) {
				if(data_index > 2) {
					log_state = 1;
				} else {
					if(received_data[0] == '1') {
						current_state = NEW_USERNAME;
						log_state = 2;
					}
					else if (received_data[0] == '2') {
						current_state = ENTERING_USERNAME;
						log_state = 3;
					}
					else
						log_state = 1;
				}
			}
			else if(current_state == NEW_USERNAME) {
				if(data_index > 5) {
					log_state = 1;
				} else {
					username[0] = received_data[0];
					username[1] = received_data[1];
					username[2] = received_data[2];
					username[3] = received_data[3];
					current_state = NEW_PASSWORD;
					log_state = 4;
				}
			}
			else if(current_state == NEW_PASSWORD) {
				if(strcmpwithlength("done", received_data, 4) && pass_index > 3) {
					pass_index = 0;
					current_state = MENU;
					log_state = 0;
				}
				else if (pass_index <= 3)
					log_state = 9;
				else
					log_state = 1;
			}
			else if(current_state == ENTERING_USERNAME) {
				if(data_index > 5) {
					log_state = 7;
					current_state = MENU;
				} else {
					username_to_check[0] = received_data[0];
					username_to_check[1] = received_data[1];
					username_to_check[2] = received_data[2];
					username_to_check[3] = received_data[3];
					if(check_username()) {
						current_state = ENTERING_PASSWORD;
						log_state = 5;
					} else {
						log_state = 7;
						current_state = MENU;
					}
				}
			}
			else if(current_state == ENTERING_PASSWORD) {
				if(strcmpwithlength("done", received_data, 4)) {
					if(passcmp()) {
						current_state = ENTERING_STATE;
						log_state = 6;
					} else {
						log_state = 8;
					}
					pass_tck_index = 0;
				} else
					log_state = 1;
			}
			else if(current_state == ENTERING_STATE) {
				if(strcmpwithlength("on", received_data, 2)) {
					current_state = SYSTEM_ON;
					log_state = 10;
				} else if (strcmpwithlength("off", received_data, 3)) {
					current_state = MENU;
					log_state = 0;
				}
			}
			else if(current_state == DOZD_DETECTED_USERNAME) {
				if(data_index > 5) {
					log_state = 7;
				} else {
					username_to_check[0] = received_data[0];
					username_to_check[1] = received_data[1];
					username_to_check[2] = received_data[2];
					username_to_check[3] = received_data[3];
					if(check_username()) {
						current_state = DOZD_DETECTED_PASSWORD;
						log_state = 5;
					} else
						log_state = 7;
				}
			}
			else if(current_state == DOZD_DETECTED_PASSWORD) {
				if(strcmpwithlength("done", received_data, 4)) {
					if(passcmp()) {
						current_state = SYSTEM_ON;
						PWM_Change_Tone(0, 0);
						log_state = 10;
					} else {
						log_state = 8;
						volume = volume * 2;
					}
					pass_tck_index = 0;
				} else
					log_state = 1;
			}
			uart_log(log_state);
			data_index = 0;
		}
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_PCD_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start(&htim4);
  HAL_TIM_Base_Start_IT(&htim6);
  HAL_UART_Receive_IT(&huart1, &receive, 1);
  PWM_Start();
//  PWM_Change_Tone(4000, 100);
//  __HAL_TIM_SET_COMPARE(led_pwm_timer, led_pwm_channel, 100);

//  htim2.Instance->CCR2 = 100;



  uart_log(log_state);


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

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x2000090E;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 48;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 10000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
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
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 4800;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 100;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

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
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin|LD4_Pin|LD3_Pin|GPIO_PIN_10
                          |LD7_Pin|LD9_Pin|LD10_Pin|LD8_Pin
                          |LD6_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED1_GO_Pin|LED2_GO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RELAY_GO_GPIO_Port, RELAY_GO_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : CS_I2C_SPI_Pin LD4_Pin LD3_Pin PE10
                           LD7_Pin LD9_Pin LD10_Pin LD8_Pin
                           LD6_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin|LD4_Pin|LD3_Pin|GPIO_PIN_10
                          |LD7_Pin|LD9_Pin|LD10_Pin|LD8_Pin
                          |LD6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : MEMS_INT3_Pin MEMS_INT4_Pin */
  GPIO_InitStruct.Pin = MEMS_INT3_Pin|MEMS_INT4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PIR_GE_Pin B3_GE_Pin B2_GE_Pin
                           B3_GED3_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_12|PIR_GE_Pin|B3_GE_Pin|B2_GE_Pin
                          |B3_GED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_GO_Pin LED2_GO_Pin */
  GPIO_InitStruct.Pin = LED1_GO_Pin|LED2_GO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : RELAY_GO_Pin */
  GPIO_InitStruct.Pin = RELAY_GO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(RELAY_GO_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_TSC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_TSC_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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

#ifdef  USE_FULL_ASSERT
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
