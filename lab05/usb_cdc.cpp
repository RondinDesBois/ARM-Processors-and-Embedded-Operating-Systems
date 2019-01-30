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
#include "ITM_write.h"

#include <mutex>
#include "Fmutex.h"
#include "user_vcom.h"

#include "strTool.h"
#include <string.h>
#include <stdlib.h>

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



//SemaphoreHandle_t mutex, binary, counter;

//QueueHandle_t queue;
QueueHandle_t colorQueue;

#define MAX_FREC 1000 //1kHz

void SCT_Init(void)
{
	Chip_SCT_Init(LPC_SCTLARGE0);
	Chip_SCT_Init(LPC_SCTLARGE1);

	Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, ledports[RED], ledpins[RED]);
	Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT1_O, ledports[GREEN], ledpins[GREEN]);
	Chip_SWM_MovablePortPinAssign(SWM_SCT1_OUT2_O, ledports[BLUE], ledpins[BLUE]);




	LPC_SCT0->CONFIG |= (1 << 17); // two 16-bit timers, auto limit
	LPC_SCT0->CTRL_L |= (72-1) << 5; // set prescaler, SCTimer/PWM clock = 1 MHz
	LPC_SCT0->MATCHREL[0].L = MAX_FREC-1;//100-1; // match 0 @ 1000/1MHz = 1ms (1 kHz PWM freq)
	LPC_SCT0->MATCHREL[1].L = MAX_FREC; // match 1 used for duty cycle (in 10 steps)
	LPC_SCT0->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
	LPC_SCT0->EVENT[0].CTRL = (1 << 12); // match 0 condition only
	LPC_SCT0->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
	LPC_SCT0->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only
	LPC_SCT0->OUT[0].SET = (1 << 0); // event 0 will set SCTx_OUT0
	LPC_SCT0->OUT[0].CLR = (1 << 1); // event 1 will clear SCTx_OUT0

	LPC_SCT0->CONFIG |= (1 << 18); // two 16-bit timers, auto limit
	LPC_SCT0->CTRL_H |= (72 - 1) << 5; //
	LPC_SCT0->MATCHREL[0].H = MAX_FREC - 1; // Match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCT0->MATCHREL[1].H = MAX_FREC; // Match 1 user for duty cycle (in 10 steps)
	LPC_SCT0->EVENT[2].STATE = 0xFFFFFFFF; // Event 0 happens in all states
	LPC_SCT0->EVENT[2].CTRL =  (0 << 0) | (1 << 12) | (1 << 4); //
	LPC_SCT0->EVENT[3].STATE = 0xFFFFFFFF; // Event 1 happens in all state
	LPC_SCT0->EVENT[3].CTRL = (1 << 0) | (1 << 12) | (1 << 4 ); // Match 1 condition only
	LPC_SCT0->OUT[1].SET = (1 << 2); //
	LPC_SCT0->OUT[1].CLR = (1 << 3); //

	LPC_SCT0->CTRL_L &= ~(1 << 2); // Unhalt by clearing bit 2 of CTRL req
	LPC_SCT0->CTRL_H &= ~(1 << 2); // Unhalt by clearing bit 2 of CTRL req

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
	ITM_init();

	/* Initial LED0 state is off */
	Board_LED_Set(RED, false);
	Board_LED_Set(GREEN, false);
	Board_LED_Set(BLUE, false);

}



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

	colorStr = colorStr.substr(1, colorStr.size()); // after the #

	char *pEnd;
	long colorLong = strtol(colorStr.c_str(), &pEnd, 16); //c_str récupérer le char* contenue dans un objet de type string. strtol pour le metre en long... base 16
// 16 pour base 16 (hexa)
	color.r = (colorLong >> 16) & 0xFF; //>> shift(décalage) bits right. If the left operand is long
	color.g = (colorLong >> 8) & 0xFF; // & is AND op
	color.b = colorLong & 0xFF;

	return color;
}


