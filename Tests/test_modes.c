/* Run the actual key and application C code with mocked hardware on Cortex-M3. */
#define main FirmwareMain
#include "../User/main.c"
#undef main
#include "../Hardware/Key.c"

static uint32_t Now;
static uint32_t PressAt;
static uint32_t ReleaseAt;
static uint8_t PressMask;
static uint8_t Pins = 3;
static uint8_t Line[6];
static float Distances[6];
static uint8_t DistanceIndex;
static uint8_t Action;
static uint8_t LastTurn;
static int8_t LeftOut, RightOut;
static uint8_t MoveCount;
static uint8_t DisplayedMode;
static uint8_t Alarm;
static uint8_t ServoCount;
static float AngleNow;
static uint32_t HeadCommandAt;
static uint32_t MovementAt;
static uint8_t PatternAfterDelay;

void Delay_ms(uint32_t ms)
{
    Now += ms;
    if (PressAt && Now >= PressAt) Pins = PressMask;
    if (ReleaseAt && Now >= ReleaseAt) Pins = 3;
}
void Delay_us(uint32_t us) { (void)us; }
void RCC_APB2PeriphClockCmd(uint32_t clock, FunctionalState state)
{ (void)clock; (void)state; }
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{ (void)port; (void)config; }
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    return pin == GPIO_Pin_9 ? (Pins & 1) : ((Pins >> 1) & 1);
}
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pin)
{ (void)port; (void)pin; Alarm = 1; }
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pin)
{ (void)port; (void)pin; Alarm = 0; }
void OLED_Init(void) {}
void OLED_ShowString(uint8_t line, uint8_t column, char *text)
{ (void)line; (void)column; (void)text; }
void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t value, uint8_t len)
{ (void)column; (void)len; if (line == 1) DisplayedMode = (uint8_t)value; }
void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t value, uint8_t len)
{ (void)line; (void)column; (void)value; (void)len; }
void SmartCar_Init(void) {}
void LineWalking_Init(void) {}
void Buzzer_Init(void) {}
void HCSR04_Init(void) {}
void Car_Stop(void) { Action = 0; }
void Move_Forward(void) { Action = 1; MoveCount++; MovementAt = Now; }
void Car_SetSpeed(int8_t left, int8_t right)
{ LeftOut = left; RightOut = right; Action = 1; MoveCount++; MovementAt = Now; }
void Turn_Left(int8_t speed)
{ (void)speed; Action = 2; LastTurn = 2; MoveCount++; MovementAt = Now; }
void Turn_Right(int8_t speed)
{ (void)speed; Action = 3; LastTurn = 3; MoveCount++; MovementAt = Now; }
void CounterClockwise_Rotation(void) { Action = 5; }
void Servo_SetAngle(float angle)
{ AngleNow = angle; ServoCount++; HeadCommandAt = Now; }
void Get_LineWalking(uint8_t *l1, uint8_t *l2, uint8_t *r1,
                     uint8_t *r2, uint8_t *tl, uint8_t *tr)
{
    if (PatternAfterDelay && Now >= 30) Line[4] = 0;
    *l1 = Line[0]; *l2 = Line[1]; *r1 = Line[2];
    *r2 = Line[3]; *tl = Line[4]; *tr = Line[5];
}
float HCSR04_Distance(void)
{
    if (DistanceIndex >= 6) return -1.0f;
    return Distances[DistanceIndex++];
}

static void ResetTest(void)
{
    Now = PressAt = ReleaseAt = 0;
    Pins = 3; PressMask = 3; KeyReleased = 1;
    Mode = MODE_LINE; Running = ModeChanged = 0; LastError = 0;
    Trapped = 0;
    LineState = 0x3A; LineCandidate = 0x3A; LineDebounce = 0; LineLost = 0;
    LeftOut = RightOut = 0;
    Action = LastTurn = MoveCount = DisplayedMode = Alarm = ServoCount = 0;
    DistanceIndex = PatternAfterDelay = 0;
    AngleNow = HEAD_CENTER; HeadCommandAt = MovementAt = 0;
    Line[0] = 0; Line[1] = 1; Line[2] = 0; Line[3] = 1;
    Line[4] = Line[5] = 1;
    Distances[0] = 100.0f; Distances[1] = 80.0f; Distances[2] = 40.0f;
    Distances[3] = 100.0f; Distances[4] = 80.0f; Distances[5] = 40.0f;
}

#define CHECK(condition, code) do { if (!(condition)) return code; } while (0)

