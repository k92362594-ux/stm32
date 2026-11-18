#include "Key.h"             /* 让函数定义与自己的声明对应 */
#include "Delay.h"           /* 使用Delay_ms消抖 */

/* 1：已松手，可接受新按下；0：本次按下已处理。 */
static uint8_t KeyReleased = 1;

void Key_Init(void)          /* 配置两个按键输入，不返回数据 */
{
    GPIO_InitTypeDef GPIO_InitStructure; /* 定义GPIO配置表 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); /* 打开GPIOA时钟 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; /* 上拉：松手时为1 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10; /* 同时选择两脚 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; /* 输入模式下不控制采样频率 */
    GPIO_Init(GPIOA, &GPIO_InitStructure); /* 把配置写入GPIOA硬件 */
    KeyReleased = 1;          /* 允许第一次按下 */
}

/* 返回0/1/2。保留20ms消抖，不再用while等待松手。 */
uint8_t Key_GetNum(void)
{
    uint8_t Key1;             /* 保存PA9电平：按下0，松开1 */
    uint8_t Key2;             /* 保存PA10电平：按下0，松开1 */

    Key1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_9);
    Key2 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10);
    if (Key1 == 1 && Key2 == 1) /* 两键都松开，&&表示并且 */
    {
        if (KeyReleased == 0)  /* 上次按下已经处理，需要确认这次松手 */
        {
            Delay_ms(20);     /* 等待机械触点抖动结束 */
            if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_9) == 1 &&
                GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10) == 1)
            {
                KeyReleased = 1; /* 确认松手，可以接收下一次按下 */
            }
        }
        return 0;              /* 松手本身不发出模式选择事件 */
    }
    if (KeyReleased == 0)       /* 长按期间，本次按下只处理一次 */
    {
        return 0;
    }
    Delay_ms(20);               /* 按下消抖，再读取确认 */
    Key1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_9);
    Key2 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10);
    if (Key1 == 0 || Key2 == 0) /* ||表示或者，任意一键按下 */
    {
        KeyReleased = 0;       /* 记住已处理，防止长按连续触发 */
    }
    if (Key1 == 0 && Key2 == 1)
    {
        return 1;              /* 仅K1按下，选择模式1 */
    }
    if (Key1 == 1 && Key2 == 0)
    {
        return 2;              /* 仅K2按下，选择模式2 */
    }
    return 0; /* 同时按两键忽略，松开两键后可重按。 */
}
