#include "usart2.h"

static uint8_t RxBuffer[6];
static uint8_t RxIndex = 0;

int16_t target_dx = 0;
int16_t target_dy = 0;
uint8_t packet_ready = 0;

volatile uint8_t motion_cmd_pending = 0;
volatile uint8_t motion_cmd_code = 0;

void USART2_Init(uint32_t baudrate)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART2, ENABLE);
}

void USART2_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

void USART2_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i++)
	{
		USART2_SendByte(Array[i]);
	}
}

/*
 * Velocity: [0xAA, dx_H, dx_L, dy_H, dy_L, Sum]
 * Motion:    [0xBB, cmd, p1, p2, p3, Sum]  Sum=(cmd+p1+p2+p3)&0xFF; use p*=0
 */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		uint8_t res = USART_ReceiveData(USART2);

		if (RxIndex == 0)
		{
			if (res == 0xAA || res == 0xBB)
			{
				RxBuffer[0] = res;
				RxIndex = 1;
			}
		}
		else if (RxIndex < 6)
		{
			RxBuffer[RxIndex++] = res;

			if (RxIndex == 6)
			{
				uint8_t sum = (RxBuffer[1] + RxBuffer[2] + RxBuffer[3] + RxBuffer[4]) & 0xFF;
				if (sum == RxBuffer[5])
				{
					if (RxBuffer[0] == 0xAA)
					{
						target_dx = (int16_t)((RxBuffer[1] << 8) | RxBuffer[2]);
						target_dy = (int16_t)((RxBuffer[3] << 8) | RxBuffer[4]);
						packet_ready = 1;
					}
					else
					{
						motion_cmd_code = RxBuffer[1];
						motion_cmd_pending = 1;
					}
				}
				RxIndex = 0;
			}
		}
		else
		{
			RxIndex = 0;
		}

		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}
