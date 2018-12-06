#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <Arduino_FreeRTOS.h>
#include "semphr.h"
#include "const.h" // constants for scheduler

/* for better performances we avoided using malloc here. */
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

	Scheduler();
		
	/* methods to garantee  mutual exclusion */
	void enterSafeZone();

	void leaveSafeZone();

	/* ALWAYS BE SURE YOU ENTERED IN THE SAFE ZONE BEFORE EXECUTE THOSE METHODS. */

	bool isSlotAvailable();

	bool wake(uint8_t target);

	void leaveSlot(); // only if you cant wakeup anyone.

	/* methods to gain the leadership */
	bool loginRequest(uint8_t mypid);

	// warning: DON'T DO THIS without executing leavesafezone() first! deadlock garanteed!
	void checkLogin(uint8_t mypid);

	bool roundRobin(int8_t preference = NONE);
};

#endif