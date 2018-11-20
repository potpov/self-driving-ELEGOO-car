/*
 Name:    car_thread.ino
 Created: 13/11/2018 9:54:35
 Author:  marco
*/

#include <Arduino_FreeRTOS.h>
#include "semphr.h"
#include "car_control.h"

/* thread constants */
#define LINE_SPACE 20 //space between two parallel line
#define STABLE_SPEED 100
#define REDUCTED_SPEED 60
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
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(e || be || c || bc || led || bled)
			bl = true;
		else {
			l = true;
			xSemaphoreGive(line_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(line_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}

		/* BUSINESS LOGIC */
		if(!line.centre()){
			line.setStable(false);
			if(line.left())
				line.setSuggested('L');
			else if(line.right())
				line.setSuggested('R');
			else
				line.setSuggested('A'); // no path
		}
		else {
			line.setStable(true);
			line.setSuggested('F');
		}

		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
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
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(e || be || l || bl || led || bled)
			bc = true;
		else {
			c = true;
			xSemaphoreGive(cruise_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(cruise_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}

		/* BUSINESS LOGIC */
		if(engine.getStatus() == 'F' || engine.getStatus() == 'S'){ 
			// dangerous situation
			if(cruise.getDistance() < 50) { 
				cruise.setEmergency(true);
			}
			// easy peasy lemon squeezy
			else {
				cruise.setEmergency(false);
				cruise.setPass(false); 
			}
		}
		//when we are stopped and stable, let's check if it is possible to pass.
		if(engine.getStatus() == 'S'){ 
			if(cruise.getEmergency()) {
				cruise.lookLeft();
				delay(350);
				cruise.setPass(cruise.getDistance() > 50); //set pass true if on the other line there's space
				cruise.lookForward();
				delay(350);
			}
		} 
		
		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
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
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(l || bl || c || bc || led || bled)
			be = true;
		else {
			e = true;
			xSemaphoreGive(engine_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(engine_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}

		/* BUSINESS LOGIC */
		if(line.getSuggested() == 'F') {
			engine.forward();
			engine.setSpeed(STABLE_SPEED);
		}
		else if(line.getSuggested() == 'L'){
			engine.setLeftSpeed(REDUCTED_SPEED);
		}
		else if(line.getSuggested() == 'R'){
			engine.setRightSpeed(REDUCTED_SPEED);
		}
		
		if(line.getSuggested() == 'A' || cruise.getEmergency()) {
			/* if we lost the path we've done.
			// but if we're stopping bc of the cruise control
			// there's still hope to pass the obstacle.
			// so let's give him the control */
			//choice = 1;
			engine.shutDown();
		}
		
		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
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
		choice = (choice + 1) % 3; // schedule the next thread fairly.
		xSemaphoreGive(mutual_ex);
		
	}
}

void turn_signal(void *pvParameters) {
	Serial.println("LIGHT THREAD STARTED.");
	light.offLight();
	for (;;) {
		/* SYNCRONIZATION */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(l || bl || c || bc || e || be)
			bled = true;
		else {
			led = true;
			xSemaphoreGive(led_sync);
		}
		xSemaphoreGive(mutual_ex);
		while(xSemaphoreTake(led_sync, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}

		/* BUSINESS LOGIC */
		if(cruise.getPass())
			light.leftLight();
		else
			light.offLight();
		
		/* SYNC ENDS -> WAKE UP THREADS */
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
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
