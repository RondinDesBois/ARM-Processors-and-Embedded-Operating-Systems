#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>
#include <string>
#include <queue>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "chip.h"
#include "queue.h"
#include "strTool.h"

#include "DigitalIoPin.h"
#include "ITM_write.h"

#include "Type.h"
#include "Command.h"


//#define exo1
//#define exo2
#define exo3

Command cutLine(std::string line);

void runStep(int speed);

const char EOF_CHAR = 255;
const char ENTER_CHAR = 13;
const char* COMMAND_SPLIT = " ";

const unsigned int CALIBRATION_LAPSES = 1;
const unsigned int DEFAULT_STEP_LENGTH = 50;
const unsigned int DEFAULT_PPS = 40;//200
const unsigned int CALIBRATION_PPS = 1000;//0
const unsigned int TEST_INCREMENT_PPS = 20;//50

QueueHandle_t messageQueue = NULL;

int currentPPS = 1000;//0
//int haha = 0;
volatile uint32_t RIT_count;
xSemaphoreHandle sbRIT;
xSemaphoreHandle wallBinary;
xSemaphoreHandle startBinary;
xSemaphoreHandle hitWallSemaphore;
xSemaphoreHandle blinkSemaphore;

DigitalIoPin *motor;
DigitalIoPin *direction;
DigitalIoPin *limL;
DigitalIoPin *limR;

bool state = false;
bool canMove = true;
unsigned int averageSteps;
int led = 0;

bool calibrating = true;

bool leftWallHit = false, rightWallHit = false;
int speedRateUp = 5;//30



/* Sets up system hardware */
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	Board_Debug_Init();

	Chip_RIT_Init(LPC_RITIMER);
	NVIC_SetPriority(RITIMER_IRQn,configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);

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

extern "C" {
void RIT_IRQHandler(void) {
	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;

	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag

	if (RIT_count > 0) {
		RIT_count--;


#ifdef exo1
		if(canMove) { // true if nothing read limit
			motor->write(state); // state : run - stop for each runStep
		}
#endif
#ifdef exo2
		if(calibrating || (!calibrating && canMove)) {
			motor->write(state);
		}
#endif
#ifdef exo3
		motor->write(state);
#endif

	} else {
		Chip_RIT_Disable(LPC_RITIMER); // disable timer
		// Give semaphore and set context switch flag if a higher priority task was woken up
		xSemaphoreGiveFromISR(sbRIT, &xHigherPriorityWoken);
	}

	// End the ISR and (possibly) do a context switch
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

void RIT_start(int count, int us) {
	uint64_t cmp_value;
	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000; // pour curentpps = 10 000 : us = 20 et cmp value = 1.4

	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;

	// enable automatic clear on when compare value==timer value // this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);

	// reset the counter
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);

	// start counting
	Chip_RIT_Enable(LPC_RITIMER);

	// Enable the interrupt signal in NVIC (the interrupt controller)
	NVIC_EnableIRQ(RITIMER_IRQn);

	// wait for ISR to tell that we're done
	if (xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		NVIC_DisableIRQ(RITIMER_IRQn);
	} else {
		// unexpected error
	}
}
}

void runStep(int speed) { // speed = PPS
	state = true;
	//DEBUGOUT("!! %d \r\n", xTaskGetTickCount());
	RIT_start(DEFAULT_STEP_LENGTH, 10000 / (speed/2));

	state = false;
	RIT_start(DEFAULT_STEP_LENGTH, 10000 / (speed/2));
}

void taskBlink(void *param) {
	while(true) {
		xSemaphoreTake(blinkSemaphore, portMAX_DELAY);
		Board_LED_Set(0, false);
		Board_LED_Set(1, false);
		Board_LED_Set(3, false);

		Board_LED_Set(led, true);
		vTaskDelay(500);
		Board_LED_Set(led, false);
	}
}

void taskLimitRead(void *params) {
	//DigitalIoPin firstSwitch(0, 27, DigitalIoPin::pullup, true); //L
	//DigitalIoPin secondSwitch(0, 28, DigitalIoPin::pullup, true); //R
	DigitalIoPin *currentSwitch = NULL;

	while (true) {
		bool leftPressed = limL->read(); //firstSwitch.read();
		bool rightPressed = limR->read(); //secondSwitch.read();

		leftWallHit = leftPressed;
		rightWallHit = rightPressed;

#if defined(exo1) || defined(exo2)
		Board_LED_Set(0, leftPressed);
		Board_LED_Set(1, rightPressed);
#endif
		canMove = !(leftPressed || rightPressed);

#ifdef exo3
		if (currentSwitch == NULL) {
			if (leftPressed) {
				currentSwitch = limL;//&firstSwitch;
			} else if (rightPressed) {
				currentSwitch = limR;//&secondSwitch;
			}
		} else {
			if (currentSwitch->read()) {
				xSemaphoreGive(hitWallSemaphore);

				if (currentSwitch == limL) { //&firstSwitch
					currentSwitch = limR; //&secondSwitch;
				} else {
					currentSwitch = limL; //&firstSwitch;
				}
			}
		}

		if(leftPressed) {
			led = 0;
			xSemaphoreGive(blinkSemaphore);
		}

		if(rightPressed) {
			led = 1;
			xSemaphoreGive(blinkSemaphore);
		}
#endif

		vTaskDelay(20);//
	}
}

