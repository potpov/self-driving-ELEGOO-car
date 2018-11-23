/*
 Name:    car_thread.ino
 Created: 13/11/2018 9:54:35
 Author:  marco
*/

#include <Arduino_FreeRTOS.h>
#include "semphr.h"
#include "car_control.h"


/* debug */
#define VERBOSE false

/* define thread's bodies */
void line_assist(void *pvParameters);
void cruise_assist(void *pvParameters);
void engine_control(void *pvParameters);
void turn_signal(void *pvParameters);

/* private sems for sync */
SemaphoreHandle_t mutual_ex = xSemaphoreCreateMutex();
SemaphoreHandle_t cruise_sync = xSemaphoreCreateCounting(1, 0);
SemaphoreHandle_t line_sync = xSemaphoreCreateCounting(1, 0);
SemaphoreHandle_t engine_sync = xSemaphoreCreateCounting(1, 0);
SemaphoreHandle_t led_sync = xSemaphoreCreateCounting(1, 0);
/* status variables */
bool c = false;
bool bc = false;
bool l = false;
bool bl = false;
bool e = false;
bool be = false;
bool led = false;
bool bled = false;
/* shared objects */
Cruise	cruise;
Line	line;
Light	light;
Engine 	engine;

void setup() { 
	Serial.begin(9600);
	while (!Serial) { ; }
	cruise.init();
	light.init();
	/* launching the threads */
	xTaskCreate(line_assist, (const portCHAR *)"line assist", 128, NULL, 1, NULL);
	xTaskCreate(cruise_assist, (const portCHAR *)"cruise control", 128, NULL, 1, NULL);
	xTaskCreate(engine_control, (const portCHAR *)"engines", 128, NULL, 1, NULL);
	xTaskCreate(turn_signal, (const portCHAR *)"lights", 64, NULL, 1, NULL);
}

void loop() {}