void taskLedRGB(void *params) {
	Color color;
	//char *ok="ok\r\n";
	//char *err="error\r\n";
	while (true) {
		xQueueReceive(colorQueue, &color, portMAX_DELAY);
		if(color.r != -1 && color.g != -1 && color.b != -1) {
			int redAmount = (color.r * (-MAX_FREC) / 255 + MAX_FREC); // in %*10
			int greenAmount = (color.g * (-MAX_FREC) / 255 + MAX_FREC);
			int blueAmount = (color.b * (-MAX_FREC) / 255 + MAX_FREC);

			LPC_SCTLARGE0->MATCHREL[1].L = redAmount;
			//LPC_SCTLARGE0->MATCHREL[2].L = greenAmount;
			LPC_SCT0->MATCHREL[1].H = greenAmount;
			LPC_SCTLARGE1->MATCHREL[1].L = blueAmount;

			DEBUGOUT("ok\r\n");
			//USB_send((uint8_t *) ok, sizeof(ok));

		} else {
			//There was an error :/
			DEBUGOUT("error\r\n");
			//USB_send((uint8_t *) err, sizeof(err));
		}
	}
}
void taskReadUSB(void *params) {
	std::string line;
	int len;
	char usbChar, enter = '\n';

	while(1) {
		len = USB_receive((uint8_t *)&usbChar, 79);

		if (usbChar != 255) {
			if (usbChar == 13) {
				USB_send((uint8_t *) &enter, len);
				Color color = parseColor(line);
				xQueueSend(colorQueue, &color, 0);
				line = "";
			} else {
				line += usbChar;
			}
		}
		USB_send((uint8_t *) &usbChar, len);
	}
}


/* the following is required if runtime statistics are to be collected */
//extern "C" {
//
//void vConfigureTimerForRunTimeStats( void ) {
//	Chip_SCT_Init(LPC_SCTSMALL1); //LPC_SCTSMALL1
//	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
//	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
//}
//
//}
///* end runtime statictics collection */
//
///* Sets up system hardware */
//static void prvSetupHardware(void)
//{
//	SystemCoreClockUpdate();
//	Board_Init();
//
//	/* Initial LED0 state is off */
//	Board_LED_Set(0, false);
//
//}

#if 0
static void receive_task(void *pvParameters) {

	uint32_t lenr, lens;
	char string[32], read;//buffer[32]
	int i=0;

	bool question = false;
	//char you[] = "[you]: ";
	//char str1[] = "\r\n ";
	while(1){

		if(string[i]!='\n' && string[i]!='\r' && i<58) {
			lenr = USB_receive((uint8_t *)&read, 79);
			if (read!=EOF) {
				string[i]= read;
				USB_send((uint8_t *) &read, lenr);
				fflush(stdout);
				if(string[i]=='?') question=true;
				i++;
			}
		} else {
			USB_send((uint8_t *)string, 32);
			fflush(stdout);
			i=0;
		}

		//lens = sprintf(string, "[you]: %s \r \n",string);
		//		for (i = 0; i < sizeof(you); ++i) {
		//			buffer[i]=you[i];
		//		}
		//		for (i = sizeof(you); i < 31; ++i) {
		//			buffer[i]=string[i-sizeof(you)];
		//		}
		//buffer[31]=0;
		// exemple :
		//		char uartChar = Board_UARTGetChar();
		//		if (uartChar != EOF_CHAR) {
		//			if (uartChar == ENTER_CHAR) {
		//				Board_UARTPutChar('\n');
		//
		//				Color color = parseColor(line);
		//				xQueueSend(colorQueue, &color, 0);
		//
		//				line = "";
		//			} else {
		//				line += uartChar;
		//			}
		//
		//			Board_UARTPutChar(uartChar);
		//		}
		//	}




		//		for (i = 0; i < 32; ++i) {
		//			string[i]=0;
		//
		//		}
		//DEBUGOUT("\r [you]: %s \r \n", string);



		if (question) {
			xSemaphoreGive(counter);
			vTaskDelay(10);
			question=false;

		} else vTaskDelay(1);

		vTaskDelay(50);
	}
}
#endif





