/**
 * @file    flame_sensor.h
 * @brief   火焰传感器（PA0 / PA1）接口声明。
 *
 * ⚠️ 重建文件：原始工程压缩包里缺少 flame_sensor.c/h，但 User/main.c 里
 *    `#include "flame_sensor.h"` 并调用了 FlameSensor_Init()。为了让工程
 *    能直接编译，这里按 main.c 的调用点与注释（"hardware uses PA0/PA1"）
 *    重建了最小实现。实际接线以硬件原理图为准；若原工程另有实现，请以其为准。
 */
#ifndef __FLAME_SENSOR_H
#define __FLAME_SENSOR_H

#include "stm32f10x.h"

/** 初始化火焰传感器输入脚（PA0 / PA1，上拉输入，低电平=检测到火焰）。 */
void FlameSensor_Init(void);

/** 任一通道检测到火焰时返回 1，否则返回 0。 */
uint8_t FlameSensor_Any(void);

#endif
