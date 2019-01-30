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


#include "FreeRTOS.h"
#include "task.h"
#include "DigitalIoPin.h"
#include "semphr.h"





#define STEP() {motor.write(true); vTaskDelay(1); motor.write(false); vTaskDelay(1);}



SemaphoreHandle_t  binary;
DigitalIoPin *limL;
DigitalIoPin *limR;



/*--------------------------------------------------------------------------*/
/*-----------------------------------ex1------------------------------------*/
#if 0
static void vReadLimitSWTask(void *pvParameters){


	while (1) {
		if (limR->read()) {
			Board_LED_Set(1, false);
			vTaskDelay(10);
		} else Board_LED_Set(1, true);

		if (limL->read()) {
			Board_LED_Set(0, false);
			vTaskDelay(10);
		}else Board_LED_Set(0, true);

		vTaskDelay(10);
	}
}


static void vReadButtonTask(void *pvParameters){

	DigitalIoPin motor(0, 24, DigitalIoPin::output, false);
	DigitalIoPin dir(1, 0, DigitalIoPin::output, false);

	DigitalIoPin sw1(0, 17, DigitalIoPin::input, false);
	DigitalIoPin sw3(1, 9, DigitalIoPin::pullup, false);


	while (1) {

		if (!sw1.read() && sw3.read() && limR->read() && limL->read()) {
			dir.write(false); //clockwise
			STEP();

		} else if (!sw3.read() && sw1.read() && limR->read() && limL->read()) {
			dir.write(true);//counterclockwise
			STEP();

		} else motor.write(false);
	}
}
#endif





/*--------------------------------------------------------------------------*/
/*-----------------------------------ex2------------------------------------*/
#if 0
static void vReadLimitSw(void *pvParameters){

	bool LedState = false;

	while(!limR->read() || !limL->read()) {
		Board_LED_Set(2, LedState);
		LedState = (bool) !LedState;
		vTaskDelay(configTICK_RATE_HZ/10 );
	}
	Board_LED_Set(2, false);
	xSemaphoreGive(binary);
	while(1){

		if(!limR->read()){
			Board_LED_Set(1, true);
			xSemaphoreGive(binary);
		} else Board_LED_Set(1, false);
		if (!limL->read()){
			Board_LED_Set(0, true);
			xSemaphoreGive(binary);
		} else Board_LED_Set(0, false);

		vTaskDelay(1);
	}

}

static void vRunSteper(void *pvParameters){

	DigitalIoPin motor(0, 24, DigitalIoPin::output, false);
	DigitalIoPin dir(1, 0, DigitalIoPin::output, false);

	bool toTheLeft = true;
	if (xSemaphoreTake(binary, portMAX_DELAY)){
		while(1){
			if (xSemaphoreTake(binary, 0)) {

				if(!limR->read() && limL->read()){ 	//right switch off
					dir.write(toTheLeft);			//then counterclockwise

					STEP();


				} else if(!limL->read() && limR->read()){ //left switch off
					dir.write(!toTheLeft);				//then clockwise

					STEP();

				} else if(!limR->read() && !limL->read()) {	//both switch off
					motor.write(false);						//stop 5sec while both open

					vTaskDelay(configTICK_RATE_HZ*5);
				}
			} else {									//if nothing happened
				STEP(); 								//run!
			}
		}
	}
}

#endif



/*--------------------------------------------------------------------------*/
/*-----------------------------------ex3------------------------------------*/
#if 1

static void vReadLimitSw(void *pvParameters){

	bool LedState = false;
	TickType_t timerRed = 0, timerGreen = 0;

	while(!limR->read() || !limL->read()) {
		Board_LED_Set(2, LedState);
		LedState = (bool) !LedState;
		vTaskDelay(configTICK_RATE_HZ/10 );
	}
	Board_LED_Set(2, false);
	vTaskDelay(10);
	xSemaphoreGive(binary); // 1 er étape  les 2 switch on eté laché, l'autre tache recois le binary et run
	while(1){
							//2 la logique est si il y a un événement alors ca give binary
		if(!limR->read()){
			timerGreen=xTaskGetTickCount();
			Board_LED_Set(1, true); //green
			xSemaphoreGive(binary);
		} else if ((xTaskGetTickCount() <= timerGreen+configTICK_RATE_HZ && timerGreen > 0)){
			//xSemaphoreGive(binary);
		} else {
			Board_LED_Set(1, false);
			timerGreen = 0;
		}
		if(!limL->read()){
			timerRed=xTaskGetTickCount();
			Board_LED_Set(0, true); //red
			xSemaphoreGive(binary);
		} else if ((xTaskGetTickCount() <= timerRed+configTICK_RATE_HZ && timerRed > 0)){
			//xSemaphoreGive(binary);
		} else {
			Board_LED_Set(0, false);
			timerRed = 0;
		}

		vTaskDelay(1);
	}

}


static void vRunSteper(void *pvParameters){

	DigitalIoPin motor(0, 24, DigitalIoPin::output, false);
	DigitalIoPin dir(1, 0, DigitalIoPin::output, false);


	int i, cmt = 0, avr = 0, line = 0;
	bool toTheLeft = true, nextDir;
	if (xSemaphoreTake(binary, portMAX_DELAY)){
		while(line <= 2) {  // choose the nb of travel switch to switch

			if (xSemaphoreTake(binary, 0)) {

				if(!limR->read()){ 					//right switch off
					dir.write(toTheLeft);			//then counterclockwise
					while (!limR->read()) STEP();

					line++;
					avr += cmt;
					cmt = 0;
					nextDir= (bool) toTheLeft;

				} else if(!limL->read()){ 				//left switch off
					dir.write(!toTheLeft);				//then clockwise
					while (!limL->read()) STEP();
					line++;
					avr += cmt;
					cmt = 0;
					nextDir= (bool) !toTheLeft;
				}

			} else { 					//if nothing happened
				if (line != 0) cmt++;	//only not for the first round

				STEP(); 				//run
			}
		}

		avr = avr/(line-1);
		Board_LED_Set(2, true);
		vTaskDelay(configTICK_RATE_HZ*2);
		Board_LED_Set(2, false);

		while(1) {
			for (i = 0; i < avr-2; ++i) {
				STEP();
			}
			nextDir = (bool) !nextDir;
			dir.write(nextDir);
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
int main(void) {

	prvSetupHardware();

	binary = xSemaphoreCreateBinary();

	limL = new DigitalIoPin(0, 27, DigitalIoPin::pullup, false); // C++ object créé en haut et instancier ici
	limR = new DigitalIoPin(0, 28, DigitalIoPin::pullup, false); // les obj sont utilisé dans toutes les taches

	//ex1
#if 0
	xTaskCreate(vReadLimitSWTask,"vReadLimitSWTask", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);

	xTaskCreate(vReadButtonTask, "vReadButtonTask", configMINIMAL_STACK_SIZE + 256, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif

	//ex2 & ex3
#if 1
	xTaskCreate(vReadLimitSw,"vReadLimitSw", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);

	xTaskCreate(vRunSteper,"vRunSteper", configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t *) NULL);
#endif

	vTaskStartScheduler();
	return 0 ;
}
