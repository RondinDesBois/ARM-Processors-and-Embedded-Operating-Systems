/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
====================================
changer dans freeRTOSConfig.h:
#define configUSE_TIMERS                1 //0

//ad
#define configTIMER_TASK_PRIORITY		configMAX_PRIORITIES-1
#define configTIMER_QUEUE_LENGTH		3
#define configTIMER_TASK_STACK_DEPTH	configMINIMAL_STACK_SIZE +128
//****


 *
 *
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
#include <stdlib.h>
#include <cr_section_macros.h>
#include "FreeRTOS.h"
#include "event_groups.h"
#include "DigitalIoPin.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "strTool.h"



enum Type {
	HELP=0, INTERVAL=1, TIME=2, ERR=3
};

struct Command {
	Type type;
	int numb;
};

// TODO: insert other definitions and declarations here
//#define EX1
//#define EX2
#define EX3

//LED
#define RED 0
#define GREEN 1
#define BLUE 2

QueueHandle_t queue;
QueueHandle_t queueCmd;
SemaphoreHandle_t binary;
SemaphoreHandle_t mutex;

TimerHandle_t autoReloadTimer;
TimerHandle_t oneShotTimer;
TimerHandle_t greenTimer;
TimerHandle_t inactivityTimer;
TimerHandle_t togglingGreenTimer;

TickType_t timerGreenToggle = 0;

#ifdef EX3
Command parseCommand(std::string line);
std::string getSec(int ticks);
#endif

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
	Board_LED_Set(RED, false);
	Board_LED_Set(GREEN, false);
	Board_LED_Set(BLUE, false);

}


//*********************************************
//***************timer*************************
//*********************************************

#ifdef EX1
void xAutoReloadTimer(TimerHandle_t timer) {
	char chr[] = "hello";
	xQueueSend(queue, &chr, 0);
	Board_LED_Toggle(GREEN);
}

void xOneShotTimer(TimerHandle_t timer) {
	xSemaphoreGive(binary);
}
#endif

#ifdef EX2
void xGreenLedTimer(TimerHandle_t timer) {
	Board_LED_Set(GREEN, false); //quand le timer expire vert false
}
#endif

#ifdef EX3
void xInactivityTimer(TimerHandle_t timer) {
	int i;
	xSemaphoreGive(binary);

}

void xTogglingGreenTimer(TimerHandle_t timer) {
	bool ledState = !(Board_LED_Test(GREEN));
	Board_LED_Set(GREEN, ledState);
	timerGreenToggle = xTaskGetTickCount();
}


#endif


//*********************************************
//***************tasks*************************
//*********************************************

#ifdef EX1
void vPrintTask(void *params) {
	char chr[10];
	while(true) {
		xQueueReceive(queue, &chr, portMAX_DELAY);
		DEBUGOUT("%s \r\n", chr);
	}
}

void vCommandTask(void *params) {
	char com[] = "aargh";

	xSemaphoreTake(binary, portMAX_DELAY);
	xQueueSend(queue, &com, 0);

	while(true) {
	}
}
#endif

#ifdef EX2
void vButtonTask(void *params) {
	DigitalIoPin sw1(0, 17, DigitalIoPin::pullup, true);
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, true);
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);
	vTaskDelay(10);

	while(true) {
		if (sw1.read() || sw2.read() || sw3.read()) {
				Board_LED_Set(GREEN, true);
				xTimerReset( greenTimer, 10 );
				}

	}
}
#endif

#ifdef EX3

void taskReadDebug(void *params) {
	std::string line;

	while (true) {
		char uartChar = Board_UARTGetChar();
		if (uartChar != 255) {
			if (uartChar == 13) {
				xSemaphoreTake(mutex, portMAX_DELAY);
				Board_UARTPutChar('\n');
				Board_UARTPutChar('\r');
				xSemaphoreGive(mutex);

				Command cmd = parseCommand(line);
				xQueueSend(queueCmd, (void *) &cmd, 0);

				line = "";
			} else {
				line += uartChar;


			}

			if(!xTimerIsTimerActive(inactivityTimer)) {
				xTimerStart(inactivityTimer, 0);
			} else {
				xTimerReset(inactivityTimer, 0);
			}

			xSemaphoreTake(mutex, portMAX_DELAY);
			Board_UARTPutChar(uartChar);
			xSemaphoreGive(mutex);
		} else if(xSemaphoreTake(binary, 1)){
			line = "";
			xSemaphoreTake(mutex, portMAX_DELAY);
			for ( int i = 0;  i < 50; ++ i) {
				Board_UARTPutChar('\n');
				Board_UARTPutChar('\r');
			}
		DEBUGOUT("[INACTIVE] \r\n");
		xSemaphoreGive(mutex);
		}

	}
}


