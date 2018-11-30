#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <Arduino_FreeRTOS.h>
#include "semphr.h"

/* for better performances we avoided using malloc here. */
#ifndef THREADS_NUM
#define THREADS_NUM 4
#endif

#define NONE		-1

struct syncronizer {
	bool blocked;
	SemaphoreHandle_t sem;
};

class Scheduler {
	struct syncronizer _Sync[THREADS_NUM];
	SemaphoreHandle_t _Mutual_ex;
	int8_t _Leader; // note which thread is executing
	uint8_t _RoundRobinCounter;
	public:

	Scheduler(){
		for(uint8_t i = 0; i < THREADS_NUM; i++) {
			_Sync[i].blocked = false;
			_Sync[i].sem = xSemaphoreCreateCounting(1, 0);
		}
		_Mutual_ex = xSemaphoreCreateMutex();
		_Leader = NONE;
		_RoundRobinCounter = 0;
	}
		
	/* methods to garantee  mutual exclusion */
	void enterSafeZone(){
		while(xSemaphoreTake(_Mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
	}

	void leaveSafeZone(){
		xSemaphoreGive(_Mutual_ex);
	}

	/* ALWAYS BE SURE YOU ENTERED IN THE SAFE ZONE BEFORE EXECUTE THOSE METHODS. */

	bool isSlotAvailable(){
		return !(_Leader >= 0);
	}

	bool wake(uint8_t target) {
		if(_Sync[target].blocked) {
			_Sync[target].blocked = false;
			_Sync[_Leader].blocked = true; // stop the current thread
			_Leader = target; // record the pid as the new exec tread.
			xSemaphoreGive(_Sync[target].sem);
			return true;
		}
		else
			return false;
	}

	void leaveSlot(){ // only if you cant wakeup anyone.
		_Leader = NONE;
	}

	/* methods to gain the leadership */
	bool loginRequest(uint8_t mypid){
		if(isSlotAvailable() || mypid == _Leader){
			_Leader = mypid;
			xSemaphoreGive(_Sync[mypid].sem);
			return true;
		}
		else
			_Sync[mypid].blocked = true;
			return false;
	}

	// warning: DON'T DO THIS without executing leavesafezone() first! deadlock garanteed!
	void checkLogin(uint8_t mypid) {
		while(xSemaphoreTake(_Sync[mypid].sem, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
	}

	/* RR: as this is a shared class, the internal variable _preference_ can be 
	// edited by all the threads which call the function. threads which want 
	// to have a real roundrobin have to keep an internal variable and use it
	// as preference.
	*/
	bool roundRobin(int8_t preference = NONE){
		if(preference >= 0)
			_RoundRobinCounter = preference;
		
		for(uint8_t i = 0; i<THREADS_NUM; i++){
			if(this->wake(_RoundRobinCounter))
				return true;
			_RoundRobinCounter = (_RoundRobinCounter + 1) % THREADS_NUM;
		}
		return false;
	}
};

#endif