MKS SERVO57D STM32脉冲接口测试例程

接线关系（接线图见 STM32脉冲控制接线图.jpg）
STM32C8T6		SERVO57D
X轴电机:
3.3V			Com
PA4				En
PA5				Stp
PA6				Dir

Y轴电机:
3.3V			Com
PB0				En
PB1				Stp
PB10			Dir


程序运行后
1. 串口接收协议包: [0xAA, dx_H, dx_L, dy_H, dy_L, Sum]
2. X轴根据 dx 控制转动，Y轴根据 dy 控制转动
3. 当偏移量在阈值内时不动作