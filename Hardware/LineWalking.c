#include "stm32f10x.h"                  // Device header

void LineWalking_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_14|GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOC,&GPIO_InitStructure);
	
	
}

void Get_LineWalking(uint8_t *L1,uint8_t *L2,uint8_t *R1,uint8_t *R2,uint8_t *Top_L,uint8_t *Top_R)
{
	*L1=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_10);
	*L2=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11);
	*R1=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12);
	*R2=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13);
	*Top_L=GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_14);
	*Top_R=GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_15);
	
	
}
