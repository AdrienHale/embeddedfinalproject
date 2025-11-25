/* Includes ------------------------------------------------------------------*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "app.h"

/* Private define ------------------------------------------------------------*/
#define 	LED_PORT 			GPIOA
#define 	LED_PIN 			GPIO_PIN_5

#define		LED1_PORT			GPIOA
#define		LED1_PIN			GPIO_PIN_6

#define		LED2_PORT			GPIOA
#define		LED2_PIN			GPIO_PIN_7

#define		LED3_PORT			GPIOB
#define		LED3_PIN			GPIO_PIN_6

#define		LEDA_PORT			GPIOC
#define		LEDA_PIN			GPIO_PIN_4

#define		LED_STATE_OFF		0 //These are for the distance sensor LEDs
#define		LED_STATE_ON		1
#define		LED_STATE_FLASHING	2

#define		STATE_CALM			0 //These are for the photoresistor
#define		STATE_THREAT		1

/* Private function prototypes -----------------------------------------------*/
void UART_TransmitString(UART_HandleTypeDef *p_huart, const char *str) {
    HAL_UART_Transmit(p_huart, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}
void DistanceLEDs(float mv);
void WeightLEDs(float mv);

/* Private variables ---------------------------------------------------------*/
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

//Should be declared as volatile if variables' values are changed in ISR.
volatile char rxData;  //One byte data received from UART
volatile int takeSample = 0;

volatile int led1State = LED_STATE_OFF; //For distance sensor LEDs
volatile int led2State = LED_STATE_OFF;
volatile int led3State = LED_STATE_OFF;

volatile float startWeight = 0;


void App_Init(void) {
	uint32_t adcValues[3] = {0, 0, 0};
	HAL_TIM_Base_Start_IT(&htim2);
	HAL_UART_Receive_IT(&huart2, (uint8_t*) &rxData, 1); //Start the Rx interrupt.

	HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);

    	UART_TransmitString(&huart2, " Light Monitor\r\n");
    	UART_TransmitString(&huart2, "System starting...\r\n");

	HAL_ADC_Start(&hadc3);
				if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK)
				adcValues[2] = HAL_ADC_GetValue(&hadc3); //PC4
			HAL_ADC_Stop(&hadc3);
	startWeight = ((float)adcValues[2]) * 3300.0/0x0FFF;

}

void App_MainLoop(void) {
    while (1) {
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *p_htim) {

	if (p_htim->Instance != TIM2) return;

    	static int lastState = -1;
    	uint32_t adcValues[3] = {0, 0, 0};
    	float mvLight = 0;
	float mvDistance = 0;
	float mvWeight = 0;

	if (led1State == LED_STATE_FLASHING) { //LED1
		HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
	}

	if (led2State == LED_STATE_FLASHING) { //LED2
		HAL_GPIO_TogglePin(LED2_PORT, LED2_PIN);
	}

	if (led3State == LED_STATE_FLASHING) { //LED3
		HAL_GPIO_TogglePin(LED3_PORT, LED3_PIN);
	}

	HAL_ADC_Start(&hadc1);
	if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        	adcValues[0] = HAL_ADC_GetValue(&hadc1); //PC2
	HAL_ADC_Stop(&hadc1);

	HAL_ADC_Start(&hadc2);
    	if (HAL_ADC_PollForConversion(&hadc2, 10) == HAL_OK)
        adcValues[1] = HAL_ADC_GetValue(&hadc2); //PC3
	HAL_ADC_Stop(&hadc2);

	HAL_ADC_Start(&hadc3);
	    	if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK)
	        adcValues[2] = HAL_ADC_GetValue(&hadc3); //PC4
		HAL_ADC_Stop(&hadc3);

	mvDistance = ((float)adcValues[0]) * 3300.0 / 0x0FFF; // PC2
    	mvLight = ((float)adcValues[1]) * 3300.0f / 4095.0f; // PC3

    mvWeight = ((float)adcValues[2]) * 3300.0/0x0FFF;

	DistanceLEDs(mvDistance);
	WeightLEDs(mvWeight);

	// Determine light based security states using mV conversion
    	int state = (mvLight > 500.0f) ? STATE_THREAT : STATE_CALM;

	// Only update if state changed
    	if (state != lastState) {
        	lastState = state;

		// PuTTy output (can switch to LEDs as well)
    		if (state == STATE_THREAT) {
    			HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
   			UART_TransmitString(&huart2, "WARNING: THREAT PERCEIVED, PLEASE SECURE ITEM \r\n");
    		}
 		else {
    		HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    		UART_TransmitString(&huart2, "STATE: CALM, PROCEED ACCORDINGLY\r\n");
        	}
    	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *p_huart) {
	//Process the data received from UART.
		HAL_UART_Receive_IT(p_huart, (uint8_t*) &rxData, 1); //Restart the Rx interrupt.
}

void WeightLEDs (float mvWeight) {
	if (startWeight-mvWeight > 20) {
		HAL_GPIO_TogglePin(LEDA_PORT, LEDA_PIN);
	}

	else if (startWeight-mvWeight < -20) {
		HAL_GPIO_WritePin(LEDA_PORT, LEDA_PIN, GPIO_PIN_SET);
	}

	else {
		HAL_GPIO_WritePin(LEDA_PORT, LEDA_PIN, GPIO_PIN_RESET);

	}
}

void DistanceLEDs(float mvDistance) {
	if (mvDistance <= 550.0f) {
	    led1State = LED_STATE_ON;
	    led2State = LED_STATE_OFF;
	    led3State = LED_STATE_OFF;
	}
	else if (mvDistance > 550.0f && mvDistance <= 700.0f) {
		led1State = LED_STATE_ON;
		led2State = LED_STATE_FLASHING;
		led3State = LED_STATE_OFF;
	}
	else if (mvDistance > 700.0f && mvDistance <= 950.0f) {
		led1State = LED_STATE_OFF;
		led2State = LED_STATE_ON;
		led3State = LED_STATE_OFF;
	}
	else if (mvDistance > 950.0f && mvDistance <= 1860.0f) {
		led1State = LED_STATE_OFF;
		led2State = LED_STATE_ON;
		led3State = LED_STATE_FLASHING;
	}
	else {
		led1State = LED_STATE_OFF;
		led2State = LED_STATE_OFF;
		led3State = LED_STATE_ON;
	}


	if (led1State == LED_STATE_ON) {
	    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
	}
	else if (led1State == LED_STATE_OFF) {
	    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
	}

	if (led2State == LED_STATE_ON) {
	    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
	}
	else if (led2State == LED_STATE_OFF) {
	    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
	}

	if (led3State == LED_STATE_ON) {
	    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
	}
	else if (led3State == LED_STATE_OFF) {
	    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
	}
}
