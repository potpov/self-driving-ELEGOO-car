// car_control.h

#ifndef _CAR_CONTROL_H
#define _CAR_CONTROL_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include <Servo.h>

// line sensors
#define CENTRAL_SENSOR 4
#define RIGHT_SENSOR 2
#define LEFT_SENSOR 10
// engines
#define R_ENGINE_POWER 5
#define R_FRONT_ENGINE_CONTROL 6
#define R_REAR_ENGINE_CONTROL 7
#define L_ENGINE_POWER 11
#define L_FRONT_ENGINE_CONTROL 9
#define L_REAR_ENGINE_CONTROL 8
// cruise control
#define SERVO_PIN 3
#define ECHO A4
#define TRIGGER A5
#define FORWARD_GRADE 145
#define LEFT_GRADE 179
#define RIGHT_GRADE 112
// light
#define LED 12
/* car constants */
#define STABLE_SPEED 95
#define REDUCTED_SPEED 30
#define INCREASED_SPEED 140
#define BACK_REDUCED_SPEED 60
#define BACK_INCREASED_SPEED 145
#define BREAKING_DISTANCE 50 // cm
#define CROSSBACK_DISTANCE 20 // cm
/* time constraints */
#define GOBACK_STABLE_TIME 2500
#define NOMORE_JUST_CROSSED_TIME 1200
#define LINE_TIMEOUT 2000
#define CROSS_TIMEOUT 2000

/*
 * this class ensures that the car is keeping the
 * line straight, suggests a direction and raises a warning
 * whenever the car is leaving the right path. this class
 * supports for the thread which manages the engines.
 */

class Line {
	bool  _Stable; 
	char  _SuggestedDirection; // F, B, P, A
	unsigned int _LineNumber; // current line

	bool _TimeoutFlag;
	unsigned long _TimeoutCount;
	unsigned long _LineTimeCount;

	public:

	Line() {
		_LineNumber = 0;
		_LineTimeCount = 0;
		_TimeoutFlag = false;
	}

	void setStable(bool val) { _Stable = val; }

	bool setSuggested(char val) {
		if(val == 'R' || val == 'L' || val == 'F' || val == 'A'){
		_SuggestedDirection = val;
		return true;
		}
		else
		return false;
  	}

  	bool getStable(){ return _Stable; }

  	char getSuggested(){ return _SuggestedDirection; }

  	bool right() { return digitalRead(RIGHT_SENSOR); }

  	bool  left() { return digitalRead(LEFT_SENSOR); }

  	bool  centre() { return digitalRead(CENTRAL_SENSOR); }

	bool lost() { return (!this->centre() && !this->left() && !this->right()); }

	void setLineNumber(unsigned int line) {
		_LineNumber = line; 
	}

	unsigned int getLineNumber(){ return _LineNumber; }

	/* 
	// those methods are used to understend how long the car is
	// on a line, to assume if it is stable and proceed to cross line.
	*/

	void restartLineCount() {
		_LineTimeCount = millis();
	}

	bool isLineStable() {
		return (millis() - _LineTimeCount > GOBACK_STABLE_TIME);
	}

	unsigned long getTimeOnThisLine(){
		return millis() - _LineTimeCount;
	}

	/* 
	// those methods are used to understend if the car is out of line
	// over a constant amount of time, calculation is made without 
	// blocking other threads.
	*/

	void startTimeoutCount() {
		if(!_TimeoutFlag) {
			_TimeoutFlag = true;
			_TimeoutCount = millis();
		}
	}

	void stopTimeoutCount() {
		_TimeoutFlag = false;
		_TimeoutCount = 0;
	}

	bool isTimeoutLine() {
		if(!_TimeoutFlag)
			return false;
		return (millis() - _TimeoutCount > LINE_TIMEOUT);
	}

	bool isTimeoutPass() {
		if(!_TimeoutFlag)
			return false;
		return (millis() - _TimeoutCount > CROSS_TIMEOUT);
	}
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

	Cruise() {}

	void init(){
		EBS.attach(SERVO_PIN);
		pinMode(ECHO, INPUT);
		pinMode(TRIGGER, OUTPUT);
		_EmergencyBreak = false;
		_AllowPass = false;
		this->lookForward(); // set the default position
  	}

