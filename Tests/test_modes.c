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
static float Distances[3];
static uint8_t DistanceIndex;
static uint8_t Action;
static uint8_t LastTurn;
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
    return pin == GPIO_Pin_11 ? (Pins & 1) : ((Pins >> 1) & 1);
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
void Turn_Left(int8_t speed)
{ (void)speed; Action = 2; LastTurn = 2; MoveCount++; MovementAt = Now; }
void Turn_Right(int8_t speed)
{ (void)speed; Action = 3; LastTurn = 3; MoveCount++; MovementAt = Now; }
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
    if (DistanceIndex >= 3) return -1.0f;
    return Distances[DistanceIndex++];
}

static void ResetTest(void)
{
    Now = PressAt = ReleaseAt = 0;
    Pins = 3; PressMask = 3; KeyReleased = 1;
    Mode = MODE_LINE; Running = ModeChanged = 0; LastDirection = 0;
    Action = LastTurn = MoveCount = DisplayedMode = Alarm = ServoCount = 0;
    DistanceIndex = PatternAfterDelay = 0;
    AngleNow = HEAD_CENTER; HeadCommandAt = MovementAt = 0;
    Line[0] = 0; Line[1] = 1; Line[2] = 0; Line[3] = 1;
    Line[4] = Line[5] = 1;
    Distances[0] = 100.0f; Distances[1] = 80.0f; Distances[2] = 40.0f;
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

    ResetTest();
    LineStep();
    CHECK(Action == 1, 14);
    Line[4] = 0;
    LineStep();
    CHECK(Action == 0 && Alarm == 0, 15); /* Stop; alert has finished. */
    ResetTest();
    Line[1] = 0; PatternAfterDelay = 1;
    LineStep();
    CHECK(Action == 0 && Now <= 40, 16); /* Obstacle during 700ms turn. */
    ResetTest();
    Line[1] = 0; PressAt = 50; PressMask = 1;
    LineStep();
    CHECK(Mode == 2 && Action == 0 && Now < 700, 17);

    ResetTest();
    Mode = 2; FreeStep();
    CHECK(Action == 1, 18); /* Clear forward measurement. */
    ResetTest();
    Mode = 2; Distances[0] = -1.0f; FreeStep();
    CHECK(Action == 0 && MoveCount == 0, 19);
    ResetTest();
    Mode = 2; Distances[0] = 10.0f; FreeStep();
    CHECK(LastTurn == 2 && Action == 0 && AngleNow == HEAD_CENTER, 20);
    ResetTest();
    Mode = 2; Distances[0] = 10.0f; Distances[1] = 30.0f; FreeStep();
    CHECK(LastTurn == 3 && Action == 0, 21);
    ResetTest();
    Mode = 2; Distances[0] = 10.0f; Distances[1] = -1.0f; FreeStep();
    CHECK(Action == 0 && MoveCount == 0, 22);
    ResetTest();
    Mode = 2; Distances[0] = Distances[1] = Distances[2] = 10.0f;
    FreeStep(); CHECK(Action == 0 && MoveCount == 0, 23);

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
