/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#include "chip.h"
#include "board.h"
#include "FreeRTOS.h"
#include <string.h>
#include <stdlib.h>

#include <cr_section_macros.h>

// TODO: insert other include files here
#include "task.h"
#include "DigitalIoPin.h"
#include "ITM_write.h"
#include "semphr.h"
#include "strTool.h"

// TODO: insert other definitions and declarations here



//LED
#define MAXLEDS 3
static const uint8_t ledpins[MAXLEDS] = {25, 3, 1};
static const uint8_t ledports[MAXLEDS] = {0, 0, 1};
#define RED 0
#define GREEN 1
#define BLUE 2


struct Color {
	int r;
	int g;
	int b;
};


//*****************
//*****************

//#define ex1
//#define ex2
#define ex3

//*****************
//*****************

#if defined(ex1) || defined(ex3)
#define MAX_FREC 1000 //1kHz
#endif
#ifdef ex1
#define DUTY_CYCLE 995 //5%
#endif
#ifdef ex2
#define MAX_FREC 20000 //50Hz
#define CENTER_PULSE 1500
#endif

//*****************
//*****************

QueueHandle_t queue;
QueueHandle_t colorQueue;

void SCT_Init(void)
{
#ifdef ex3
Chip_SCT_Init(LPC_SCTLARGE0);
Chip_SCT_Init(LPC_SCTLARGE1);
#else
Chip_SCT_Init(LPC_SCT0);
#endif

#ifdef ex1
Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, ledports[RED], ledpins[RED]);
#endif

#ifdef ex2
Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, 0, 8);
#endif

#ifdef ex3
	Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, ledports[RED], ledpins[RED]);
	Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT1_O, ledpins[GREEN], ledpins[GREEN]);
	Chip_SWM_MovablePortPinAssign(SWM_SCT1_OUT2_O, ledpins[BLUE], ledpins[BLUE]);
#endif

 LPC_SCT0->CONFIG |= (1 << 17); // two 16-bit timers, auto limit
 LPC_SCT0->CTRL_L |= (72-1) << 5; // set prescaler, SCTimer/PWM clock = 1 MHz
#ifdef ex1
 LPC_SCT0->MATCHREL[0].L = MAX_FREC-1;//100-1; // match 0 @ 1000/1MHz = 1ms (1 kHz PWM freq)
 LPC_SCT0->MATCHREL[1].L = DUTY_CYCLE; // match 1 used for duty cycle (in 10 steps)
#endif
#ifdef ex2
 LPC_SCT0->MATCHREL[0].L = MAX_FREC-1;//100-1; // match 0 @ 1000/1MHz = 10 usec (1 kHz PWM freq)
 LPC_SCT0->MATCHREL[1].L = CENTER_PULSE; // match 1 used for duty cycle (in 10 steps)

#endif
#if defined(ex1) || defined(ex2)
 LPC_SCT0->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
 LPC_SCT0->EVENT[0].CTRL = (1 << 12); // match 0 condition only
 LPC_SCT0->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
 LPC_SCT0->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only
 LPC_SCT0->OUT[0].SET = (1 << 0); // event 0 will set SCTx_OUT0
 LPC_SCT0->OUT[0].CLR = (1 << 1); // event 1 will clear SCTx_OUT0
 LPC_SCT0->CTRL_L &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg
#endif