	bool distance(){
		unsigned long timing;
		digitalWrite(TRIGGER, LOW);
		delayMicroseconds(2);
		digitalWrite(TRIGGER, HIGH);
		delayMicroseconds(20);
		digitalWrite(TRIGGER, LOW);
		//std pulseIn cannot be used
		timing = micros();
		while (digitalRead(ECHO) != HIGH) {
			if(micros() - timing > 80000) //timeout
				return false;
		} // waits for the pin to go from LOW to HIGH
		timing = micros();
		while (digitalRead(ECHO) != LOW && (micros() - timing < 80000)) {} // then waits for the pin to go LOW (timeout added)
		if(((float)((micros() - timing)) / 58) > 200) // out of range, timeout.
			return false;
		else {
			_Distance = ((float)((micros() - timing)) / 58);
			return true;
		}
  	}

	// ensuring that distance is stable (trade off for the timeout. we can't block the other threads).
  	void stableDistance() {
		uint8_t timeout;
		//DEBUG
		for(timeout = 0; timeout < 3; timeout ++){
			if(this->distance())
				return;
			delay(10);
		}

		//timeout reached, **hopefully** no obstacles on our way (don't do this with your own car).
		_Distance = 100;
		return;
	}
  
  	void lookForward(){
		EBS.write(FORWARD_GRADE);
  	}

  	void lookLeft(){
		EBS.write(LEFT_GRADE);
  	}

	void lookRight(){
		EBS.write(RIGHT_GRADE);
  	}
	void customPosition(unsigned int pos){
		EBS.write(pos);
	}

	void setPass(bool val) {
		_AllowPass = val;
	}

	void setBack(bool val) {
		_AllowBack = val;
	}

	void setEmergency(bool val) {
		_EmergencyBreak = val;
	}

	float getDistance() {
		this->stableDistance();  //update the value
		return _Distance;
	}

	bool getPass() {
		return _AllowPass;
	}

	bool getBack() {
		return _AllowBack;
	}

	bool getEmergency() {
		return _EmergencyBreak;
	}
};


/*
 * very simple class to enlight a signal whenever
 * it's time to switch line.
 */

class Light {
	bool _status;
  	public:

  	Light() {}

	void init(){
		pinMode(LED, OUTPUT);
	}

  	// use these functions in a loop with the engine phase variable as breaking condition
	void leftLight() {
		_status = true;
		digitalWrite(LED, HIGH);
	}

	void offLight() {
		_status = false;
		digitalWrite(LED, LOW);
	}

	bool isLightOn(){ return _status; }

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

	Engine() {
		_LeftSpeed = 0;
		_RightSpeed = 0;
		_Status = 'S';
		analogWrite(R_ENGINE_POWER, 0);
		analogWrite(L_ENGINE_POWER, 0);
		digitalWrite(R_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(L_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
		digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
	}

	void start() {
		_Status = 'F';
		analogWrite(R_ENGINE_POWER, _RightSpeed);
		analogWrite(L_ENGINE_POWER, _LeftSpeed);
	}

	void forward() {
		digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
		digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
	}

	void left() {
		digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(L_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
		digitalWrite(L_REAR_ENGINE_CONTROL, HIGH);
	}

	void right() {
		digitalWrite(R_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(R_REAR_ENGINE_CONTROL, HIGH);
		digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
	}

	void shutDown() {
		_RightSpeed = 0;
		_LeftSpeed = 0;
		analogWrite(R_ENGINE_POWER, 0);
		analogWrite(L_ENGINE_POWER, 0);
	}

	void setSpeed(uint8_t genSpeed) {
		_RightSpeed = genSpeed;
		_LeftSpeed = genSpeed;
		analogWrite(R_ENGINE_POWER, _RightSpeed);
		analogWrite(L_ENGINE_POWER, _LeftSpeed);
	}

	void setRightSpeed(uint8_t rightSpeed) {
		_RightSpeed = rightSpeed;
		analogWrite(R_ENGINE_POWER, _RightSpeed);
	}

	void setLeftSpeed(uint8_t leftSpeed) {
		_LeftSpeed = leftSpeed;
		analogWrite(L_ENGINE_POWER, _LeftSpeed);
	}

	void pass(){
		this->start(); // re-start engines if stopped
		this->setLeftSpeed(REDUCTED_SPEED);
		this->setRightSpeed(INCREASED_SPEED);	
		delay(650); //time to cross the path
	}

	void back(){
		this->start(); // re-start engines if stopped
		this->setRightSpeed(BACK_REDUCED_SPEED);
		this->setLeftSpeed(BACK_INCREASED_SPEED);	
		delay(650); //time to cross the path
	}

	uint8_t getRightSpeed() { return _RightSpeed; }

	uint8_t getLeftSpeed() { return _LeftSpeed; }

	char getStatus() { return _Status; }

	void setStatus(char status) { _Status = status; }
};

#endif