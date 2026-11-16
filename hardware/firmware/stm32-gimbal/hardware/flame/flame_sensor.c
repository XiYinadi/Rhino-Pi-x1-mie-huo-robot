/**
 * @file    flame_sensor.c
 * @brief   火焰传感器（PA0 / PA1）最小实现 —— 重建文件，见头文件说明。
 *
 * 说明：
 *   - PA0 / PA1 配置为上拉输入；火焰模块常见输出为"无火高、有火低"。
 *   - 顶层 `FLAME_STOP_MOTORS_ON_DETECT` 默认 0，即检测到火焰时不停转台，
 *     只有初始化会被调用；把该宏改成 1 才会进入"火焰即停"逻辑。
 */
#include "flame_sensor.h"

#define FLAME_PORT  GPIOA
#define FLAME_PIN_1 GPIO_Pin_0
#define FLAME_PIN_2 GPIO_Pin_1

void FlameSensor_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin  = FLAME_PIN_1 | FLAME_PIN_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   /* 上拉输入 */
	GPIO_Init(FLAME_PORT, &GPIO_InitStructure);
}

uint8_t FlameSensor_Any(void)
{
	/* 低电平视为检测到火焰（与常见火焰模块的 DO 输出一致） */
	if (GPIO_ReadInputDataBit(FLAME_PORT, FLAME_PIN_1) == Bit_RESET) {
		return 1;
	}
	if (GPIO_ReadInputDataBit(FLAME_PORT, FLAME_PIN_2) == Bit_RESET) {
		return 1;
	}
	return 0;
}
