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
#define HEAD_CENTER      90.0f    /* 正前方，与参考工程相同的舵机中位 */
#define HEAD_LEFT      180.0f     /* 若实物方向相反，交换左右角度 */
#define HEAD_RIGHT      0.0f
#define STOP_DISTANCE   25.0f     /* 厘米：停车距离 */
#define HEAD_WAIT_MS   1000       /* 毫秒：等待舵机转稳 */
#define TURN_TIME_MS   1000       /* 毫秒：自由避障转向/自转时间 */
#define FREE_FORWARD_SPEED 30     /* 自由避障前进速度，参考工程为30 */
#define FREE_TURN_SPEED   40      /* 自由避障转向速度，参考工程为40 */
/* 循迹PD控制与防冲线保护 */
#define LINE_FILTER_COUNT 2       /* 传感器状态连续一致2次才采用，滤除单帧毛刺 */
#define LINE_LOST_CONFIRM 10      /* 连续丢线约50ms内保持原动作，不立即找线 */
#define LINE_LOST_STOP   400      /* 连续丢线约2s后停车，防止长时间冲出赛道 */
#define LINE_FIND_SPEED  25       /* 丢线找线速度 */
#define LINE_BASE_SPEED  20       /* PD循迹基础速度，先低速验证方向再提速 */
#define LINE_KP          8        /* 比例系数：每档偏差对应的速度差 */
#define LINE_KD          2        /* 微分系数：抑制左右摇晃，起步不宜过大 */
#define LINE_MAX_SPEED   40       /* 单轮最高速度 */
#define LINE_MIN_SPEED   0        /* 不允许反转，避免起步甩尾 */
#define LINE_MAX_ADJUST  20       /* 每次修正的最大速度差，防止微分冲击导致猛转 */

static uint8_t Mode = MODE_LINE;  /* 显示在OLED上的模式编号 */
static uint8_t Running = 0;       /* 上电先停车，按模式键后启动 */
static uint8_t ModeChanged = 0;   /* 切换后先回正、等待，再运行 */
static int8_t LastError = 0;      /* 上一次循迹偏差：负=线偏左，正=线偏右 */
static uint8_t Trapped = 0;       /* 自由避障四面被困：停车持续鸣响 */
static uint8_t LineState = 0x3A;     /* 滤波后的传感器状态，初始为直行状态 */
static uint8_t LineCandidate = 0x3A; /* 正在确认的候选状态 */
static uint8_t LineDebounce = 0;  /* 候选状态连续出现次数 */
static uint16_t LineLost = 0;     /* 连续丢线（四路全1）计数 */

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
        LastError = 0;
        Trapped = 0;
        LineLost = 0;
        LineDebounce = 0;
        LineCandidate = 0x3A;
        LineState = 0x3A;
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

static void Alert(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    WaitWithKey(100);
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}

/* 与参考工程Buzzer()同为响500ms停500ms，但可被模式键打断。 */
static uint8_t BeepWithKey(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    if (WaitWithKey(500) == 1) return 1;
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    return WaitWithKey(500);
}

/* 循迹避障：PD连续控制，替代开关式分支，避免转弯中途停车和左右摇晃。
   根据最外侧压线传感器计算偏差（负=线偏左，正=线偏右），
   左右轮速度 = 基础速度 ± (Kp*偏差 + Kd*偏差变化)。
   保留：双红外同时触发停车蜂鸣、单红外/未匹配保持原动作、
   丢线延时确认与超时停车、传感器2次滤波。 */
static void LineStep(void)
{
    uint8_t L1, L2, R1, R2, TopL, TopR;
    uint8_t State;
    int8_t Error;
    int16_t Adjust, LeftSpeed, RightSpeed;

    Get_LineWalking(&L1, &L2, &R1, &R2, &TopL, &TopR);
    State = (uint8_t)((L1 & 1) | ((L2 & 1) << 1) | ((R1 & 1) << 2) |
                      ((R2 & 1) << 3) | ((TopL & 1) << 4) | ((TopR & 1) << 5));

    if (State == LineCandidate)
    {
        if (LineDebounce < 255) LineDebounce++;
    }
    else
    {
        LineCandidate = State;
        LineDebounce = 1;
    }
    if (LineDebounce >= LINE_FILTER_COUNT)
    {
        LineState = State;
    }

    L1 = LineState & 1;
    L2 = (LineState >> 1) & 1;
    R1 = (LineState >> 2) & 1;
    R2 = (LineState >> 3) & 1;
    TopL = (LineState >> 4) & 1;
    TopR = (LineState >> 5) & 1;

    if (TopL == 0 && TopR == 0)
    {
        Car_Stop();
        BeepWithKey();
        return;
    }
    if (TopL != 1 || TopR != 1) return; /* 单红外触发：保持原动作 */

    if (L1 == 1 && L2 == 1 && R1 == 1 && R2 == 1)
    {
        /* 四路全1（丢线）：先保持，再按最近偏差方向低速找线，超时停车 */
        if (LineLost < 65535) LineLost++;
        if (LineLost < LINE_LOST_CONFIRM) return;
        if (LineLost >= LINE_LOST_STOP)
        {
            Car_Stop();
            return;
        }
        if (LastError < 0) Car_SetSpeed(0, LINE_FIND_SPEED);
        else if (LastError > 0) Car_SetSpeed(LINE_FIND_SPEED, 0);
        else Car_Stop();
        return;
    }
    LineLost = 0;

    /* 最外侧压线传感器优先，保证大弯/直角也有足够修正量 */
    if (L2 == 0) Error = -3;
    else if (R2 == 0) Error = 3;
    else if (L1 == 0 && R1 == 1) Error = -1;
    else if (R1 == 0 && L1 == 1) Error = 1;
    else if (L1 == 0 && R1 == 0) Error = 0;
    else Error = LastError;

    Adjust = LINE_KP * Error + LINE_KD * (Error - LastError);
    if (Adjust > LINE_MAX_ADJUST) Adjust = LINE_MAX_ADJUST;
    if (Adjust < -LINE_MAX_ADJUST) Adjust = -LINE_MAX_ADJUST;
    LeftSpeed = LINE_BASE_SPEED + Adjust;
    RightSpeed = LINE_BASE_SPEED - Adjust;
    if (LeftSpeed > LINE_MAX_SPEED) LeftSpeed = LINE_MAX_SPEED;
    if (LeftSpeed < LINE_MIN_SPEED) LeftSpeed = LINE_MIN_SPEED;
    if (RightSpeed > LINE_MAX_SPEED) RightSpeed = LINE_MAX_SPEED;
    if (RightSpeed < LINE_MIN_SPEED) RightSpeed = LINE_MIN_SPEED;
    Car_SetSpeed((int8_t)LeftSpeed, (int8_t)RightSpeed);
    LastError = Error;
}

