/*
 Name:    car_thread.ino
 Created: 13/11/2018 9:54:35
 Author:  marco
*/

#include <Arduino_FreeRTOS.h>
#include "semphr.h"
#include "car_control.h"
#include "scheduler.h"

/* using strings instead of PIDS for a more readble code */
#define ENGINE_T	0
#define LINE_T 		1
#define CRUISE_T 	2
#define LIGHT_T		3
#define THREADS_NUM 4

/* define thread's bodies */
void line_assist(void *pvParameters);
void cruise_assist(void *pvParameters);
void engine_control(void *pvParameters);
void turn_signal(void *pvParameters);

/* shared objects */
Cruise	cruise;
Line	line;
Light	light;
Engine 	engine;
Scheduler scheduler;

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
		scheduler.enterSafeZone();
		scheduler.loginRequest(LINE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(LINE_T);

		Serial.println("LINE in control.");
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
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(ENGINE_T)) // try to wake engine first.
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();
	}
}


void cruise_assist(void *pvParameters) {
	Serial.println("CRUISE THREAD STARTED.");
	for (;;) {

		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(CRUISE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(CRUISE_T);

		Serial.println("cruise in control.");
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
				/* SYNC ENDS -> WAKE UP THREADS */
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(ENGINE_T)) // try to wake engine first.
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();	
	}  
}


void engine_control(void *pvParameters) {
	Serial.println("ENGINE THREAD STARTED.");	
	engine.start();
	uint8_t preference = 0;
	for (;;) {
	
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(ENGINE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(ENGINE_T);

		Serial.println("engine in control.");
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
				if(scheduler.wake(LIGHT_T))
					continue;
			}
			for(;;){ //priority to line to catch the cross.
				scheduler.enterSafeZone();
				if(scheduler.wake(LINE_T)){
					scheduler.leaveSafeZone();
					break;
				}
				scheduler.leaveSafeZone();
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
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(preference))
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();
		preference = (preference + 1) % THREADS_NUM;
	}
}


void turn_signal(void *pvParameters) {
	Serial.println("LIGHT THREAD STARTED.");
	light.offLight();
	for (;;) {
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(LIGHT_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(LIGHT_T);
		
		Serial.println("LIGHT in control.");
		/* BUSINESS LOGIC */
		if(cruise.getPass() && engine.getStatus() == 'P')
			light.leftLight();
		else
			light.offLight();
		
		/* SYNC ENDS -> WAKE UP THREADS */
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(LINE_T))
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();
	}
}
