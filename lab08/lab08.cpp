/*
===============================================================================
 Name        : main.c
 Author      : $(sam)
 Version     :
 Copyright   : $(sam)
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

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <cr_section_macros.h>
#include "FreeRTOS.h"
#include "task.h"
#include "chip.h"
#include "queue.h"
#include "strTool.h"
#include "DigitalIoPin.h"
#include "semphr.h"


// TODO: insert other definitions and declarations here
// EX 3 A FAIRE
//dernier truk ajouté : mutex
//#define ex1
//#define ex2
#define ex3

#ifdef ex2
struct ItrNotification {
	int id;
	long tick;
};
#endif
#ifdef ex2
volatile int MINIMUM_FILTER_TIME_MS = 50;
#endif

#ifdef ex3
volatile int MINIMUM_FILTER_TIME_MS = 300;
#endif


void putInQueue(int number, portBASE_TYPE *woken);
int parseTime(std::string line);

QueueHandle_t itrQueue;
SemaphoreHandle_t  mutex;

#ifdef ex3
const int BUTTON_LOCK_DELAY = 3000;
const int OPEN_DELAY = 5000;

const int LOCK_QUEUE_LENGTH = 10;
const int LOCK_SEQUENCE_LENGTH = 8;
int LOCK_SEQUENCE[LOCK_SEQUENCE_LENGTH] = {1, 1, 1, 1, 0, 1, 1, 0};

void taskDoor(void *params);
#endif
//*****************
//*****************


void setupGPIOInterupt(void) {
	// assign interrupts to pins
	//GPIO PINTSEL interrupt, should be: 0 to 7
	Chip_INMUX_PinIntSel(0, 0, 17); //pinstel:0  sw1  port:0 pin:17
	Chip_INMUX_PinIntSel(1, 1, 11); //pinstel:1 sw2 port:1 pin:11
#if defined(ex1) || defined(ex2)
	Chip_INMUX_PinIntSel(2, 1, 9); //pinstel:2 sw3 port:1 pin:9
#endif

	uint32_t pm = IOCON_DIGMODE_EN;
	pm |= IOCON_MODE_PULLUP; // pm = pm OR IOCON_MODE_PULLUP

	Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 17, pm);
	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 11, pm);
#if defined(ex1) || defined(ex2)
	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 9, pm);
#endif
	//  enable clock for GPIO interrupt
	Chip_PININT_Init(LPC_GPIO_PIN_INT);
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PININT);

	// Set Interrupt priorities
	NVIC_SetPriority(PIN_INT0_IRQn, 5); //#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191 /* equivalent to 0xb0, or priority 11. */
	NVIC_SetPriority(PIN_INT1_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);
#if defined(ex1) || defined(ex2)
	NVIC_SetPriority(PIN_INT2_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);
#endif

	// interrupt at falling edge, clear initial status
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

#if defined(ex1) || defined(ex2)
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH2);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH2);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);
#endif

	// Enable IRQ
	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
	NVIC_EnableIRQ(PIN_INT2_IRQn);
}

extern "C" {
void vConfigureTimerForRunTimeStats(void) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}
}
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	Board_Debug_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
	Board_LED_Set(1, false);
	Board_LED_Set(2, false);

}

//*****************
//****interupt*****