/* 自由避障：
   畅通则前进；前方<25cm停车鸣响，先探测右侧(0°)，可走则右转1000ms；
   右侧不通则探测左侧(180°)，可走则左转1000ms；
   两侧均不通则逆时针自转1000ms后重测正前方；
   仍不通则再按右、左顺序各探测一次；四面被困则停车持续鸣响。 */
static void FreeStep(void)
{
    float FrontDistance, LeftDistance, RightDistance;

    if (Trapped == 1)
    {
        BeepWithKey();
        return;
    }

    FrontDistance = HCSR04_Distance();
    if (CheckModeKey() == 1) return;
    if (FrontDistance < 0.0f)
    {
        Car_Stop();
        OLED_ShowString(3, 1, "Echo error      ");
        Alert();
        return;
    }
    OLED_ShowString(3, 1, "Front:     cm   ");
    OLED_ShowNum(3, 7, (uint32_t)FrontDistance, 3);
    if (FrontDistance > STOP_DISTANCE)
    {
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 前方有障碍：停车鸣响，先探测右侧。 */
    Car_Stop();
    if (BeepWithKey() == 1) return;
    Servo_SetAngle(HEAD_RIGHT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    RightDistance = HCSR04_Distance();
    OLED_ShowString(4, 1, "R:     cm       ");
    OLED_ShowSignedNum(4, 3, (int32_t)RightDistance, 3);
    if (CheckModeKey() == 1) return;
    if (RightDistance > STOP_DISTANCE)
    {
        Servo_SetAngle(HEAD_CENTER);
        if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
        Turn_Right(FREE_TURN_SPEED);
        if (WaitWithKey(TURN_TIME_MS) == 1) return;
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 右侧不通，探测左侧。 */
    Servo_SetAngle(HEAD_LEFT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    LeftDistance = HCSR04_Distance();
    OLED_ShowString(4, 1, "L:     cm       ");
    OLED_ShowSignedNum(4, 3, (int32_t)LeftDistance, 3);
    if (CheckModeKey() == 1) return;
    if (LeftDistance > STOP_DISTANCE)
    {
        Servo_SetAngle(HEAD_CENTER);
        if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
        Turn_Left(FREE_TURN_SPEED);
        if (WaitWithKey(TURN_TIME_MS) == 1) return;
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 左右均不通：回正后逆时针自转1000ms，重测正前方。 */
    Servo_SetAngle(HEAD_CENTER);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    CounterClockwise_Rotation();
    if (WaitWithKey(TURN_TIME_MS) == 1) return;
    Car_Stop();
    FrontDistance = HCSR04_Distance();
    OLED_ShowString(3, 1, "Front:     cm   ");
    OLED_ShowSignedNum(3, 7, (int32_t)FrontDistance, 3);
    if (CheckModeKey() == 1) return;
    if (FrontDistance > STOP_DISTANCE)
    {
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 自转后仍不通：再探测一次右侧。 */
    Servo_SetAngle(HEAD_RIGHT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    RightDistance = HCSR04_Distance();
    OLED_ShowString(4, 1, "R:     cm       ");
    OLED_ShowSignedNum(4, 3, (int32_t)RightDistance, 3);
    if (CheckModeKey() == 1) return;
    if (RightDistance > STOP_DISTANCE)
    {
        Servo_SetAngle(HEAD_CENTER);
        if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
        Turn_Right(FREE_TURN_SPEED);
        if (WaitWithKey(TURN_TIME_MS) == 1) return;
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 最后探测一次左侧。 */
    Servo_SetAngle(HEAD_LEFT);
    if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
    LeftDistance = HCSR04_Distance();
    OLED_ShowString(4, 1, "L:     cm       ");
    OLED_ShowSignedNum(4, 3, (int32_t)LeftDistance, 3);
    if (CheckModeKey() == 1) return;
    if (LeftDistance > STOP_DISTANCE)
    {
        Servo_SetAngle(HEAD_CENTER);
        if (WaitWithKey(HEAD_WAIT_MS) == 1) return;
        Turn_Left(FREE_TURN_SPEED);
        if (WaitWithKey(TURN_TIME_MS) == 1) return;
        Car_SetSpeed(FREE_FORWARD_SPEED, FREE_FORWARD_SPEED);
        return;
    }

    /* 四面被困：停车、舵机回正、持续鸣响，直到按模式键解除。 */
    Car_Stop();
    Servo_SetAngle(HEAD_CENTER);
    OLED_ShowString(4, 1, "Trapped!        ");
    Trapped = 1;
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
