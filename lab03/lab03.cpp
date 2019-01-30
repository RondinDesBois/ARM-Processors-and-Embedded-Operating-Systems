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

#include "FreeRTOS.h"
#include "task.h"
//#include <string>
#include "DigitalIoPin.h"
#include <mutex>
#include "semphr.h"
#include <string.h>
#include <stdlib.h>
#include "ITM_write.h"

// TODO: insert other definitions and declarations here

SemaphoreHandle_t mutex, binary, counter;
QueueHandle_t queue;

void debug(char *format, uint32_t d1, uint32_t d2, uint32_t d3);

struct debugEvent {
	char *format;
	uint32_t data[3];
};



/*****************************************************************************
 ***************************** lab 03 functions ******************************
 ****************************************************************************/
/*-----------------------------------ex1------------------------------------*/
#if 0
static void vReadAndCountsTask(void *pvParameters){
	int read,i=0, *count = &i;
	char string[60];

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
				i++;
			} else {
				string[i]='\0';
				if(xQueueSendToBack(queue, count, portMAX_DELAY)) {
					if(xSemaphoreTake(mutex, portMAX_DELAY)){
						DEBUGOUT("\r [you]: %s \r \n", string);
						xSemaphoreGive(mutex);
					}
				}
				i=0;
			}
		} else {
			vTaskDelay(1);
		}

	}
}

static void vSW1Task(void *pvParameters){
	DigitalIoPin sw1(0, 17, DigitalIoPin::input, false);
	int fin = -1;


	while (1) {
		if (!sw1.read()) {
			xQueueSendToBack(queue, &fin, portMAX_DELAY);
			vTaskDelay(configTICK_RATE_HZ);
		}
		vTaskDelay(1);
	}
}

static void vWaitQueueTask(void *pvParameters){
	int sum=0, receivesum=0;
	vTaskDelay(10);
	while(1){

		if(xQueueReceive(queue, &receivesum, portMAX_DELAY)) {

			if (receivesum != EOF) {
				sum = receivesum + sum;
			} else {
				if (xSemaphoreTake(mutex, portMAX_DELAY)) {
					DEBUGOUT(" \r [sum]-> %d \n\r----------------\n\r", sum);
					fflush(stdout);
					xSemaphoreGive(mutex);
					sum = 0;
					receivesum = 0;
				}
			}
		}
		vTaskDelay(1);
	}
}
#endif
/*--------------------------------------------------------------------------*/
/*-----------------------------------ex2------------------------------------*/
#if 0
static void vSendRdToBackTask(void *pvParameters){
	int nbRand;

	vTaskDelay(1);
	while(1){
		nbRand = (rand() %1000) + 1;
		vTaskDelay(((rand() %5)+1)*100);
		xQueueSendToBack(queue, &nbRand, portMAX_DELAY);
	}
}

static void vSW1Task(void *pvParameters){
	DigitalIoPin sw1(0, 17, DigitalIoPin::input, false);
	int btPressed = 112;
	bool is_pressed = false;


	while (1) {
		if (!sw1.read()) {
			if(is_pressed == false)
			{
				xQueueSendToFront(queue, &btPressed, portMAX_DELAY);
				is_pressed =true;
			}

			//vTaskDelay(150);
		} else is_pressed = false;

		vTaskDelay(1);
	}
}

static void vWaitPrintQueueTask(void *pvParameters){
	int receivesum=0;
	vTaskDelay(10);
	while(1){

		if(xQueueReceive(queue, &receivesum, portMAX_DELAY)) {


			DEBUGOUT(" %d \n\r", receivesum);
			fflush(stdout);

			if (receivesum == 112) {
				vTaskDelay(1);
				DEBUGOUT(" \r help me \n\r");
				fflush(stdout);

				vTaskDelay(300);
			}
		}
	}
}
#endif

/*--------------------------------------------------------------------------*/
/*-----------------------------------ex3------------------------------------*/
#if 1
void debug(char *format, uint32_t d1, uint32_t d2, uint32_t d3) {
	debugEvent event;
	event.format=format;
	event.data[0] = d1;
	event.data[1] = d2;
	event.data[2] = d3;

	xQueueSendToBack(queue, &event, portMAX_DELAY);
}

static void vReadPrintITMTask(void *pvParameters){

	int read,i=0;
	char string[60];

	while(1){

		if(xSemaphoreTake(mutex, portMAX_DELAY)){
			read = Board_UARTGetChar();
			xSemaphoreGive(mutex);
		}
		fflush(stdout);

		if(read!=EOF){
			string[i]=read;

			if(string[i]!='\n' && string[i]!='\r'&& string[i]!=' ' && i<59){
				if(xSemaphoreTake(mutex, portMAX_DELAY)){
					DEBUGOUT("%c", string[i]);
					xSemaphoreGive(mutex);
				}
				i++;
			} else {

				string[i]='\0';

				debug((char*)"numb of char: %d  -  tick: %d \n", i, xTaskGetTickCount(), 0);

				if(xSemaphoreTake(mutex, portMAX_DELAY)){

					DEBUGOUT("\r [you]: %s \r \n", string);
					xSemaphoreGive(mutex);
				}
				i=0;
			}
		} else {
			vTaskDelay(1);
		}


	}
}

static void vSW1Task(void *pvParameters){
	DigitalIoPin sw1(0, 17, DigitalIoPin::input, false);
	uint32_t timer=0;

	while (1) {

		while(!sw1.read()) {

			timer++;
			vTaskDelay(1);
		}
		if(timer != 0) {

			debug((char*)"sw pressed: %d ms \n", timer,0,0);
			timer = 0;
		}
		vTaskDelay(1);
	}
}

static void vDebugPrintTask(void *pvParameters){
	char buffer[64];
	debugEvent e;

	while(1){

		if(xQueueReceive(queue, &e, portMAX_DELAY)) {

			snprintf(buffer, 64, e.format, e.data[0], e.data[1], e.data[2]);//e.data[0]
			ITM_write(buffer);
		}
	}
}
#endif
/*****************************************************************************
 ***************************** base functions ********************************
 ****************************************************************************/

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

extern "C" {
void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}
}
/*****************************************************************************
 ***************************** MAIN  *****************************************
 ****************************************************************************/
int main(void)
{

	prvSetupHardware();
	ITM_init();

	mutex = xSemaphoreCreateMutex();
	binary = xSemaphoreCreateBinary();
	counter = xSemaphoreCreateCounting(10,0);


	//ex1
#if 0
	queue = xQueueCreate(5, sizeof( int32_t ));

	xTaskCreate(vReadAndCountsTask, "vReadAndCountsTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	xTaskCreate(vSW1Task,"vSW1Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	xTaskCreate(vWaitQueueTask, "vWaitQueueTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif
	//ex2
#if 0
	queue = xQueueCreate(20, sizeof( int32_t ));

	xTaskCreate(vSendRdToBackTask, "vSendRdToBackTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	xTaskCreate(vSW1Task,"vSW1Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);

	xTaskCreate(vWaitPrintQueueTask, "vWaitPrintQueueTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif
	//ex3
#if 1
	queue = xQueueCreate(10, sizeof( debugEvent ));

	xTaskCreate(vReadPrintITMTask, "vReadPrintITMTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 2UL),
			(TaskHandle_t *) NULL);
	xTaskCreate(vSW1Task,"vSW1Task", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 2UL),
			(TaskHandle_t *) NULL);

	xTaskCreate(vDebugPrintTask, "vDebugPrintTask", configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif

	/* Start the scheduler */
	vTaskStartScheduler();
	return 1;
}
