#include "stm32f10x.h"
#include "Delay.h"

void HCSR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);

    /* Echo经分压到约3V后接PB15，使用浮空输入。 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_InternalClockConfig(TIM1);
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 60000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    TIM_Cmd(TIM1, DISABLE);
}

/* 返回距离cm；无效回波或超时返回-1，不把它当成无障碍。 */
float HCSR04_Distance(void)
{
    uint16_t EchoTime;

    TIM_Cmd(TIM1, DISABLE);
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == SET)
    {
        return -1.0f;
    }
    TIM_SetCounter(TIM1, 0);
    TIM_Cmd(TIM1, ENABLE);
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);
    Delay_us(2);
    GPIO_SetBits(GPIOB, GPIO_Pin_14);
    Delay_us(10);
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);

    while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == RESET)
    {
        if (TIM_GetCounter(TIM1) >= 30000)
        {
            TIM_Cmd(TIM1, DISABLE);
            return -1.0f;
        }
    }
    TIM_SetCounter(TIM1, 0);
    while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == SET)
    {
        if (TIM_GetCounter(TIM1) >= 30000)
        {
            TIM_Cmd(TIM1, DISABLE);
            return -1.0f;
        }
    }
    EchoTime = TIM_GetCounter(TIM1);
    TIM_Cmd(TIM1, DISABLE);
    if (EchoTime < 116 || EchoTime > 23200)
    {
        return -1.0f;
    }
    return EchoTime * 0.017f;
}
