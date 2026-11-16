#ifndef __USART2_H
#define __USART2_H

#include "stm32f10x.h"

// Э��������ȫ�ֱ���
extern int16_t target_dx;    // ��������X��ƫ����
extern int16_t target_dy;    // ��������Y��ƫ����
extern uint8_t packet_ready; // ���ݰ�������־λ

/* ��չ����֡ [0xBB, cmd, p1, p2, p3, Sum] �� У��ͬ 0xAA ֡ */
extern volatile uint8_t motion_cmd_pending; /* ��ѭ��ȡ 1 �κ��� 0 */
extern volatile uint8_t motion_cmd_code;    /* cmd �ֽ� */

// ��������
void USART2_Init(uint32_t baudrate);
void USART2_SendByte(uint8_t Byte);
void USART2_SendArray(uint8_t *Array, uint16_t Length);

#endif