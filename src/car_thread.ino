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


/* sem definitions */
SemaphoreHandle_t line_m = xSemaphoreCreateMutex();
SemaphoreHandle_t main_engine_m = xSemaphoreCreateMutex();
SemaphoreHandle_t cruise_m = xSemaphoreCreateMutex();
//SemaphoreHandle_t reader_m = xSemaphoreCreateCounting(1, 0);
//SemaphoreHandle_t writer_m = xSemaphoreCreateCounting(1, 0);

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
	xTaskCreate(line_assist, (const portCHAR *)"line assist", 128, NULL, 3, NULL);
	xTaskCreate(cruise_assist, (const portCHAR *)"cruise control", 128, NULL, 3, NULL);
	xTaskCreate(engine_control, (const portCHAR *)"engines", 128, NULL, 3, NULL);
	xTaskCreate(turn_signal, (const portCHAR *)"lights", 64, NULL, 3, NULL);
}

void loop() {}

void line_assist(void *pvParameters) {
	Serial.println("LINE ASSIST THREAD STARTED.");
	for (;;) {
		/* assuming we are going forward */
		while(xSemaphoreTake(line_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
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
		xSemaphoreGive(line_m);
	}
}

void cruise_assist(void *pvParameters) {
	Serial.println("CRUISE THREAD STARTED.");
	float left;
	char engineStatus;
	for (;;) {
		while(xSemaphoreTake(main_engine_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		engineStatus = engine.getStatus();
		xSemaphoreGive(main_engine_m);

		while(xSemaphoreTake(cruise_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(engineStatus == 'F'){ 
			// dangerous situation
			if(cruise.getDistance() < 50) { 
				cruise.setEmergency(true);
			}
			// easy peasy lemon squeezy
			else
				cruise.setEmergency(false);
		}
		//when we are stopped and stable, let's check if it is possible to pass.
		else if(engineStatus == 'S'){ 
			if(cruise.getEmergency()) {
				cruise.lookLeft();
				delay(200);
				cruise.setPass(cruise.getDistance() > 50); //set pass true if on the other line there's space
				cruise.lookForward();
			}
		} 
		xSemaphoreGive(cruise_m);
		
		delay(500); // avoid line thread starvation [TO FIX]
		if(VERBOSE){
			Serial.print("valore di pass: ");
			Serial.println(left);
			Serial.print("valore di emergency: ");
			Serial.println(cruise.getEmergency());
			Serial.print("valore distanza: ");
			Serial.println(cruise.getDistance());
			delay(1000);
		}
	}  
}

void engine_control(void *pvParameters) {
	Serial.println("ENGINE THREAD STARTED.");
	char suggested;
	for (;;) {
		engine.start();
		while(xSemaphoreTake(line_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		suggested = line.getSuggested();
		xSemaphoreGive(line_m);

		while(xSemaphoreTake(main_engine_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(suggested == 'F') {
			engine.forward();
			engine.setSpeed(STABLE_SPEED);
		}
		else if(suggested == 'L'){
			engine.setLeftSpeed(REDUCTED_SPEED);
		}
		else if(suggested == 'R'){
			engine.setRightSpeed(REDUCTED_SPEED);
		}
		xSemaphoreGive(main_engine_m);
		
		while(xSemaphoreTake(cruise_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		if(suggested == 'A' || cruise.getEmergency()) {
			xSemaphoreGive(cruise_m);
			//ask for mutex and while(getsuggested() == A || cruise.getEmergency()) wait
			engine.shutDown();
			delay(999999);
		}
		else
			xSemaphoreGive(cruise_m);
		
	}
}

void turn_signal(void *pvParameters) {
	Serial.println("LIGHT THREAD STARTED.");
	light.offLight();
	bool pass;
	for (;;) {
		while(xSemaphoreTake(cruise_m, portMAX_DELAY) != pdTRUE) {vTaskDelay(10);}
		pass = cruise.getPass();
		xSemaphoreGive(cruise_m);

		if(pass)
			light.leftLight();
		else
			light.offLight();
		
		delay(150);
	}
}
