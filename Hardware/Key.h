#ifndef __KEY_H             /* 如果还没有包含过这个头文件 */
#define __KEY_H             /* 做一个标记，避免重复包含 */

#include "stm32f10x.h"      /* 提供uint8_t和STM32类型定义 */

void Key_Init(void);        /* 声明初始化函数，无参数、无返回值 */
uint8_t Key_GetNum(void);   /* 声明读键函数：返回0、1或2 */

#endif                     /* 结束防重复包含的条件 */
