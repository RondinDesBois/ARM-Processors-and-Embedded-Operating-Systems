/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
 */

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>

// TODO: insert other include files here
// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
//#include <string>
#include "DigitalIoPin.h"
#include <mutex>
#include "semphr.h"
#include <string.h>
#include <stdlib.h>
#define exo





/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
SemaphoreHandle_t mutex, binary, counter;
/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

// ** ex1 **
#ifdef exo1
static void vSW1Task(void *pvParameters){
	DigitalIoPin sw1(0, 17, DigitalIoPin::input, false);

	mutex = xSemaphoreCreateMutex();
	while (1) {
		//DEBUGOUT("test1\n");
		if (!sw1.read()) {
			if (mutex != NULL) {
				if (xSemaphoreTake(mutex, (TickType_t) 10) == pdTRUE) {  // attendre 10 ms et réessayer
					DEBUGOUT("sw1 pressed \r \n");
					xSemaphoreGive(mutex);
				}
			}
		}
		vTaskDelay(configTICK_RATE_HZ/10);
	}
}
static void vSW2Task(void *pvParameters){
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, false); // pull up pour le bouton 2 et 3

	mutex = xSemaphoreCreateMutex();
	while (1) {
		if (!sw2.read()) {
			if (mutex != NULL) {
				if (xSemaphoreTake(mutex, (TickType_t) 10) == pdTRUE) {  // attendre 10 ms et réessayer
					DEBUGOUT("sw2 pressed \r \n");
					xSemaphoreGive(mutex);
				}
			}
		}
		vTaskDelay(configTICK_RATE_HZ/10);
	}
}
static void vSW3Task(void *pvParameters){
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, false);

	mutex = xSemaphoreCreateMutex();
	while (1) {
		if (!sw3.read()) {
			if (mutex != NULL) {
				if (xSemaphoreTake(mutex, (TickType_t) 10) == pdTRUE) {  // attendre 10 ms et réessayer
					DEBUGOUT("sw3 pressed \r \n");
					xSemaphoreGive(mutex);
				}
			}
		}
		vTaskDelay(configTICK_RATE_HZ/10);
	}
}
#endif

/*****************************************************************************
 * ex2
 */
#ifdef exo2
static void vReadAndEchoTask(void *pvParameters){
	int read;

	while(1){

		if ((read = Board_UARTGetChar())!=EOF) {
			DEBUGOUT(" echo: %c \r \n", read);
			xSemaphoreGive(binary);
		}
		vTaskDelay(configTICK_RATE_HZ/10);
	}
}

static void vBlinksTask(void *pvParameters){
	bool LedState = false;

	while(1){
		if (xSemaphoreTake(binary, portMAX_DELAY)) {
			if (binary != NULL) {
				LedState = true;
				Board_LED_Set(0, LedState);
				vTaskDelay(configTICK_RATE_HZ/10);
				LedState = false;
				Board_LED_Set(0, LedState);
				vTaskDelay(configTICK_RATE_HZ/10);
			}
		}
	}
}
#endif
/*
 ****************************************************************************/

// ** ex3 **
#ifdef exo
static void vReadAndBackCharTask(void *pvParameters){
	int read,i=0;
	char string[60];
	bool question = false;

	while(1){
		if(xSemaphoreTake(mutex, portMAX_DELAY)){
			read = Board_UARTGetChar();
			xSemaphoreGive(mutex);
		}
		fflush(stdout);
		if(read!=EOF){
			string[i]=read;

			if(string[i]!='\n' && string[i]!='\r' && i<59){

				if(xSemaphoreTake(mutex, portMAX_DELAY)){
					DEBUGOUT("%c", string[i]);
					xSemaphoreGive(mutex);
				}


				if(string[i]=='?') question=true;
				i++;
			} else {
				string[i]='\0';

				if(xSemaphoreTake(mutex, portMAX_DELAY)){
					DEBUGOUT("\r [you]: %s \r \n", string);
					xSemaphoreGive(mutex);
				}
				i=0;

				if (question) {
					xSemaphoreGive(counter);
					vTaskDelay(10);
					question=false;
				}
			}
		} else {
			vTaskDelay(1);
		}

	}


}
static void vOracleTask(void *pvParameters){

	vTaskDelay(configTICK_RATE_HZ);
	while(1){

		if(xSemaphoreTake(counter, portMAX_DELAY)) {
			if(xSemaphoreTake(mutex, portMAX_DELAY)){
				DEBUGOUT("\n\r[Oracle]: Hummm \r \n\n");


				xSemaphoreGive(mutex);
			}

			//xSemaphoreGive(counter);
			vTaskDelay(configTICK_RATE_HZ*3);

			if(xSemaphoreTake(mutex, portMAX_DELAY)){
				switch((rand() %5)+1){
				case 1: DEBUGOUT("\n\r[Oracle]: why ? \r \n\n"); break;
				case 2: DEBUGOUT("\n\r[Oracle]: you are a penda!! \r \n\n"); break;
				case 3: DEBUGOUT("\n\r[Oracle]: you will fall in love soon... <3\r \n\n"); break;
				case 4: DEBUGOUT("\n\r[Oracle]: hahahaha x) \r \n\n"); break;
				case 5: DEBUGOUT("\n\r[Oracle]: you are not handsome :o \r \n\n"); break;

				}

				xSemaphoreGive(mutex);

			}

			//xSemaphoreGive(counter);
			vTaskDelay(configTICK_RATE_HZ*2);
		} else vTaskDelay(1);
	}
}

#endif
//***********************
extern "C" {
void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

}


int main(void)
{
	prvSetupHardware();

	mutex = xSemaphoreCreateMutex();
	binary = xSemaphoreCreateBinary();
	counter = xSemaphoreCreateCounting(10,0);

	// ** ex1 **
	//	xTaskCreate(vSW1Task,"vSW1Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);
	//	xTaskCreate(vSW2Task,"vSW2Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);
	//	xTaskCreate(vSW3Task,"vSW3Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);

	// ** ex2 **
	//	xTaskCreate(vReadAndEchoTask, "vReadAndEchoTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 2UL),
	//						(TaskHandle_t *) NULL);
	//
	//	xTaskCreate(vBlinksTask, "vBlinksTask", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);

	//ex3**
	xTaskCreate(vReadAndBackCharTask, "vReadAndBackCharTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	xTaskCreate(vOracleTask, "vOracleTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	//**

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

