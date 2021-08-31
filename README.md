self-driving ELEGOO car

this is an Arduino Uno project. it uses the elegoo car kit: http://amzn.eu/d/fGHfR8n

required libraries:
freeRTOS
servo.h

NOTE: we use (proto) threads! and semaphores! and a lot of other evil stuff.

thread organization:

thread 1: cruise control,
thread 2: line assist,
thread 3: engine control,
thread 4: lights control


each body has three phases: 
1: syncronization. thread checks if other threads are executing or are waiting and decides if it is better to stop or go head.
2: business logic. thread is executing.
3: wake up phase. thread checks which threads need to be waken up and wakes up one of them according to the following policy:

-engine wakes cruise OR line OR alternately with a RoundRobin approach.
-line tries to wake engine first.
-cruise tries to wake engine first.
-lights tries to wake engine first.

each thread, in order to avoid deadlocks, wakes at least a thread if there's one waiting.

demo is available here
[![](http://img.youtube.com/vi/9X0A19ljb68/0.jpg)](http://www.youtube.com/watch?v=9X0A19ljb68 "demo")
