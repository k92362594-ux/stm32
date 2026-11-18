#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "SmartCar.h"
#include "LineWalking.h"
#include "Buzzer.h"
#include "Servo.h"
#include "HCSR04.h"
#include "Key.h"

#define MODE_LINE        1        /* 模式1：循迹 */
#define MODE_FREE        2        /* 模式2：普通地面自由避障 */
#define HEAD_CENTER     90.0f     /* 正前方，需按支架校准 */
#define HEAD_LEFT      120.0f     /* 若实物方向相反，交换左右角度 */
#define HEAD_RIGHT      60.0f
#define STOP_DISTANCE   25.0f     /* 厘米：停车距离 */
#define HEAD_WAIT_MS   500        /* 毫秒：等待舵机转稳 */
#define TURN_TIME_MS   300        /* 毫秒：自由避障转向时间，需实车调试 */

static uint8_t Mode = MODE_LINE;  /* 显示在OLED上的模式编号 */
static uint8_t Running = 0;       /* 上电先停车，按模式键后启动 */
static uint8_t ModeChanged = 0;   /* 切换后先回正、等待，再运行 */
static int8_t LastDirection = 0;  /* -1：左，1：右，0：未知 */

static void ShowMode(void)
{
    OLED_ShowString(1, 1, "Mode:           ");
    OLED_ShowNum(1, 6, Mode, 1);
    if (Mode == MODE_LINE)
    {
        OLED_ShowString(2, 1, "Line tracking   ");
    }
    else
    {
        OLED_ShowString(2, 1, "Free avoidance  ");
    }
    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 1, "                ");
}

/* 返回1表示收到模式键，调用者应退出旧动作。 */
static uint8_t CheckModeKey(void)
{
    uint8_t KeyNum = Key_GetNum();

    if (KeyNum == 1 || KeyNum == 2)
    {
        Car_Stop();
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Mode = KeyNum;
        Running = 1;
        ModeChanged = 1;
        LastDirection = 0;
        Servo_SetAngle(HEAD_CENTER);
        ShowMode();
        return 1;
    }
    return 0;
}

/* 将长等待拆成10ms的小段，期间允许按键中断旧动作。 */
static uint8_t WaitWithKey(uint16_t TimeMs)
{
    while (TimeMs > 0)
    {
        if (CheckModeKey() == 1) return 1;
        if (TimeMs >= 10)
        {
            Delay_ms(10);
            TimeMs -= 10;
        }
        else
        {
            Delay_ms(TimeMs);
            TimeMs = 0;
        }
    }
    return CheckModeKey();
}

/* 循迹定时转弯时，同时检查前方红外和模式键。 */
static void LineTurnWait(uint16_t TimeMs)
{
    uint8_t L1, L2, R1, R2, TopL, TopR;

    while (TimeMs >= 10)
    {
        if (CheckModeKey() == 1) return;
        Get_LineWalking(&L1, &L2, &R1, &R2, &TopL, &TopR);
        if (TopL == 0 || TopR == 0)
        {
            Car_Stop();
            return;
        }
        Delay_ms(10);
        TimeMs -= 10;
    }
}

static void Alert(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    WaitWithKey(100);
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}