#ifdef ex3
	LPC_SCTLARGE0->CONFIG |= (1 << 17); // Two 16-bit timers, auto limit
	LPC_SCTLARGE0->CTRL_L |= (72 - 1) << 5; // Set prescaler, SCTimer/PWM Clock = 1 Mhz

	LPC_SCTLARGE0->MATCHREL[0].L = MAX_FREC - 1; // Match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCTLARGE0->MATCHREL[1].L = MAX_FREC; // Match 1 user for duty cycle (in 10 steps)
	LPC_SCTLARGE0->MATCHREL[2].L = MAX_FREC; // Match 1 user for duty cycle (in 10 steps)

	LPC_SCTLARGE0->EVENT[0].STATE = 0xFFFFFFFF; // Event 0 happens in all states
	LPC_SCTLARGE0->EVENT[0].CTRL = (0 << 0) | (1 << 12); // Match 0 condition only

	LPC_SCTLARGE0->EVENT[1].STATE = 0xFFFFFFFF; // Event 1 happens in all state
	LPC_SCTLARGE0->EVENT[1].CTRL = (1 << 0) | (1 << 12); // Match 1 condition only

	LPC_SCTLARGE0->EVENT[2].STATE = 0xFFFFFFFF; // Event 0 happens in all states
	LPC_SCTLARGE0->EVENT[2].CTRL = (2 << 0) | (1 << 12); // Match 0 condition only

	LPC_SCTLARGE0->OUT[0].SET = (1 << 0); // Event 0 will set SCTx_OUT0
	LPC_SCTLARGE0->OUT[0].CLR = (1 << 1); // Event 1 will clear SCTx_OUT0

	LPC_SCTLARGE0->OUT[1].SET = (1 << 0); // Event 0 will set SCTx_OUT0
	LPC_SCTLARGE0->OUT[1].CLR = (1 << 2); // Event 1 will clear SCTx_OUT0

	LPC_SCTLARGE0->CTRL_L &= ~(1 << 2); // Unhalt by clearing bit 2 of CTRL req

	LPC_SCTLARGE1->CONFIG |= (1 << 17); // Two 16-bit timers, auto limit
	LPC_SCTLARGE1->CTRL_L |= (72 - 1) << 5; // Set prescaler, SCTimer/PWM Clock = 1 Mhz

	LPC_SCTLARGE1->MATCHREL[0].L = MAX_FREC - 1; // Match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCTLARGE1->MATCHREL[1].L = MAX_FREC; // Match 1 user for duty cycle (in 10 steps)

	LPC_SCTLARGE1->EVENT[0].STATE = 0xFFFFFFFF; // Event 0 happens in all states
	LPC_SCTLARGE1->EVENT[0].CTRL = (0 << 0) | (1 << 12); // Match 0 condition only

	LPC_SCTLARGE1->EVENT[1].STATE = 0xFFFFFFFF; // Event 1 happens in all state
	LPC_SCTLARGE1->EVENT[1].CTRL = (1 << 0) | (1 << 12); // Match 1 condition only

	LPC_SCTLARGE1->OUT[2].SET = (1 << 0); // Event 0 will set SCTx_OUT0
	LPC_SCTLARGE1->OUT[2].CLR = (1 << 1); // Event 1 will clear SCTx_OUT0

	LPC_SCTLARGE1->CTRL_L &= ~(1 << 2); // Unhalt by clearing bit 2 of CTRL req

#endif
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
	SCT_Init();
#if defined(ex1) || defined(ex3)
	ITM_init();
#endif
	/* Initial LED0 state is off */
	Board_LED_Set(RED, false);
	Board_LED_Set(GREEN, false);
	Board_LED_Set(BLUE, false);
}

//**********************************
//**********************************
#ifdef ex3



Color parseColor(std::string line) {
	std::queue<std::string> parts = StringUtil::split(line, " ");
	Color color;

	std::string type = parts.front();
	if (!(type == "rbg" || type == "rgb")) {
		color.r = -1;
		color.g = -1;
		color.b = -1;
		return color;
	}
	parts.pop();

	std::string colorStr = parts.front();
	if(colorStr.size() != 7 || colorStr.find("#", 0, 1) == std::string::npos) {
		color.r = -1;
		color.g = -1;
		color.b = -1;
		return color;
	}
	parts.pop();

	colorStr = colorStr.substr(1, colorStr.size());

	char *pEnd;
	long colorLong = strtol(colorStr.c_str(), &pEnd, 16);

	color.r = (colorLong >> 16) & 0xFF;
	color.g = (colorLong >> 8) & 0xFF;
	color.b = colorLong & 0xFF;

	return color;
}

#endif


//**********************************
//**********************************

#ifdef ex1
void taskLEDBrightness(void *param) {
	DigitalIoPin sw1(0, 17, DigitalIoPin::input, true);
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, true);
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);

	int power = DUTY_CYCLE;
	while (true) {
		if (sw1.read()) {
			power -= (sw2.read()) ? 50 : 10;

			if (power < 0) {
				power = 0;
			}
			xQueueSendToBack(queue, &power, portMAX_DELAY);
		}

		if (sw3.read()) {
			power += (sw2.read()) ? 50 : 10;
			if (power > 1000){
				power = 1000;
			}
			xQueueSendToBack(queue, &power, portMAX_DELAY);
		}

		LPC_SCT0->MATCHREL[1].L = power;

		vTaskDelay(100);
	}
}


