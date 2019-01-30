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

//LED
#define MAXLEDS 3
static const uint8_t ledpins[MAXLEDS] = {25, 3, 1};
static const uint8_t ledports[MAXLEDS] = {0, 0, 1};
#define RED 0
#define GREEN 1
#define BLUE 2

const int DEFAULT_STATE = 0;
const int LEARNING_STATE = 1;


volatile uint32_t MIN_FILTER_MS = 200;



const int BUTTON_LOCK_DELAY = 3000;
const int OPEN_DELAY = 5000;

const int LOCK_QUEUE_LENGTH = 10;
const int LOCK_SEQUENCE_LENGTH = 8;
int LOCK_SEQUENCE[LOCK_SEQUENCE_LENGTH] = {1, 1, 1, 1, 0, 1, 1, 0};

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <cr_section_macros.h>
#include "FreeRTOS.h"
#include "task.h"
#include "chip.h"
#include "queue.h"

#include "DigitalIoPin.h"

QueueHandle_t queue;
std::vector<int> vector;

TickType_t lastKeyPressTick;

bool locked = true;

int state = DEFAULT_STATE;
TickType_t buttonLastPressed[3] = {0, 0, 0};


void taskDoor(void *params);

/* Sets up system hardware */
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	Board_Debug_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(RED, false);
	Board_LED_Set(GREEN, false);
	Board_LED_Set(BLUE, false);
}

void setupGPIOInterupt(void) {
	// assign interrupts to pins
	Chip_INMUX_PinIntSel(0, 0, 17);
	Chip_INMUX_PinIntSel(1, 1, 11);

	uint32_t pm = IOCON_DIGMODE_EN;
	pm |= IOCON_MODE_PULLUP;

	Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 17, pm);
	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 11, pm);

	// enable clock for GPIO interrupt
	Chip_PININT_Init(LPC_GPIO_PIN_INT);
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PININT);

	// Set Interrupt priorities
	NVIC_SetPriority(PIN_INT0_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);
	NVIC_SetPriority(PIN_INT1_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);

	// interrupt at falling edge, clear initial status
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	// Enable IRQ
	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
	NVIC_EnableIRQ(PIN_INT2_IRQn);
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

bool conbinaison(std::vector<int> vector, const int *arr, size_t size) {
	if(vector.size() < size) {
		return false;
	}

	size_t i = vector.size();
	size_t j = size;

	for(i = 0, j = 0; i < vector.size() && j < size; i++) {
		if(vector[i] == arr[j]) {
			j ++;
		} else {
			i -= j; j = 0;
		}
	}

	return j == size;
}



void addToQueue(int number, int button, portBASE_TYPE *woken) {

	lastKeyPressTick = xTaskGetTickCountFromISR();

	if(buttonLastPressed[button] == 0) {
		buttonLastPressed[button] = lastKeyPressTick;
		xQueueSendFromISR(queue, (void *) &number, woken);
	}

	if(lastKeyPressTick - buttonLastPressed[button] > MIN_FILTER_MS) {
		buttonLastPressed[button] = lastKeyPressTick;
		xQueueSendFromISR(queue, (void *) &number, woken);
	}


}



extern "C" void PIN_INT0_IRQHandler(void) {
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	addToQueue(0, PININTCH0, &xHigherPriorityWoken);
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

extern "C" void PIN_INT1_IRQHandler(void) {
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	addToQueue(1, PININTCH1, &xHigherPriorityWoken);
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

extern "C" void PIN_INT2_IRQHandler(void) {
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

	addToQueue(3, PININTCH2, &xHigherPriorityWoken);
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}




void taskMonitorSW3(void *params) {
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);
	vTaskDelay(10);

	TickType_t lastTick = 0;

	while(true) {
		if(sw3.read()) {
			if(lastTick == 0) {
				lastTick = xTaskGetTickCount();
			}

			if(xTaskGetTickCount() - lastTick > BUTTON_LOCK_DELAY) {
				int number = 3;
				xQueueSendToFront(queue, &number, 0);
				lastTick = 0;
			}
		} else if(lastTick != 0) {
			lastTick = 0;
			//a voir : vTaskDelay(10);
		}
		vTaskDelay(10);
	}
}

void taskDoorLED(void *params) {
	long lastTick = -1;
	while(true) {
		if(state == DEFAULT_STATE) {
			Board_LED_Set(RED, locked);
			Board_LED_Set(GREEN, !locked);
			Board_LED_Set(BLUE, false);

			if(!locked) {
				if(lastTick == -1) {
					lastTick = xTaskGetTickCount();
				} else {
					long timer = xTaskGetTickCount() - lastTick;

					if(timer > OPEN_DELAY) {
						locked = true;
						lastTick = -1;
					}
				}
			}
		} else {
			Board_LED_Set(RED, false);
			Board_LED_Set(GREEN, false);
			Board_LED_Set(BLUE, true);
		}
		vTaskDelay(100);
	}
}

void taskLearning(void *params) {
	vTaskDelay(10);
	int number;
	int cur = 0;
	printf("learning...\r\n");
	while(true) {
		xQueueReceive(queue, &number, portMAX_DELAY);
		LOCK_SEQUENCE[cur] = number; // de 0 a 7 = 8
		cur++;
		if(cur >= LOCK_SEQUENCE_LENGTH) {
			state = DEFAULT_STATE;
			printf("end of learning\n\r");
			xTaskCreate(taskDoor, "vTaskDoor", configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
			vTaskDelete(NULL);
		}

		//		if(cur < LOCK_SEQUENCE_LENGTH) {
		//			LOCK_SEQUENCE[cur] = number;
		//			cur++;
		//		} else {
		//			state = DEFAULT_STATE;
		//			xTaskCreate(taskDoor, "vTaskDoor", configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
		//			vTaskDelete(NULL);
		//		}
	}
}

void taskDoor(void *params) {
	int number;

	while (true) {
		if(xQueueReceive(queue, &number, 10000)){ // 15s ne reset pas

			long diff = xTaskGetTickCount() - lastKeyPressTick;

			if(diff > 10000) {
				xQueueReset(queue);
			}

			if(number == 3) {
				// Remove the item 3 from the queue
				state = LEARNING_STATE;
				vector.clear();
				xTaskCreate(taskLearning, "vTaskLearning", configMINIMAL_STACK_SIZE + 128,
						NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
				vTaskDelete(NULL);
			} else {
				vector.push_back(number);

				DEBUGOUT("actual code ->");
				for(int i = 0; i < LOCK_SEQUENCE_LENGTH; i ++) {
					DEBUGOUT("%d", LOCK_SEQUENCE[i]);
				}
				DEBUGOUT("<-\r\n");

				DEBUGOUT("Vector ->");
				for(size_t i = 0; i < vector.size(); i ++) {
					DEBUGOUT("%d", vector[i]);
				}
				DEBUGOUT("->\r\n");

				if(conbinaison(vector, LOCK_SEQUENCE, LOCK_SEQUENCE_LENGTH)) {
					locked = false;
					printf("open!!\n\r");
					vector.clear();
				}
			}
		} else
		{
			vector.clear();
			xQueueReset(queue);
		}
		vTaskDelay(10);
	}
}


int main(void) {
	prvSetupHardware();
	setupGPIOInterupt();

	queue = xQueueCreate(LOCK_QUEUE_LENGTH, sizeof(int));

	xTaskCreate(taskMonitorSW3, "vTaskMonitorSW3", configMINIMAL_STACK_SIZE,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskDoor, "vTaskDoor", configMINIMAL_STACK_SIZE + 256,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskDoorLED, "vTaskDoorLED", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);



	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}


























