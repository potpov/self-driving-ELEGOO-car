little clever ELEGOO car

this is an Arduino Uno project. it uses the elegoo car kit: http://amzn.eu/d/fGHfR8n

required libraries:
freeRTOS
servo.h

NOTE: we use (proto) threads! and semaphores! and a lot of other evil stuff.

thread organization:

PRIORITY 1 group
thread 1: cruise control
thread 2: line assist
thread 3: engine control

PRIORITY 2 group
thread 1: lights control


each body has three phasis: 
1: syncronization. thread checks if other threads are executing or are waiting and decided if stops or go head.
2: business logic. thread is executing.
3: wake up phase. thread checks which threads need to be waken up and wake up one of them according to the following policy:

-engine wakes cruise OR line alternately
-line wakes engine (if engine is not waiting, do nothing.)
-cruise wakes engine (if engine is not waiting, it wakes up lights.)
-lights wakes up engine.
