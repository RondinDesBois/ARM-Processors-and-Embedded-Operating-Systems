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





/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

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
// la led passe de on a of toutes les task delay /2
/* LED1 toggle thread */
static void vLEDTask1(void *pvParameters) { //red
	bool LedState = false;
	int n;
	while (1) {
		//Board_LED_Set(0, LedState);
		//LedState = (bool) !LedState;
		for (n = 0; n < 6; ++n) {
			Board_LED_Set(0, LedState);
			LedState = (bool) !LedState;
			vTaskDelay(configTICK_RATE_HZ /2);
		}
		for (n = 0; n < 6; ++n) {
					Board_LED_Set(0, LedState);
					LedState = (bool) !LedState;
					vTaskDelay(configTICK_RATE_HZ);
				}
		for (n = 0; n < 6; ++n) {
					Board_LED_Set(0, LedState);
					LedState = (bool) !LedState;
					vTaskDelay(configTICK_RATE_HZ /2);
				}

		/* About a 3Hz on/off toggle rate */
		//vTaskDelay(configTICK_RATE_HZ /2 ); // / 6
	}
}

/* LED2 toggle thread */
static void vLEDTask2(void *pvParameters) {  //green
	bool LedState = false;


	while (1) {
		Board_LED_Set(1, LedState);
		LedState = (bool) !LedState;
		/* not About a 7Hz on/off toggle rate */
		vTaskDelay(configTICK_RATE_HZ*12 );// / 14
	}
}

/* UART (or output) thread */
static void vUARTTask(void *pvParameters) {
	int tickCnt = 0, m=0, s=0;




	while (1) {

		DigitalIoPin sw1(0, 17, DigitalIoPin::input, false); //DigitalIoPin::input
		bool res=sw1.read();

		DEBUGOUT("Tick: %d \r\n", tickCnt);
		if (s==60) {
			s=0;
			m++;
		}else s++;

		//if (s<10) {
		//	DEBUGOUT("Time: 0%d:0%d \n",m,s);
		//}else DEBUGOUT("Time: 0%d:%d \n",m,s);
		DEBUGOUT("Time: %02d:%02d \n",m,s);
		if (!res) {
			vTaskDelay(configTICK_RATE_HZ/10);
			//tickCnt = tickCnt+10;
		}else vTaskDelay(configTICK_RATE_HZ);

			tickCnt++;

		/* About a 1s delay here */


	}

}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

}
/* end runtime statictics collection */

/**
 * @brief	main routine for FreeRTOS blinky example
 * @return	Nothing, function should not exit
 */
int main(void)
{
	prvSetupHardware();



	/* LED1 toggle thread */
	xTaskCreate(vLEDTask1, "vTaskLed1",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED2 toggle thread */
	xTaskCreate(vLEDTask2, "vTaskLed2",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* UART output thread, simply counts seconds */
	xTaskCreate(vUARTTask, "vTaskUart",
				configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