void taskReadDebug(void *params) {
	std::string line;

	while (true) {
		char uartChar = Board_UARTGetChar();
		if (uartChar != EOF_CHAR) {
			if (uartChar == ENTER_CHAR) { // si enter et press
				Command command = cutLine(line);

				if (command.type == Type::NONE) {
					Board_UARTPutSTR("\r\nerror\r\n");
				} else {
					Board_UARTPutSTR("\r\nok\r\n");
				}

				if (command.type == Type::GO) {
					xSemaphoreGive(startBinary);
				} else if (command.type == Type::STOP) {
					xQueueSendToFront(messageQueue, &command, 0);
				} else if (command.type != Type::NONE) {
					if (uxQueueSpacesAvailable(messageQueue) > 0) {
						xQueueSend(messageQueue, &command, 0);
						#ifdef exo1
						xSemaphoreGive(startBinary);//ajt
						#endif//ajt
					} else {
						Board_UARTPutSTR("\r\ncommand queue full\r\n");
					}
				}
				line = "";
			} else {
				line += uartChar;
			}
			Board_UARTPutChar(uartChar);
		}
	}
}

Command cutLine(std::string line) { // chope la ligne, la split en 2 dans une queu.
	std::queue<std::string> sCut = StringUtil::split(line, COMMAND_SPLIT); // COMMAND_SPLIT = " "
	Command command;
	command.type = Type::NONE;
	command.count = -1;

	if (sCut.size() == 0) {
		return command;
	}
	//si c'est go ou stop il n y a pas de commande.count donc return: on sort de la fonction . sinon continu et test la 2em partie de la queu
	// Get the type of the command
	std::string type = sCut.front(); // recup 1er chaine de la ligne
	if (type == "left") {
		command.type = Type::LEFT;
	} else if (type == "right") {
		command.type = Type::RIGHT;
	} else if (type == "pps") {
		command.type = Type::PPS;
	} else if (type == "go") {
		command.type = Type::GO; //stop et go return, pas les autres!!
		return command;
	} else if (type == "stop") {
		command.type = Type::STOP;
		return command;
	}
	sCut.pop(); // supr la 1er chaine de la ligne

	if (sCut.size() == 0) {
		command.type = Type::NONE;
		return command;
	}

	// Convert the string to an integer
	std::string count = sCut.front(); // recup 2em chaine de la ligne

	// Check if the string is an actual integer
	if (!StringUtil::isInteger(count)) {
		command.type = Type::NONE;
		return command;
	}

	command.count = atoi(count.c_str()); // si c'est un entier
	sCut.pop(); // supr la 1er chaine de la ligne

	return command;
}

void taskInputHandler(void *params) {
	Command command;

	xSemaphoreTake(startBinary, portMAX_DELAY);

	while (true) {
		//if (xSemaphoreTake(startBinary, 0)) { //ajt
			xQueueReceive(messageQueue, &command, portMAX_DELAY);

			if (command.type == Type::LEFT) { //
				direction->write(true);

				for (int i = 0; i < command.count; i++) {
					runStep(currentPPS);
				}
			} else if (command.type == Type::RIGHT) {
				direction->write(false);

				for (int i = 0; i < command.count; i++) {
					runStep(currentPPS);
				}
			} else if (command.type == Type::PPS) {
				currentPPS = command.count;
			} else if (command.type == Type::STOP) {
				xSemaphoreTake(startBinary, portMAX_DELAY);
			}
		//waitstart
		//}//if sema take
	}
}



void letsgo(int speed, unsigned int steps) {
	int incrementStep = speed / speedRateUp; //
	int currentSpeed = 0;
	bool ok = false;

	for (unsigned int i = 1; i < steps + 1; i++) {
		int percent = (i*100)/steps;

		if(percent <= speedRateUp) {
			currentSpeed += incrementStep;
		} else if(percent >= 100-speedRateUp) {
			currentSpeed -= incrementStep;
		}
		runStep(currentSpeed);
	}

//	for (unsigned int i = 1; i < steps + 1; i++) {
//
//		if (speed <= currentSpeed || ok) {
//			currentSpeed -= incrementStep;
//			ok  = true;
//		} else {
//			currentSpeed += incrementStep;
//		}
//		runStep(currentSpeed);
//	}

}

