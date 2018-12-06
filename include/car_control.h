// car_control.h

#ifndef _CAR_CONTROL_H
#define _CAR_CONTROL_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include <Servo.h>
#include "const.h" // constants for the car methods

/*
 * this class ensures that the car is keeping the
 * line straight, suggests a direction and raises a warning
 * whenever the car is leaving the right path. this class
 * supports for the thread which manages the engines.
 */

class Counter {
	int _counter;
	
public:

	Counter();
	
	void reset();

	void increment();

	int value();

};

class Line {
	Counter lx, rx;
	bool  _Stable; 
	char  _SuggestedDirection; // F, B, P, A
	unsigned int _LineNumber; // current line

	bool _TimeoutFlag;
	unsigned long _TimeoutCount;
	unsigned long _LineTimeCount;

public:

	Line();

	void setStable(bool val);

	bool setSuggested(char val);

  	bool getStable();

  	char getSuggested();

  	bool right();

  	bool  left();

  	bool  centre();

	bool lost();

	void setLineNumber(unsigned int line);

	unsigned int getLineNumber();

	Counter &leftCounter();

	Counter &rightCounter();

	/* 
	// those methods are used to understend how long the car is
	// on a line, to assume if it is stable and can proceed to cross line.
	*/

	void restartLineCount();

	bool isLineStable();

	unsigned long getTimeOnThisLine();

	/* 
	// those methods are used to understend if the car is out of line
	// over a constant amount of time, calculation is made without 
	// blocking other threads.
	*/

	void startTimeoutCount();

	void stopTimeoutCount();

	bool isTimeoutLine();

	bool isTimeoutPass();
};


/*
 * this class checks if there's an object in front of the car,
 * checks also if there's the chance to pass the object.
 */

class Cruise {
	float  _Distance;
	bool  _EmergencyBreak;
	bool  _AllowPass;
	bool _AllowBack;
	Servo EBS;

public:

	Cruise();

	void init();

	bool distance();

	// ensuring that distance is stable (trade off for the timeout. we can't block the other threads).
  	void stableDistance();
  
  	void lookForward();

  	void lookLeft();

	void lookRight();

	void customPosition(unsigned int pos);

	void setPass(bool val);

	void setBack(bool val);

	void setEmergency(bool val);

	float getDistance();

	bool getPass();

	bool getBack();

	bool getEmergency();

};


/*
 * very simple class to enlight a signal whenever
 * it's time to switch line.
 */

class Light {
	bool _Status;

public:

  	Light();

	void init();

  	// use these functions in a loop with the engine phase variable as breaking condition

	void leftLight();

	void offLight();

	bool isLightOn();

};

/*
 * this class controls the engines, set up their speeds,
 * check if we are ready to move, set a status for the car
 * which is helpful to the other sensors whenever they
 * need to verify the consistency of their data
 */

class Engine {
	uint8_t _LeftSpeed;
	uint8_t _RightSpeed;
	// S = stopped, F = forward, P = passing, C = cursing
	char  _Status;

public:

	Engine();

	void start();

	void forward();

	void left();

	void right();

	void shutDown();

	void setSpeed(uint8_t genSpeed);

	void setRightSpeed(uint8_t rightSpeed);

	void setLeftSpeed(uint8_t leftSpeed);

	void pass();

	void back();

	uint8_t getRightSpeed();

	uint8_t getLeftSpeed();

	char getStatus();

	void setStatus(char status);
};

#endif