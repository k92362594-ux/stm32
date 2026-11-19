#ifndef __SMARTCAR_H
#define __SMARTCAR_H
#include "stdint.h"
void SmartCar_Init(void);
void Car_SetSpeed(int8_t LeftSpeed, int8_t RightSpeed);
void Move_Forward(void);
void Move_Backward(void);
void Car_Stop(void);
void Turn_Left(int8_t Speed);
void Turn_Right(int8_t Speed);
void Clockwise_Rotation(void);
void CounterClockwise_Rotation(void);
#endif