void vExecuterTask(void *params) {
	Command cmd;
	const char *helpMsg = "\r\nhelp - display usage instructions\r\ninterval <number> - set the led toggle interval (default is 5 seconds)\r\ntime – prints the number of seconds with 0.1s accuracy since the last led toggle\r\n";

	while(true) {

		xQueueReceive(queueCmd, &cmd, portMAX_DELAY);
		xSemaphoreTake(mutex, portMAX_DELAY);
		TickType_t timer = xTaskGetTickCount() - timerGreenToggle;
		switch (cmd.type) {
			case HELP:
				DEBUGOUT(helpMsg);
				break;
			case INTERVAL:
				if(xTimerChangePeriod(togglingGreenTimer, cmd.numb * 1000, 0) == pdPASS) {
					DEBUGOUT("> interval set to: %d\r\n", cmd.numb);
				} else {
					DEBUGOUT(">error\r\n");
				}

				break;
			case TIME:
				DEBUGOUT("> %s\r\n", getSec(timer).c_str());
				//DEBUGOUT("%d \r\n",timer);
				break;
			case ERR:
				DEBUGOUT("> error\r\n");
				break;
		}
		xSemaphoreGive(mutex);
	}
}


//*********************************************
//***************functions*********************
//*********************************************

std::string getSec(int ticks) {
	char sec[10];
	int seconds = ticks / 1000;
	int decimal = (ticks % 1000) / 100;
	sprintf(sec, "%d.%d s", seconds, decimal);
	return sec;
}

Command parseCommand(std::string line) {
	std::queue<std::string> parts = StringUtil::split(line, " ");
	Command cmd;

	std::string type = parts.front();

	if(type == "help") {
		cmd.type = HELP;
	} else if(type == "interval") {
		cmd.type = INTERVAL;
	} else if(type == "time") {
		cmd.type = TIME;
	} else {
		cmd.type = ERR;
		return cmd;
	}

	parts.pop();

	if(cmd.type == INTERVAL) {
		std::string num = parts.front();
		cmd.numb = atoi(num.c_str());
	}

	return cmd;
}


#endif



//*********************************************
//**************main***************************
//*********************************************


int main(void) {
	prvSetupHardware();

	#ifdef EX1

	queue = xQueueCreate(10, sizeof(char[10]));
	mutex = xSemaphoreCreateMutex();
	binary= xSemaphoreCreateBinary();

	autoReloadTimer = xTimerCreate("auto-reload", 5000, pdTRUE, (void *) 0, xAutoReloadTimer);

	xTimerStart(autoReloadTimer, 0);

	oneShotTimer = xTimerCreate("one-shot", 20000, pdFALSE, (void *) 0, xOneShotTimer);

	xTimerStart(oneShotTimer, 0);


		xTaskCreate(vPrintTask, "vPrintTask", configMINIMAL_STACK_SIZE + 256,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

		xTaskCreate(vCommandTask, "vCommandTask", configMINIMAL_STACK_SIZE + 128,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	#endif

	#if defined(EX2)
		greenTimer = xTimerCreate("green-timer", 5000, pdFALSE, (void *) 0, xGreenLedTimer);

		xTaskCreate(vButtonTask, "vButtonTask", configMINIMAL_STACK_SIZE + 128,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
	#endif


	#ifdef EX3
		queueCmd = xQueueCreate(10, sizeof(Command) );
		mutex = xSemaphoreCreateMutex();
		binary = xSemaphoreCreateBinary();


		inactivityTimer = xTimerCreate("inactivity-timer", 10000, pdFALSE, (void *) 0, xInactivityTimer);
		xTimerStart(inactivityTimer, 0);

		togglingGreenTimer = xTimerCreate("toggling-timer", 5000, pdTRUE, (void *) 0, xTogglingGreenTimer);
		xTimerStart(togglingGreenTimer, 0);

		xTaskCreate(vExecuterTask, "vExecuterTask", configMINIMAL_STACK_SIZE + 128,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
		xTaskCreate(taskReadDebug, "taskReadDebug", configMINIMAL_STACK_SIZE + 128,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	#endif
		/* Start the scheduler */
		vTaskStartScheduler();

		/* Should never arrive here */
    return 0 ;
}