#ifdef __cplusplus
extern "C" {
#endif

void PIN_INT0_IRQHandler(void) {
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

#if defined(ex1) || defined(ex2)
	putInQueue(1, &xHigherPriorityWoken);
#endif

	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}


void PIN_INT1_IRQHandler(void) { //when sw2 is pressed, catch ISR
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

#if defined(ex1) || defined(ex2)
	putInQueue(2, &xHigherPriorityWoken);
#endif

	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

void PIN_INT2_IRQHandler(void) {
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

#if defined(ex1) || defined(ex2)
	putInQueue(3, &xHigherPriorityWoken);
#endif

	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

#ifdef __cplusplus
}
#endif

//*****************
//***** task ******
//*****************


#ifdef ex1
void taskReadQueue(void *params) {
	int val;
	int last = -1;
	int count = 0;

	while (true) {
		xQueueReceive(itrQueue, &val, portMAX_DELAY);

		if (val == last) {
			count++;
		} else {
			DEBUGOUT("Button %d pressed %d times.\r\n", last, count);
			count = 1;
		}

		last = val;
	}

}
#endif


#ifdef ex2
void taskReadTick(void *params) {
	ItrNotification msg;
	long lastTick = xTaskGetTickCount();

	while (true) {
		xQueueReceive(itrQueue, &msg, portMAX_DELAY);

		long diff = msg.tick - lastTick;
		char val[10];
		if(diff>MINIMUM_FILTER_TIME_MS){
			if (diff > 1000) {
				int seconds = diff / 1000;
				int decimal = (diff % 1000) / 100;
				sprintf(val, "%d.%d s", seconds, decimal);
			} else {
				sprintf(val, "%ld ms", diff);
			}
			if(xSemaphoreTake(mutex, portMAX_DELAY)){
				DEBUGOUT("%s Button %d \r\n", val, msg.id);
				xSemaphoreGive(mutex);
			}
		} else {
			if(xSemaphoreTake(mutex, portMAX_DELAY)){
				DEBUGOUT(" warning filter = %d \r\n", MINIMUM_FILTER_TIME_MS);
				xSemaphoreGive(mutex);
			}
		}
		lastTick = msg.tick;
	}
}

void taskReadDebug(void *params) {
	std::string line;

	while (true) {
		char uartChar = Board_UARTGetChar();
		if (uartChar != 255) {
			if (uartChar == 13) {

				if(xSemaphoreTake(mutex, portMAX_DELAY)){
					Board_UARTPutChar('\n');
					xSemaphoreGive(mutex);
				}

				int time = parseTime(line);
				MINIMUM_FILTER_TIME_MS = time;

				line = "";
			} else {
				line += uartChar;
			}
			if(xSemaphoreTake(mutex, portMAX_DELAY)){
				Board_UARTPutChar(uartChar);
				xSemaphoreGive(mutex);
			}

		}
	}
}
#endif

#ifdef ex3

#endif




//*****************
//*** functions ***
//*****************
int parseTime(std::string line) {
	std::queue<std::string> parts = StringUtil::split(line, " ");

	std::string type = parts.front();
	if (type != "filter") {
		return MINIMUM_FILTER_TIME_MS;
	}
	parts.pop();

	std::string num = parts.front();

	return atoi(num.c_str());
	parts.pop();

	return MINIMUM_FILTER_TIME_MS;
}
#if defined(ex1) || defined(ex2)
void putInQueue(int number, portBASE_TYPE *woken) {

#ifdef ex2
	ItrNotification msg;
	msg.id = number;
	msg.tick = xTaskGetTickCountFromISR();

	xQueueSendFromISR(itrQueue, (void *) &msg, woken);
#endif

#ifdef ex1
	xQueueSendFromISR(itrQueue, (void *)&number, woken);
#endif
}
#endif

#ifdef ex3


#endif

//*****************
//***** main ******
//*****************
int main(void) {

	prvSetupHardware();
	mutex = xSemaphoreCreateMutex();
#ifdef ex1
	itrQueue = xQueueCreate(20, sizeof(int));
#endif
#ifdef ex2
	itrQueue = xQueueCreate(20, sizeof(ItrNotification));
#endif
#ifdef ex3

#endif

	setupGPIOInterupt();


#ifdef ex1

	xTaskCreate(taskReadQueue, "vTaskReadQueue", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif
#ifdef ex2

	xTaskCreate(taskReadTick, "taskReadTick", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskReadDebug, "vTaskReadDebug", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif

	vTaskStartScheduler();

	return 0 ;
}
