#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <Arduino_FreeRTOS.h>
#include "semphr.h"

/* for better performances we avoided using malloc here. */
#ifndef THREADS_NUM
#define THREADS_NUM 4
#endif

struct syncronizer {
	bool executing;
	bool blocked;
	SemaphoreHandle_t sem;
};

class Scheduler {
	struct syncronizer sync[THREADS_NUM];
	SemaphoreHandle_t mutual_ex;
	uint8_t roundRobinCounter;

	public:

	Scheduler(){
		for(uint8_t i = 0; i < THREADS_NUM; i++) {
			sync[i].blocked = false;
			sync[i].executing = false;
			sync[i].sem = xSemaphoreCreateCounting(1, 0);
		}
		mutual_ex = xSemaphoreCreateMutex();
		roundRobinCounter = 0;
	}
		
	/* methods to garantee  mutual exclusion */
	void enterSafeZone(){
		while(xSemaphoreTake(mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
	}

	void leaveSafeZone(){
		xSemaphoreGive(mutual_ex);
	}

	/* ALWAYS BE SURE YOU ENTERED IN THE SAFE ZONE BEFORE EXECUTE THOSE METHODS. */

	bool isSlotAvailable(uint8_t mypid){
		for(uint8_t i=0; i<THREADS_NUM; i++) {
			if(mypid == i)
				continue;
			if(sync[i].executing || sync[i].blocked)
				return false;
		}
		return true;
	}

	bool wake(uint8_t mypid, uint8_t target) {
		if(sync[target].blocked) {
			sync[target].blocked = false;
			sync[target].executing = true;
			sync[mypid].executing = false;
			xSemaphoreGive(sync[target].sem);
			return true;
		}
		else
			return false;
	}

	void leaveSlot(uint8_t mypid){ // only if you cant wakeup anyone.
		sync[mypid].executing = false;
	}

	/* methods to gain the leadership */
	void loginRequest(uint8_t mypid){
		if(isSlotAvailable(mypid)){
			sync[mypid].executing = true;
			xSemaphoreGive(sync[mypid].sem);
		}
		else
			sync[mypid].blocked = true;
	}

	// warning: DON'T DO THIS without executing leavesafezone() first! deadlock garantee!
	void checkLogin(uint8_t mypid) {
		while(xSemaphoreTake(sync[mypid].sem, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
	}

	/* 
	// RR: as this is a shared class, the internal variable _preference_ can be 
	// edited by all the threads which call the function. threads which want 
	// to have a real roundrobin have to keep an internal variable and use it
	// as preference.
	*/
	bool roundRobin(uint8_t mypid, int8_t preference = -1){
		if(preference >= 0)
			roundRobinCounter = preference;
		
		for(uint8_t i = 0; i<THREADS_NUM; i++){
			if(this->wake(mypid, roundRobinCounter))
				return true;
			roundRobinCounter = (roundRobinCounter + 1) % THREADS_NUM;
		}
		return false;
	}
};

#endif