/* 沿用原来的四路条件；逻辑排列顺序是 L2、L1、R1、R2。 */
static void LineStep(void)
{
    uint8_t L1, L2, R1, R2, TopL, TopR;

    Get_LineWalking(&L1, &L2, &R1, &R2, &TopL, &TopR);
    if (TopL == 0 || TopR == 0)
    {
        Car_Stop();
        Alert();
        return;
    }
    if (L2 == 1 && L1 == 0 && R1 == 0 && R2 == 1)
    {
        Move_Forward();
    }
    else if (L2 == 1 && L1 == 0 && R1 == 1 && R2 == 1)
    {
        Turn_Left(30);
        LastDirection = -1;
    }
    else if (L2 == 0 && L1 == 0 && R1 == 0 && R2 == 1)
    {
        Turn_Left(40);
        LastDirection = -1;
        LineTurnWait(700);
    }
    else if (L2 == 0 && L1 == 1 && R1 == 1 && R2 == 1)
    {
        Turn_Left(50);
        LastDirection = -1;
    }
    else if (L2 == 1 && L1 == 1 && R1 == 0 && R2 == 1)
    {
        Turn_Right(30);
        LastDirection = 1;
    }
    else if (L2 == 1 && L1 == 0 && R1 == 0 && R2 == 0)
    {
        Turn_Right(40);
        LastDirection = 1;
        LineTurnWait(700);
    }
    else if (L2 == 1 && L1 == 1 && R1 == 1 && R2 == 0)
    {
        Turn_Right(50);
        LastDirection = 1;
    }
    else if (L2 == 1 && L1 == 1 && R1 == 1 && R2 == 1)
    {
        if (LastDirection < 0) Turn_Left(40);
        else if (LastDirection > 0) Turn_Right(40);
        else Car_Stop();
    }
    else
    {
        Car_Stop();
    }
}

static void FreeStep(void)
{
    uint8_t L1, L2, R1, R2, TopL, TopR;
    float FrontDistance, LeftDistance, RightDistance;

    Get_LineWalking(&L1, &L2, &R1, &R2, &TopL, &TopR);
    if (TopL == 0 || TopR == 0) Car_Stop();
    FrontDistance = HCSR04_Distance();
    if (CheckModeKey() == 1) return;
    Get_LineWalking(&L1, &L2, &R1, &R2, &TopL, &TopR);

    if (FrontDistance < 0.0f)
    {
        Car_Stop();
        OLED_ShowString(3, 1, "Echo error      ");
        Alert();
        return;
    }
    OLED_ShowString(3, 1, "Front:     cm   ");
    OLED_ShowNum(3, 7, (uint32_t)FrontDistance, 3);
    if (FrontDistance > STOP_DISTANCE && TopL == 1 && TopR == 1)
    {
        Move_Forward();
        return;
    }

    Car_Stop();
    Servo_SetAngle(HEAD_LEFT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    LeftDistance = HCSR04_Distance();
    if (CheckModeKey() == 1) return;

    Servo_SetAngle(HEAD_RIGHT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    RightDistance = HCSR04_Distance();
    if (CheckModeKey() == 1) return;

    Servo_SetAngle(HEAD_CENTER);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    OLED_ShowString(4, 1, "L:     R:       ");
    OLED_ShowSignedNum(4, 3, (int32_t)LeftDistance, 3);
    OLED_ShowSignedNum(4, 10, (int32_t)RightDistance, 3);

    if (LeftDistance < 0.0f || RightDistance < 0.0f)
    {
        Alert();
        return;
    }
    if (LeftDistance > STOP_DISTANCE && LeftDistance >= RightDistance)
    {
        Turn_Left(40);
    }
    else if (RightDistance > STOP_DISTANCE)
    {
        Turn_Right(40);
    }
    else
    {
        Alert();
        return;
    }
    WaitWithKey(TURN_TIME_MS);
    Car_Stop(); /* 下一轮重新测量转向后的正前方。 */
}

int main(void)
{
    OLED_Init();
    SmartCar_Init(); /* 已通过PWM_Init配置好电机TIM2和舵机TIM3。 */
    Car_Stop();
    LineWalking_Init();
    Buzzer_Init();
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    Key_Init();
    HCSR04_Init();
    Servo_SetAngle(HEAD_CENTER);
    Delay_ms(HEAD_WAIT_MS);
    ShowMode();
    OLED_ShowString(3, 1, "Press K1 or K2  ");

    while (1)
    {
        CheckModeKey();
        if (ModeChanged == 1)
        {
            ModeChanged = 0;
            WaitWithKey(HEAD_WAIT_MS);
            continue;
        }
        if (Running == 0)
        {
            Delay_ms(10);
            continue;
        }
        if (Mode == MODE_LINE)
        {
            LineStep();
            if (ModeChanged == 0) WaitWithKey(5);
        }
        else
        {
            FreeStep();
            if (ModeChanged == 0) WaitWithKey(70);
        }
    }
}
