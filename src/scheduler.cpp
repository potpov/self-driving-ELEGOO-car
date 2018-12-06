#include "scheduler.h"

Scheduler::Scheduler(){
	for(uint8_t i = 0; i < THREADS_NUM; i++){
		_Sync[i].blocked = false;
		_Sync[i].sem = xSemaphoreCreateCounting(1, 0);
	}
	_Mutual_ex = xSemaphoreCreateMutex();
	_Leader = NONE;
	_RoundRobinCounter = 0;
}

void Scheduler::enterSafeZone(){
	while(xSemaphoreTake(_Mutual_ex, portMAX_DELAY) != pdTRUE) {vTaskDelay(3);}
 }

void Scheduler::leaveSafeZone(){
	xSemaphoreGive(_Mutual_ex);
}

bool Scheduler::isSlotAvailable(){
	return !(_Leader >= 0);
}

bool Scheduler::wake(uint8_t target){
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

void Scheduler::leaveSlot(){ // only if you cant wakeup anyone.
	_Leader = NONE;
}

bool Scheduler::loginRequest(uint8_t mypid){
	if(isSlotAvailable() || mypid == _Leader){
		_Leader = mypid;
		xSemaphoreGive(_Sync[mypid].sem);
		return true;
	}
	else
		_Sync[mypid].blocked = true;
		return false;
}

void Scheduler::checkLogin(uint8_t mypid){
	while(xSemaphoreTake(_Sync[mypid].sem, portMAX_DELAY) != pdTRUE) { vTaskDelay(3); }
}

/* RR: as this is a shared class, the internal variable _preference_ can be 
// edited by all the threads which call the function. threads which want 
// to have a real roundrobin have to keep an internal variable and use it
// as preference.
*/

bool Scheduler::roundRobin(int8_t preference){
	if(preference >= 0)
		_RoundRobinCounter = preference;
	
	for(uint8_t i = 0; i<THREADS_NUM; i++){
		if(this->wake(_RoundRobinCounter))
			return true;
		_RoundRobinCounter = (_RoundRobinCounter + 1) % THREADS_NUM;
	}
	return false;
}