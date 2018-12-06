/*
 Name:    car_thread.ino
 Created: 13/11/2018 9:54:35
 Author:  Marco Cipriano
*/

#include <Arduino_FreeRTOS.h>
#include "semphr.h"
#include "car_control.h"
#include "scheduler.h"
#include "const.h" // PID infos can be found here

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
	for (;;) {
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(LINE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(LINE_T);

		/* BUSINESS LOGIC */
		//checking timeout variables first
		if(line.lost())
			line.startTimeoutCount();
		else
			line.stopTimeoutCount();
		
		// in line handler
		if(engine.getStatus() == 'F'){
			if(line.isTimeoutLine() && line.getTimeOnThisLine() > NOMORE_JUST_CROSSED_TIME)
				line.setSuggested('A');	
			if(line.centre()){
				line.setStable(true);
				line.leftCounter().reset();
				line.rightCounter().reset();
				line.setSuggested('F');
			}
			else if(line.left()){
				line.setStable(false);	
				line.setSuggested('L');
				line.leftCounter().increment();
			}
			else if(line.right()){
				line.setStable(false);
				line.setSuggested('R');	
				line.rightCounter().increment();
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

		//come back handler
		if(engine.getStatus() == 'B'){
			for(;;){
				if(line.isTimeoutPass()) {
					line.setSuggested('A'); // stop. timeout reached.
					break;
				}
				if(!line.lost()) {
					line.setSuggested('L');
					line.setLineNumber(0); // record that we changed the line.
					line.restartLineCount(); // restart time recording for this new line.
					cruise.setBack(false); 
					break;
				}
			}
		}

		/* SYNC ENDS -> WAKE UP THREADS */
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(ENGINE_T)) // try to wake engine first.
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();
		delay(25); // as someone once said: "pausetta"
	}
}


void cruise_assist(void *pvParameters) {
	for (;;) {
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(CRUISE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(CRUISE_T);

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
		if(line.getLineNumber() == 1 && engine.getStatus() == 'F' && line.getStable() && line.isLineStable()){
			cruise.lookRight();
			delay(350);
			cruise.setBack(cruise.getDistance() > CROSSBACK_DISTANCE); //set back true if in the other line there's enought space
			cruise.lookForward();
			delay(350);
		}

		/* SYNC ENDS -> WAKE UP THREADS */
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(ENGINE_T)) // try to wake engine first.
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();	
		delay(25); // as someone once said: "pausetta"
	}  
}


void engine_control(void *pvParameters) {
	engine.start();
	uint8_t preference = 0;
	for (;;) {
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(ENGINE_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(ENGINE_T);

		/* BUSINESS LOGIC */
		if(line.getSuggested() == 'F') {
			engine.forward();
			engine.setSpeed(STABLE_SPEED);
			engine.setStatus('F');
		}
		else if(line.getSuggested() == 'L'){
			engine.setLeftSpeed(REDUCTED_SPEED - line.leftCounter().value());
			engine.setRightSpeed(INCREASED_SPEED + line.leftCounter().value());
			engine.setStatus('F');
		}
		else if(line.getSuggested() == 'R'){
			engine.setRightSpeed(REDUCTED_SPEED - line.rightCounter().value());
			engine.setLeftSpeed(INCREASED_SPEED + line.rightCounter().value());
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
			
			if(!light.isLightOn()) {
				scheduler.enterSafeZone();
				if(scheduler.wake(LIGHT_T)){
				scheduler.leaveSafeZone();
				continue;
				}
				scheduler.leaveSafeZone();
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

		// coming back to first line
		if(cruise.getBack()){
			if(engine.getStatus() != 'B') { //first time
				engine.back();
				engine.setStatus('B');
			}

			if(!light.isLightOn()) {
				scheduler.enterSafeZone();
				if(scheduler.wake(LIGHT_T)){
				scheduler.leaveSafeZone();
				continue;
				}
				scheduler.leaveSafeZone();
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
		
		/* SYNC ENDS -> WAKE UP THREADS */
		scheduler.enterSafeZone();
		if(!scheduler.roundRobin(preference))
			scheduler.leaveSlot();
		scheduler.leaveSafeZone();
		preference = (preference + 1) % THREADS_NUM;
		delay(25); // as someone once said: "pausetta"
	}
}


void turn_signal(void *pvParameters) {
	light.offLight();
	for (;;) {
		/* SYNCRONIZATION */
		scheduler.enterSafeZone();
		scheduler.loginRequest(LIGHT_T);
		scheduler.leaveSafeZone();
		scheduler.checkLogin(LIGHT_T);
		
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
		delay(25); // as someone once said: "pausetta"
	}
}