void line_assist(void *pvParameters) {
	Serial.println("LINE ASSIST THREAD STARTED.");
	for (;;) {
		/* SYNCRONIZATION */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		if(e || be || c || bc || led || bled)
			bl = true;
		else {
			l = true;
			xSemaphoreGive(line_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(line_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}

		/* BUSINESS LOGIC */
		
		//checking timeout variables first
		if(line.lost())
			line.startTimeoutCount();
		else
			line.stopTimeoutCount();
		
		// in line handler
		if(engine.getStatus() == 'F'){
			if(line.isTimeoutLine())
				line.setSuggested('A');	
			if(line.centre()){
				line.setStable(true);
				line.setSuggested('F');
			}
			else if(line.left()){
				line.setStable(false);	
				line.setSuggested('L');
			}
			else if(line.right()){
				line.setStable(false);
				line.setSuggested('R');	
			}
		}
		
		//pass handler
		if(engine.getStatus() == 'P'){
			for(;;){
				if(line.isTimeoutPass()) {
					line.setSuggested('A'); // stop. timeout reached.
					break;
				}
				if(!line.lost()) {
					line.setSuggested('R');
					line.setLineNumber(1); // record that we changed the line.
					line.restartLineCount(); // restart time recording for this new line.
					cruise.setPass(false); 
					break;
				}
			}
		}

		//come back to the previous line.
		/*
		if(engine.getStatus() == 'B'){
			if(!line.getTimeoutFlag())
				line.setTimeoutFlag(true);

			while(!line.right()){
				if(line.getTimeoutFlag() && line.getTimeoutValue() > CROSS_TIMEOUT)	
					line.setSuggested('A'); // stop. timeout reached.
			}
			line.setSuggested('L');
			line.setLineNumber(0); // record that we changed the line.
			line.setFirstTime(); 
			cruise.setBack(false); 
			
		}
		*/

		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		l = false;
		if(be){ //wake up engine first.
			e = true;
			be = false;
			xSemaphoreGive(engine_sync);
		}
		else if(bc) {
			c = true;
			bc = false;
			xSemaphoreGive(cruise_sync);
		}
		else if(bled){
			led = true;
			bled = false;
			xSemaphoreGive(led_sync);
		}
		xSemaphoreGive(mutual_ex);
	}
}


void cruise_assist(void *pvParameters) {
	Serial.println("CRUISE THREAD STARTED.");
	for (;;) {

		/* SYNCRONIZATION */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		if(e || be || l || bl || led || bled)
			bc = true;
		else {
			c = true;
			xSemaphoreGive(cruise_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(cruise_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}

		/* BUSINESS LOGIC */
		if(engine.getStatus() == 'F' || engine.getStatus() == 'S'){ 
			// dangerous situation
			if(cruise.getDistance() < BREAKING_DISTANCE) { 
				cruise.setEmergency(true);
			}
			// easy peasy lemon squeezy
			else {
				cruise.setEmergency(false);
				cruise.setPass(false); // we can go head.
			}
		}

		// when we are stopped and stable, let's check if it's possible to pass.
		if(engine.getStatus() == 'S' && cruise.getEmergency() && line.getLineNumber() == 0 && !cruise.getPass()){ 
			cruise.lookLeft();
			delay(350);
			cruise.setPass(cruise.getDistance() > BREAKING_DISTANCE); //set pass true if on the other line there's space
			cruise.lookForward();
			delay(350);
		} 

		//when we are stable we can come back on the first line.
		/*
		if(line.getLineNumber() == 1 && engine.getStatus() == 'F' && line.getTimeOnLine() > GOBACK_STABLE_TIME){
			cruise.lookRight();
			delay(350);
			cruise.setBack(cruise.getDistance() > CROSSBACK_DISTANCE); //set back true if in the other line there's enought space
			cruise.lookForward();
			delay(350);
		}
		*/

		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		c = false;
		if(be){ //wake up engine first.
			e = true;
			be = false;
			xSemaphoreGive(engine_sync);
		}
		else if(bl){
			l = true;
			bl = false;
			xSemaphoreGive(line_sync);
		}
		else if(bled){
			led = true;
			bled = false;
			xSemaphoreGive(led_sync);
		}
		xSemaphoreGive(mutual_ex);

		
	}  
}


void engine_control(void *pvParameters) {
	Serial.println("ENGINE THREAD STARTED.");
	uint8_t choice = 0; // alternate threads.	
	engine.start();
	for (;;) {
	
		/* SYNCRONIZATION */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		if(l || bl || c || bc || led || bled)
			be = true;
		else {
			e = true;
			xSemaphoreGive(engine_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(engine_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}

		/* BUSINESS LOGIC */
		if(line.getSuggested() == 'F') {
			engine.forward();
			engine.setSpeed(STABLE_SPEED);
			engine.setStatus('F');
		}
		else if(line.getSuggested() == 'L'){
			engine.setLeftSpeed(REDUCTED_SPEED);
			engine.setRightSpeed(INCREASED_SPEED);
			engine.setStatus('F');
		}
		else if(line.getSuggested() == 'R'){
			engine.setRightSpeed(REDUCTED_SPEED);
			engine.setLeftSpeed(INCREASED_SPEED);
			engine.setStatus('F');
		}
		
		if(cruise.getEmergency()) {
			engine.shutDown();
			engine.setStatus('S');
		}

		if(line.getSuggested() == 'A') {
			engine.shutDown();
			delay(999999999);
		}

		if(cruise.getPass()){
			if(engine.getStatus() != 'P') { //first time
				engine.pass();
				engine.setStatus('P');
			}
			for(;;){ //priority to line to catch the cross.
				while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
				if(bl) {
					e = false;
					l = true;
					bl = false;
					xSemaphoreGive(line_sync);
					xSemaphoreGive(mutual_ex); 
					break;
				}
				xSemaphoreGive(mutual_ex);
				delay(10);
			}
			continue;
		}

		/*
		if(cruise.getBack() && engine.getStatus() != 'B'){
			engine.back();
		}
		*/



		
		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		e = false;
		// priority to line.
		switch(choice){
			case 0:
				if(bl){
					l = true;
					bl = false;
					xSemaphoreGive(line_sync);
				}
				else if(bc){
					c = true;
					bc = false;
					xSemaphoreGive(cruise_sync);
				}
				else if(bled){
					led = true;
					bled = false;
					xSemaphoreGive(led_sync);
				}
				break;
			case 1:
				if(bc){
					c = true;
					bc = false;
					xSemaphoreGive(cruise_sync);
				}
				else if(bl){
					l = true;
					bl = false;
					xSemaphoreGive(line_sync);
				}
				else if(bled){
					led = true;
					bled = false;
					xSemaphoreGive(led_sync);
				}
				break;
			case 2:
				if(bled){
					led = true;
					bled = false;
					xSemaphoreGive(led_sync);
				}
				else if(bc){
					c = true;
					bc = false;
					xSemaphoreGive(cruise_sync);
				}
				else if(bl){
					l = true;
					bl = false;
					xSemaphoreGive(line_sync);
				}
		}
		xSemaphoreGive(mutual_ex);		
		choice = (choice + 1) % 3; // schedule the next thread fairly.
	}
}


void turn_signal(void *pvParameters) {
	Serial.println("LIGHT THREAD STARTED.");
	light.offLight();
	for (;;) {
		/* SYNCRONIZATION */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		if(l || bl || c || bc || e || be)
			bled = true;
		else {
			led = true;
			xSemaphoreGive(led_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(led_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}

		/* BUSINESS LOGIC */
		if(cruise.getPass() && engine.getStatus() == 'P')
			light.leftLight();
		else
			light.offLight();
		
		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
		led = false;
		if(bc) {
			c = true;
			bc = false;
			xSemaphoreGive(cruise_sync);
		}
		else if(bl){
			l = true;
			bl = false;
			xSemaphoreGive(line_sync);
		}
		else if(be){ //wake up engine first.
			e = true;
			be = false;
			xSemaphoreGive(engine_sync);
		}
		xSemaphoreGive(mutual_ex);
	}
}