/* send data and toggle thread */
//
//static void send_task(void *pvParameters) {
//	bool LedState = false;
//	uint32_t count = 0;
//	while (1) {
//
//
//		char str[32];
//		int len = sprintf(str, "Counter: %lu runs really fast\r\n", count);
//		USB_send((uint8_t *)str, len);
//
//		//		char str[] = "moii\r\n";
//		//		USB_send((uint8_t *)str, sizeof(str));
//
//		Board_LED_Set(0, LedState);
//		LedState = (bool) !LedState;
//		count++;
//
//		vTaskDelay(configTICK_RATE_HZ / 50);
//	}
//}


#if 0
static void send_task(void *pvParameters) {
	bool LedState = false;
	uint32_t count = 0;
	char str1[] = "moii\r\n";
	char str2[] = "voila! \r\n";
	while (1) {


		if(xSemaphoreTake(counter, portMAX_DELAY)) {


			USB_send((uint8_t *)str1, sizeof(str1));

			vTaskDelay(configTICK_RATE_HZ*3);


			USB_send((uint8_t *)str2, sizeof(str2));
			//				switch((rand() %5)+1){
			//				case 1: DEBUGOUT("\n\r[Oracle]: why ? \r \n\n"); break;
			//				case 2: DEBUGOUT("\n\r[Oracle]: you are a penda!! \r \n\n"); break;
			//				case 3: DEBUGOUT("\n\r[Oracle]: you will fall in love soon... <3\r \n\n"); break;
			//				case 4: DEBUGOUT("\n\r[Oracle]: hahahaha x) \r \n\n"); break;
			//				case 5: DEBUGOUT("\n\r[Oracle]: you are not handsome :o \r \n\n"); break;
			//				}


			//xSemaphoreGive(counter);
			vTaskDelay(configTICK_RATE_HZ*2);
		} else vTaskDelay(1);

		//***



		Board_LED_Set(0, LedState);
		LedState = (bool) !LedState;
		count++;

		vTaskDelay(configTICK_RATE_HZ / 50);
	}
}
#endif
/* LED1 toggle thread */

//static void receive_task(void *pvParameters) {
//	bool LedState = false;
//
//	while (1) {
//		char str[80];
//		uint32_t len = USB_receive((uint8_t *)str, 79);
//
//
//
//		//		char * str = NULL;
//		//		uint32_t len = USB_receive((uint8_t *)str, 79);
//
//		str[len] = 0; /* make sure we have a zero at the end so that we can print the data */
//
//		ITM_write(str);
//		USB_send((uint8_t *)str, sizeof(str));
//
//		if (*str=='?') {
//			xSemaphoreGive(counter);
//		}
//		Board_LED_Set(1, LedState);
//		LedState = (bool) !LedState;
//	}
//}



//static void receive_task(void *pvParameters) {
//	bool LedState = false;
//
//	while (1) {
////		char str[80];
////		uint32_t len = USB_receive((uint8_t *)str, 79);
//
//		char * str = NULL;
//		uint32_t len = USB_receive((uint8_t *)str, 79);
//
//		str[len] = 0; /* make sure we have a zero at the end so that we can print the data */
//		ITM_write(str);
//
//		Board_LED_Set(1, LedState);
//		LedState = (bool) !LedState;
//	}
//}


int main(void) {

	prvSetupHardware();

	colorQueue = xQueueCreate(10, sizeof(Color));

	xTaskCreate(taskLedRGB, "vTaskLedRGB", configMINIMAL_STACK_SIZE + 256, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
	xTaskCreate(taskReadUSB, "taskReadUSB", configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);


	//	ITM_init();
	//	mutex = xSemaphoreCreateMutex();
	//	//binary = xSemaphoreCreateBinary();
	//	counter = xSemaphoreCreateCounting(10,0);
	//
	//	/* LED1 toggle thread */
	//	xTaskCreate(send_task, "Tx",
	//			configMINIMAL_STACK_SIZE*3, NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);
	//
	//	/* LED1 toggle thread */
	//	xTaskCreate(receive_task, "Rx",
	//			configMINIMAL_STACK_SIZE*3 , NULL, (tskIDLE_PRIORITY + 1UL),
	//			(TaskHandle_t *) NULL);
	//
	//	/* LED2 toggle thread */
	xTaskCreate(cdc_task, "CDC",
			configMINIMAL_STACK_SIZE *3, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
	//

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