void taskRun(void *params) {
	bool currentDirection = true, retrying = false;
	long lastTime = 0, currentTime = 0;

	for (unsigned int i = 0; i < (averageSteps / 2); i++) {
		runStep(CALIBRATION_PPS);
	}

	while (true) {

		direction->write(currentDirection);
		currentDirection = (bool) !currentDirection;
		vTaskDelay(1);

		letsgo(currentPPS, averageSteps);

		currentTime = xTaskGetTickCount();

		if (xSemaphoreTake(hitWallSemaphore, 0)) {
			if(retrying) retrying = false;

			long diff = currentTime - lastTime;
			int rpm = ((double)((currentPPS*10 / 400)) * 60);
			DEBUGOUT("**********\r\n");
			DEBUGOUT("RPM: %d, PPS: %d, speedUp: %d\r\n", rpm, currentPPS*10, speedRateUp);

			lastTime = currentTime;

			currentPPS += TEST_INCREMENT_PPS;
			speedRateUp ++;
		} else {
			lastTime = currentTime;
			if(retrying) {
				DEBUGOUT("task out\r\n");
				break;
			} else {
				DEBUGOUT("Retry\r\n");
				retrying = true;
			}
		}
	}

	vTaskDelete(NULL);
}

void taskCalibration(void *params) {
	//DigitalIoPin firstSwitch(0, 27, DigitalIoPin::pullup, true); //L
	//DigitalIoPin secondSwitch(0, 28, DigitalIoPin::pullup, true); // R
	DigitalIoPin *statePin;

	bool currentDirection = false;
	bool leftPressed = false;
	bool rightPressed = false;

	unsigned int calibrationState = 0;
	unsigned int currentLapse = 0;
	unsigned int steps = 0;
	unsigned long lastTick = 0;
	unsigned long diff = 0;

	currentPPS = CALIBRATION_PPS;

	while (calibrating) {
		leftPressed = limL->read(); //firstSwitch.read();
		rightPressed = limR->read(); //secondSwitch.read();
		diff = xTaskGetTickCount() - lastTick;

		direction->write(currentDirection);
		if (calibrationState == 0) {
			runStep(currentPPS);

			// When either switches is pressed, go to the next state, change the direction and update the statePin and lastTick.
			if (leftPressed) {
				calibrationState++;
				currentDirection = (bool) !currentDirection;

				lastTick = xTaskGetTickCount();
				statePin = limR; //&secondSwitch;
			} else if (rightPressed) {
				calibrationState++;
				currentDirection = (bool) !currentDirection;

				lastTick = xTaskGetTickCount();
				statePin = limL; //&firstSwitch;
			}
		} else if (calibrationState == 1) {
			steps++;
			runStep(currentPPS);

			// A diff is used because the switch will be pressed while it tries to leave.
			if (diff > 100 && statePin->read()) {
				currentLapse++;
				currentDirection = (bool) !currentDirection;
				lastTick = xTaskGetTickCount();

				if (statePin == limL) { //&firstSwitch
					statePin = limR;//&secondSwitch;
				} else {
					statePin = limL; //&firstSwitch;
				}

				if (currentLapse >= CALIBRATION_LAPSES) {
					averageSteps = steps / CALIBRATION_LAPSES;
					calibrationState++;
				}
			}
		} else if (calibrationState == 2) {
			int len = averageSteps / 2;
			for (int i = 0; i < len; i++) {
				runStep(currentPPS);
			}

			// Done calibrating!
			calibrating = false;
		}
	}

#ifdef exo2
	xTaskCreate(taskReadDebug, "vTaskReadDebug", configMINIMAL_STACK_SIZE + 256,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskInputHandler, "vTaskInputHandler",
			configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);

//	xTaskCreate(taskLimitRead, "vTaskLimitRead", configMINIMAL_STACK_SIZE + 128,
//			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

#endif
#ifdef exo3
	xTaskCreate(taskRun, "vTaskRun", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif

	currentPPS = DEFAULT_PPS; //*****

	vTaskDelete(NULL);
}

int main(void) {
	prvSetupHardware();
	ITM_init();

	messageQueue = xQueueCreate(10, sizeof(Command));

	sbRIT = xSemaphoreCreateBinary();
	wallBinary = xSemaphoreCreateBinary();
	startBinary = xSemaphoreCreateBinary();
	hitWallSemaphore = xSemaphoreCreateBinary();
	blinkSemaphore = xSemaphoreCreateBinary();

	motor = new DigitalIoPin(0, 24, DigitalIoPin::output);
	direction = new DigitalIoPin(1, 0, DigitalIoPin::output);

	limL = new DigitalIoPin(0, 27, DigitalIoPin::pullup, true); // C++ object créé en haut et instancier ici
	limR = new DigitalIoPin(0, 28, DigitalIoPin::pullup, true); // les obj sont utilisé dans toutes les taches

#if defined(exo2) || defined(exo3)
	xTaskCreate(taskCalibration, "vTaskCalibration",
			configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif

#ifdef exo1
	xTaskCreate(taskReadDebug, "vTaskReadDebug", configMINIMAL_STACK_SIZE + 256,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(taskInputHandler, "vTaskInputHandler",
			configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif

#ifdef exo3
	xTaskCreate(taskBlink, "vTaskBlink", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif

	xTaskCreate(taskLimitRead, "vTaskLimitRead", configMINIMAL_STACK_SIZE + 128,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL); // 2fois ??

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}





