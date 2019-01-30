/*
 ===============================================================================
 Name        : main.c
 Author      : $(sam)
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

#include <stdlib.h>
#include <cr_section_macros.h>
#include "FreeRTOS.h"
#include "event_groups.h"
#include "DigitalIoPin.h"
#include "task.h"
#include "semphr.h"


//#define EX1
//#define EX2
#define EX3

#define BIT_1		( 1 << 1 ) //sur 1 bit 1
#define BIT_2		( 1 << 2 ) // sur 2bit 10
#define BIT_3		( 1 << 3 ) //sur 3 bit 100
#define BITS_SYN ( BIT_1 | BIT_2 | BIT_3 )

TickType_t TIMEOUT = 5000;
#ifdef EX3
int tick1=0, tick2=0, tick3=0;
#endif
EventGroupHandle_t eventGroup;
SemaphoreHandle_t mutex;//, sw1Binary, sw2Binary, sw3Binary;



/* Sets up system hardware */
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	Board_Debug_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
	Board_LED_Set(1, false);
	Board_LED_Set(2, false);
}

/* the following is required if runtime statistics are to be collected */
extern "C" {
void vConfigureTimerForRunTimeStats(void) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}
}
/* end runtime statictics collection */


#ifdef EX1
void taskFirst(void *params) {
	DigitalIoPin sw1(0, 17, DigitalIoPin::pullup, true);
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, true);
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);

	vTaskDelay(10);
	while (true) {
		if (sw1.read() || sw2.read() || sw3.read()) {
			xEventGroupSetBits(eventGroup, 1);
		}

		vTaskDelay(10);
	}
}

void taskSecond(void *params) {
	long lastTick = xTaskGetTickCount();

	xEventGroupWaitBits(eventGroup, 1, pdTRUE, pdFALSE, portMAX_DELAY);

	//xEventGroupClearBits(eventGroup, 1);
	while (true) {


		xSemaphoreTake(mutex, portMAX_DELAY);
		long timer = xTaskGetTickCount() - lastTick;
		lastTick = xTaskGetTickCount();
		int num = 1000 + (rand() % 1000);
		DEBUGOUT("task %s Elapsed time: %d, delay: %d\r\n", pcTaskGetName(NULL), timer, num); // diff entre 1 et 2 s
		xSemaphoreGive(mutex);

		vTaskDelay(num);
	}
}
#endif



//here

#if 1
void taskSW1(void *params) {
	DigitalIoPin sw1(0, 17, DigitalIoPin::pullup, true);
	bool is_pressed = false;
	int n = 0;
	while (true) {
		if (sw1.read()) {
			if (is_pressed == false) {

#ifdef EX2
				n++;
				if (n >= 1) {

							xSemaphoreTake(mutex, portMAX_DELAY);
							DEBUGOUT("SW1 ready \r\n");
							xSemaphoreGive(mutex);

							long start = xTaskGetTickCount();
							xEventGroupSync(eventGroup, BIT_1, BITS_SYN, portMAX_DELAY);

							xEventGroupClearBits(eventGroup, BITS_SYN);

							xSemaphoreTake(mutex, portMAX_DELAY);
							long timer = xTaskGetTickCount() - start;
							DEBUGOUT("SW1, avaiable since: %d ms\r\n", timer);
							xSemaphoreGive(mutex);
							//vTaskSuspend(NULL);
							n = 0;

						}
#endif
#ifdef EX3
				tick2 = xTaskGetTickCount();
				xEventGroupSetBits(eventGroup, BIT_1);
#endif

				is_pressed = true;
			}
		} else {
			is_pressed = false;
		}

		vTaskDelay(20);
	}
}