int TestMain(void)
{
    uint16_t i;

    ResetTest();
    Key_Init();
    CHECK(Key_GetNum() == 0, 1);
    Pins = 2; /* K1 low */
    CHECK(Key_GetNum() == 1, 2);
    for (i = 0; i < 100; i++) CHECK(Key_GetNum() == 0, 3);
    Pins = 3;
    CHECK(Key_GetNum() == 0, 4);
    Pins = 1;
    CHECK(Key_GetNum() == 2, 5);

    ResetTest();
    Pins = 2; ReleaseAt = 10;
    CHECK(Key_GetNum() == 0, 6); /* A bounce disappears during debounce. */
    ResetTest();
    Pins = 0;
    CHECK(Key_GetNum() == 0, 7);
    Pins = 2;
    CHECK(Key_GetNum() == 0, 8); /* Partial release of a double press. */
    Pins = 3; Key_GetNum(); Pins = 2;
    CHECK(Key_GetNum() == 1, 9);

    ResetTest();
    ShowMode();
    CHECK(DisplayedMode == 1 && Running == 0 && Action == 0, 10);
    Action = 1; Pins = 1;
    CHECK(CheckModeKey() == 1, 11);
    CHECK(Mode == 2 && DisplayedMode == 2 && Action == 0, 12);
    CHECK(ModeChanged == 1 && Running == 1 && AngleNow == HEAD_CENTER, 13);

    /* 传感器状态需连续2次一致才生效，故每个动作调用两次 LineStep。 */
    ResetTest();
    LineStep(); LineStep();
    CHECK(Action == 1 && LeftOut == 20 && RightOut == 20, 14); /* 直行 */
    Line[4] = 0; /* 单红外触发：保持原输出，不停车。 */
    LineStep(); LineStep();
    CHECK(Action == 1 && LeftOut == 20 && Alarm == 0, 15);
    Line[5] = 0; /* 双红外同时触发：停车，蜂鸣结束后 Alarm 复位。 */
    LineStep(); LineStep();
    CHECK(Action == 0 && Alarm == 0, 16);

    /* 左小弯：偏差-1，左轮减速右轮加速。 */
    ResetTest();
    Line[2] = 1; Line[3] = 1;
    LineStep(); LineStep();
    CHECK(LeftOut == 10 && RightOut == 30 && LastError == -1, 30);
    /* 左大弯：修正限幅，左轮停车右轮加速，不反转。 */
    ResetTest();
    Line[1] = 0;
    LineStep(); LineStep();
    CHECK(LeftOut == 0 && RightOut == 40 && LastError == -3, 31);
    /* 右小弯：偏差+1，左轮加速右轮减速。 */
    ResetTest();
    Line[0] = 1;
    LineStep(); LineStep();
    CHECK(LeftOut == 30 && RightOut == 10 && LastError == 1, 32);
    /* 无方向信息时持续丢线：停车而不是直冲。 */
    ResetTest();
    Line[0] = Line[1] = Line[2] = Line[3] = 1;
    for (i = 0; i < 20; i++) LineStep();
    CHECK(Action == 0, 33);
    /* 左偏后丢线：低速左找线，超时停车。 */
    ResetTest();
    Line[2] = 1; Line[3] = 1;
    LineStep(); LineStep();
    Line[0] = Line[1] = Line[2] = Line[3] = 1;
    for (i = 0; i < 20; i++) LineStep();
    CHECK(LeftOut == 0 && RightOut == 25, 34);
    for (i = 0; i < 450; i++) LineStep();
    CHECK(Action == 0, 35);

    ResetTest();
    Mode = 2; FreeStep();
    CHECK(Action == 1, 18); /* Clear forward measurement. */
    ResetTest();
    Mode = 2; Distances[0] = -1.0f; FreeStep();
    CHECK(Action == 0 && MoveCount == 0, 19);
    /* 前方10cm受阻，右侧80cm畅通：右转后前进。 */
    ResetTest();
    Mode = 2; Distances[0] = 10.0f; FreeStep();
    CHECK(LastTurn == 3 && Action == 1 && AngleNow == HEAD_CENTER, 20);
    /* 右侧也不通，左侧40cm畅通：左转后前进。 */
    ResetTest();
    Mode = 2; Distances[0] = 10.0f; Distances[1] = 10.0f; FreeStep();
    CHECK(LastTurn == 2 && Action == 1, 21);
    /* 左右均不通：逆时针自转后前方60cm畅通，继续前进。 */
    ResetTest();
    Mode = 2; Distances[0] = Distances[1] = Distances[2] = 10.0f;
    Distances[3] = 60.0f; FreeStep();
    CHECK(Action == 1 && MoveCount == 1 && AngleNow == HEAD_CENTER, 22);
    /* 自转后各方向仍不通：被困停车，保持报警直到按键。 */
    ResetTest();
    Mode = 2; Distances[0] = Distances[1] = Distances[2] = 10.0f;
    Distances[3] = Distances[4] = Distances[5] = 10.0f;
    FreeStep();
    CHECK(Action == 0 && MoveCount == 0 && Trapped == 1, 23);
    FreeStep();
    CHECK(Action == 0 && Trapped == 1, 24);

    /* Switch mode at several points during left/right/centre waits and turn. */
    for (i = 100; i <= 1700; i += 400)
    {
        ResetTest();
        Mode = 2; Distances[0] = 10.0f;
        PressAt = i; PressMask = 2;
        FreeStep();
        CHECK(Mode == 1 && ModeChanged == 1 && Action == 0, 24 + i);
        CHECK(AngleNow == HEAD_CENTER && DisplayedMode == 1, 25 + i);
        /* No movement was issued after the key recentred the head. */
        CHECK(MovementAt <= HeadCommandAt, 26 + i);
    }
    return 0;
}
