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
#define FORWARD_GRADE 140
#define LEFT_GRADE 179
#define RIGHT_GRADE 90
// light
#define LED 12

/*
 * this class ensures that the car is keeping the
 * line straight, suggests a direction and raises a warning
 * whenever the car is leaving the right path. this class
 * supports for the thread which manages the engines.
 */

class Line {
	bool  _Stable;
  // F means forward, R suggests to go right, L suggests to go left, A suggests to stop
	char  _SuggestedDirection;

public:

	Line() {
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

};


/*
 * this class checks if there's an object in front of the car,
 * checks also if there's the chance to pass the object.
 */

class Cruise {
	float  _Distance;
	bool  _EmergencyBreak;
	bool  _AllowPass;
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

	void customPosition(unsigned int pos){
		EBS.write(pos);
	}

	void setPass(bool val) {
		_AllowPass = val;
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

	bool getEmergency() {
		return _EmergencyBreak;
	}
};


/*
 * very simple class to enlight a signal whenever
 * it's time to switch line.
 */

class Light {
  	public:

  	Light() {
		pinMode(LED, OUTPUT);
  	}

  // use these functions in a loop with the engine phase variable as breaking condition
	void leftLight() {
		digitalWrite(LED, LOW);
		delayMicroseconds(20);
		digitalWrite(LED, HIGH);
		delayMicroseconds(20);
	}

	void offLight() {
		digitalWrite(LED, LOW);
	}

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
		_Status = 'F';
		digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
		digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
	}

	void left() {
		_Status = 'C';
		digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(L_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
		digitalWrite(L_REAR_ENGINE_CONTROL, HIGH);
	}

	void right() {
		_Status = 'C';
		digitalWrite(R_FRONT_ENGINE_CONTROL, LOW);
		digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
		digitalWrite(R_REAR_ENGINE_CONTROL, HIGH);
		digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
	}

	void shutDown() {
		_RightSpeed = 0;
		_LeftSpeed = 0;
		_Status = 'S';
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

	uint8_t getRightSpeed() { return _RightSpeed; }

	uint8_t getLeftSpeed() { return _LeftSpeed; }

	char getStatus() { return _Status; }

};

#endif