void taskSW2(void *params) {
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, true);
	vTaskDelay(10);
	bool is_pressed = false;
	int n = 0;
	while (true) {
		if (sw2.read()) {
			if (!is_pressed) {
#ifdef EX2
				n++;
				if (n >= 2) {

							xSemaphoreTake(mutex, portMAX_DELAY);
							DEBUGOUT("SW2 ready \r\n");
							xSemaphoreGive(mutex);

							long start = xTaskGetTickCount();
							xEventGroupSync(eventGroup, BIT_2, BITS_SYN, portMAX_DELAY);

							xEventGroupClearBits(eventGroup, BITS_SYN);//

							xSemaphoreTake(mutex, portMAX_DELAY);
							long timer = xTaskGetTickCount() - start;
							DEBUGOUT("SW2, avaiable since: %d ms\r\n", timer);
							xSemaphoreGive(mutex);
							//vTaskSuspend(NULL);
							n = 0;

						}
#endif
#ifdef EX3
				tick2 = xTaskGetTickCount();
				xEventGroupSetBits(eventGroup, BIT_2);
#endif
				is_pressed = true;
			}
		} else {
			is_pressed = false;
		}

		vTaskDelay(20);
	}
}

void taskSW3(void *params) {
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);

	bool is_pressed = false;
	int n = 0;
	while (true) {
		if (sw3.read()) {
			if (!is_pressed) {
#ifdef EX2
				n++;
				if (n >= 3) {

							xSemaphoreTake(mutex, portMAX_DELAY);
							DEBUGOUT("SW3 ready \r\n");
							xSemaphoreGive(mutex);

							long start = xTaskGetTickCount();
							xEventGroupSync(eventGroup, BIT_3, BITS_SYN, portMAX_DELAY);

							xEventGroupClearBits(eventGroup, BITS_SYN);//

							xSemaphoreTake(mutex, portMAX_DELAY);
							long timer = xTaskGetTickCount() - start;
							DEBUGOUT("SW3, avaiable since: %d ms\r\n", timer);
							xSemaphoreGive(mutex);
							//vTaskSuspend(NULL);
							n = 0;

						}
#endif
#ifdef EX3
				tick2 = xTaskGetTickCount();
				xEventGroupSetBits(eventGroup, BIT_3);
#endif
				is_pressed = true;
			}
		} else {
			is_pressed = false;
		}

		vTaskDelay(20);
	}
}
#ifdef EX3
void taskWatchdog(void *params) {
	long curTick;

	while (true) {

		EventBits_t syn = xEventGroupWaitBits(eventGroup, BITS_SYN, pdTRUE, pdTRUE, TIMEOUT); //wait snice the value of the grp event are == SYN or FAIL_TIMEOUT are passed unbouded
		xEventGroupClearBits(eventGroup, BITS_SYN);


		curTick = xTaskGetTickCount();

		if(syn == BITS_SYN) {
			Board_UARTPutSTR("OK\r\n");
		} else {
			Board_UARTPutSTR("Fail\r\n");

			if((syn & BIT_1) == 0) {
				DEBUGOUT("SW1 not pressed for %d ticks\r\n", (curTick - tick1));
			}

			if((syn & BIT_2) == 0) {
				DEBUGOUT("SW2 not pressed for %d ticks\r\n", (curTick - tick2));
			}

			if((syn & BIT_3) == 0) {
				DEBUGOUT("SW3 not pressed for %d ticks\r\n", (curTick - tick3));
			}
		}
	}
}
#endif
#endif


int main(void) {
	prvSetupHardware();

//#ifdef EX2
//		sw1Binary = xSemaphoreCreateBinary();
//		sw2Binary = xSemaphoreCreateBinary();
//		sw3Binary = xSemaphoreCreateBinary();
//#endif

	eventGroup = xEventGroupCreate();
	mutex = xSemaphoreCreateMutex();

#ifdef EX1
	xTaskCreate(taskFirst, "vTaskFirst", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSecond, "1_vTask2", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSecond, "2_vTask2", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSecond, "3_vTask2", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif

#if defined(EX2) || defined(EX3)
//	xTaskCreate(vReadButtonTask, "vReadButtonTask", configMINIMAL_STACK_SIZE, NULL,
//				(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSW1, "vTaskSW1", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSW2, "vTaskSW2", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskSW3, "vTaskSW3", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif


#ifdef EX3
	xTaskCreate(taskWatchdog, "vTaskWatchdog", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
