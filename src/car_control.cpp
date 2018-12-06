#include "car_control.h"

/* 
// COUNTER CLASS METHODS
*/

Counter::Counter(){ this->reset(); }

void Counter::reset(){ _counter = 0; }

void Counter::increment(){ _counter++; }

int Counter::value(){ return _counter * COUNTER_UNIT_DIMENSION; }

/*
// LINE CLASS METHODS
*/

Line::Line(){
	_LineNumber = 0;
	_LineTimeCount = 0;
	_TimeoutFlag = false;
}

void Line::setStable(bool val) { _Stable = val; }

bool Line::setSuggested(char val) {
	if(val == 'R' || val == 'L' || val == 'F' || val == 'A'){
	_SuggestedDirection = val;
	return true;
	}
	else
	return false;
}

bool Line::getStable(){ return _Stable; }

char Line::getSuggested(){ return _SuggestedDirection; }

bool Line::right(){ return digitalRead(RIGHT_SENSOR); }

bool Line::left(){ return digitalRead(LEFT_SENSOR); }

bool Line::centre(){ return digitalRead(CENTRAL_SENSOR); }

bool Line::lost(){ 
	return (!this->centre() && !this->left() && !this->right()); 
}

void Line::setLineNumber(unsigned int line){
	_LineNumber = line; 
}

unsigned int Line::getLineNumber(){ return _LineNumber; }

Counter & Line::leftCounter(){ return lx; }

Counter & Line::rightCounter(){ return rx; }

void Line::restartLineCount(){
	_LineTimeCount = millis();
}

bool Line::isLineStable(){
	return (millis() - _LineTimeCount > GOBACK_STABLE_TIME);
}

unsigned long Line::getTimeOnThisLine(){
	return millis() - _LineTimeCount;
}

void Line::startTimeoutCount(){
	if(!_TimeoutFlag) {
		_TimeoutFlag = true;
		_TimeoutCount = millis();
	}
}

void Line::stopTimeoutCount(){
	_TimeoutFlag = false;
	_TimeoutCount = 0;
}

bool Line::isTimeoutLine(){
	if(!_TimeoutFlag)
		return false;
	return (millis() - _TimeoutCount > LINE_TIMEOUT);
}

bool Line::isTimeoutPass(){
	if(!_TimeoutFlag)
		return false;
	return (millis() - _TimeoutCount > CROSS_TIMEOUT);
}

/* 
// CRUISE CLASS METHODS 
*/

Cruise::Cruise(){}

void Cruise::init(){
	EBS.attach(SERVO_PIN);
	pinMode(ECHO, INPUT);
	pinMode(TRIGGER, OUTPUT);
	_EmergencyBreak = false;
	_AllowPass = false;
	this->lookForward(); // set the default position
}

bool Cruise::distance(){
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
void Cruise::stableDistance(){
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

void Cruise::lookForward(){
	EBS.write(FORWARD_GRADE);
}

void Cruise::lookLeft(){
	EBS.write(LEFT_GRADE);
}

void Cruise::lookRight(){
	EBS.write(RIGHT_GRADE);
}
void Cruise::customPosition(unsigned int pos){
	EBS.write(pos);
}

void Cruise::setPass(bool val) {
	_AllowPass = val;
}

void Cruise::setBack(bool val) {
	_AllowBack = val;
}

void Cruise::setEmergency(bool val) {
	_EmergencyBreak = val;
}

float Cruise::getDistance(){
	this->stableDistance();  //update the value
	return _Distance;
}

bool Cruise::getPass(){
	return _AllowPass;
}

bool Cruise::getBack(){
	return _AllowBack;
}

bool Cruise::getEmergency(){
	return _EmergencyBreak;
}

/* 
// LIGHT CLASS METHODS 
*/

Light::Light(){}

void Light::init(){
	pinMode(LED, OUTPUT);
}

// use these functions in a loop with the engine phase variable as breaking condition
void Light::leftLight(){
	_Status = true;
	digitalWrite(LED, HIGH);
}

void Light::offLight(){
	_Status = false;
	digitalWrite(LED, LOW);
}

bool Light::isLightOn(){ return _Status; }

/* 
// ENGINE CLASS METHODS
*/

Engine::Engine(){
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

void Engine::start(){
	_Status = 'F';
	analogWrite(R_ENGINE_POWER, _RightSpeed);
	analogWrite(L_ENGINE_POWER, _LeftSpeed);
}

void Engine::forward(){
	digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
	digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
	digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
	digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
}

void Engine::left(){
	digitalWrite(R_FRONT_ENGINE_CONTROL, HIGH);
	digitalWrite(L_FRONT_ENGINE_CONTROL, LOW);
	digitalWrite(R_REAR_ENGINE_CONTROL, LOW);
	digitalWrite(L_REAR_ENGINE_CONTROL, HIGH);
}

void Engine::right(){
	digitalWrite(R_FRONT_ENGINE_CONTROL, LOW);
	digitalWrite(L_FRONT_ENGINE_CONTROL, HIGH);
	digitalWrite(R_REAR_ENGINE_CONTROL, HIGH);
	digitalWrite(L_REAR_ENGINE_CONTROL, LOW);
}

void Engine::shutDown(){
	_RightSpeed = 0;
	_LeftSpeed = 0;
	analogWrite(R_ENGINE_POWER, 0);
	analogWrite(L_ENGINE_POWER, 0);
}

void Engine::setSpeed(uint8_t genSpeed) {
	_RightSpeed = genSpeed;
	_LeftSpeed = genSpeed;
	analogWrite(R_ENGINE_POWER, _RightSpeed);
	analogWrite(L_ENGINE_POWER, _LeftSpeed);
}

void Engine::setRightSpeed(uint8_t rightSpeed) {
	_RightSpeed = rightSpeed;
	analogWrite(R_ENGINE_POWER, _RightSpeed);
}

void Engine::setLeftSpeed(uint8_t leftSpeed) {
	_LeftSpeed = leftSpeed;
	analogWrite(L_ENGINE_POWER, _LeftSpeed);
}

void Engine::pass(){
	this->start(); // re-start engines if stopped
	this->setLeftSpeed(REDUCTED_SPEED);
	this->setRightSpeed(INCREASED_SPEED);	
	delay(650); //time to cross the path
}

void Engine::back(){
	this->start(); // re-start engines if stopped
	this->setRightSpeed(BACK_REDUCED_SPEED);
	this->setLeftSpeed(BACK_INCREASED_SPEED);	
	delay(650); //time to cross the path
}

uint8_t Engine::getRightSpeed(){ return _RightSpeed; }

uint8_t Engine::getLeftSpeed(){ return _LeftSpeed; }

char Engine::getStatus(){ return _Status; }

void Engine::setStatus(char status) { _Status = status; }