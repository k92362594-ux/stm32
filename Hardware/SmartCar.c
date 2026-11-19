#include "stm32f10x.h"                  // Device header
#include "Motor.h"

void SmartCar_Init(void)
{
	Motor_Init();
}
/* 与参考工程（2.自动避障）相同的接口，自由避障模式用它给出参考速度。 */
void Car_SetSpeed(int8_t LeftSpeed, int8_t RightSpeed)
{
	LeftMotor_Speed(LeftSpeed);
	RightMotor_Speed(RightSpeed);
}
void Move_Forward(void)
{
	/* 循迹直行速度，与参考循迹工程一致为30；自由避障用Car_SetSpeed不受影响 */
	LeftMotor_Speed(30);
	RightMotor_Speed(30);
}
void Move_Backward(void)
{
	LeftMotor_Speed(-40);
	RightMotor_Speed(-40);
}
void Car_Stop(void)
{
	LeftMotor_Speed(0);
	RightMotor_Speed(0);
}
void Turn_Left(int8_t Speed)
{
	LeftMotor_Speed(0);
	RightMotor_Speed(Speed);
}
void Turn_Right(int8_t Speed)
{
	LeftMotor_Speed(Speed);
	RightMotor_Speed(0);
}
void Clockwise_Rotation(void)
{
	LeftMotor_Speed(45);
	RightMotor_Speed(-45);
}
void CounterClockwise_Rotation(void)
{
	LeftMotor_Speed(-45);
	RightMotor_Speed(45);
}