static void vDebugPrintTask(void *pvParameters){
	char buffer[64];
	int power;
	while(1){
		if(xQueueReceive(queue, &power, portMAX_DELAY)) {
			power = 100-(power/10);
			sprintf(buffer, "%d", power);
			strcat(buffer," \% \n \r");

			ITM_write(buffer);
		}
	}
}
#endif

//**********************************
//**********************************

#ifdef ex2
void taskServo(void *params) {
	DigitalIoPin sw1(0, 17, DigitalIoPin::pullup, true);
	DigitalIoPin sw2(1, 11, DigitalIoPin::pullup, true);
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, true);

	int power = 500; //500
	int lim = 1000; //1000

	while (true) {
		if(power < 0) {
			power = 0;
		} else if(power > lim) {
			power = lim;
		}
		//DEBUGOUT("power: %d\r\n", power);
		if (sw1.read()) {
			power += 10;//10
			LPC_SCT0->MATCHREL[1].L = lim + power;
		} else if(sw2.read()) {
			power = 500;
			LPC_SCT0->MATCHREL[1].L = CENTER_PULSE; //1500
		} else if(sw3.read()) {
			power -= 10;//10
			LPC_SCT0->MATCHREL[1].L = lim + power;
		} else {
			LPC_SCT0->MATCHREL[1].L = MAX_FREC;
		}
		vTaskDelay(20);
	}
}
#endif

//**********************************
//**********************************

#ifdef ex3

void taskLedRGB(void *params) {
	Color color;

	while (true) {
		xQueueReceive(colorQueue, &color, portMAX_DELAY);
		if(color.r != -1 && color.g != -1 && color.b != -1) {
			int redAmount = (color.r * (-MAX_FREC) / 255 + MAX_FREC); // in %*10
			int greenAmount = (color.g * (-MAX_FREC) / 255 + MAX_FREC);
			int blueAmount = (color.b * (-MAX_FREC) / 255 + MAX_FREC);

			LPC_SCTLARGE0->MATCHREL[1].L = redAmount;
			LPC_SCTLARGE0->MATCHREL[2].L = greenAmount;
			LPC_SCTLARGE1->MATCHREL[1].L = blueAmount;

			DEBUGOUT("ok\r\n");
		} else {
			//There was an error :/
			DEBUGOUT("error\r\n");

		}
	}
}
void taskReadUSB(void *params) {
	std::string line;
//	char usbChar, enter = '\n';
//	int len=0;
//
//	while(1) {
//		 len = USB_receive((uint8_t *)&usbChar, 79);
//
//		 if (usbChar != 255) {
//			 if (usbChar == 13) {
//				 USB_send((uint8_t *) &enter, len);
//				 Color color = parseColor(line);
//				 xQueueSend(colorQueue, &color, 0);
//				 line = "";
//			 } else {
//				 line += usbChar;
//			 }
//		 }
//		 USB_send((uint8_t *) &usbChar, len);
//	}

	while (true) {
		char uartChar = Board_UARTGetChar();
		if (uartChar != 255) {
			if (uartChar == 13) {
				Board_UARTPutChar('\n');

				Color color = parseColor(line);
				xQueueSend(colorQueue, &color, 0);

				line = "";
			} else {
				line += uartChar;
			}

			Board_UARTPutChar(uartChar);
		}
	}

}
#endif

int main(void) {

	prvSetupHardware();


#ifdef ex1
	queue = xQueueCreate(10, sizeof(int));

	xTaskCreate(taskLEDBrightness, "vTaskLEDBrightness", configMINIMAL_STACK_SIZE,
			NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(vDebugPrintTask, "vDebugPrintTask", configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif
#ifdef ex2
	xTaskCreate(taskServo, "vTaskServo", configMINIMAL_STACK_SIZE + 128,
				NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif
#ifdef ex3
	colorQueue = xQueueCreate(10, sizeof(Color));

	xTaskCreate(taskLedRGB, "vTaskLedRGB", configMINIMAL_STACK_SIZE + 256, NULL,
				(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
	xTaskCreate(taskReadUSB, "taskReadUSB", configMINIMAL_STACK_SIZE + 128, NULL,
				(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
#endif
	vTaskStartScheduler();
    return 0 ;
}
