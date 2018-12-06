#ifndef _CONST_H_
#define _CONST_H_


// line sensors
#define CENTRAL_SENSOR 	4
#define RIGHT_SENSOR 	2
#define LEFT_SENSOR 	10
// engines
#define R_ENGINE_POWER 	5
#define R_FRONT_ENGINE_CONTROL 	6
#define R_REAR_ENGINE_CONTROL 	7
#define L_ENGINE_POWER 	11
#define L_FRONT_ENGINE_CONTROL 	9
#define L_REAR_ENGINE_CONTROL 	8
// cruise control
#define SERVO_PIN 	3
#define ECHO 	A4
#define TRIGGER A5
#define FORWARD_GRADE 	145
#define LEFT_GRADE 	179
#define RIGHT_GRADE 112
// light
#define LED 	12
/* car constants */
#define STABLE_SPEED 	95
#define REDUCTED_SPEED 	30
#define INCREASED_SPEED 	140
#define BACK_REDUCED_SPEED 	60
#define BACK_INCREASED_SPEED 	145
#define COUNTER_UNIT_DIMENSION	2
#define BREAKING_DISTANCE 	50 // cm
#define CROSSBACK_DISTANCE 	20 // cm
/* time constraints */
#define GOBACK_STABLE_TIME 	2500
#define NOMORE_JUST_CROSSED_TIME 	1200
#define LINE_TIMEOUT 	2000
#define CROSS_TIMEOUT 	2000

/* thread pids and informations */
#define ENGINE_T	0
#define LINE_T 		1
#define CRUISE_T 	2
#define LIGHT_T		3
#define NONE 	-1
#define THREADS_NUM 4

#endif //_CONST_H_
