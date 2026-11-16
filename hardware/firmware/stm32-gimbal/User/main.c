
#include "stm32f10x.h"                  
#include <stdio.h>
#include "Delay.h"
#include "usart2.h"
#include "flame_sensor.h"

/*---------X axis pulse interface------------*/
#define X_EN_PORT   GPIOA
#define X_EN_PIN    GPIO_Pin_4      // En  PA4
#define X_STP_PORT  GPIOA
#define X_STP_PIN   GPIO_Pin_5      // Stp PA5
#define X_DIR_PORT  GPIOA
#define X_DIR_PIN   GPIO_Pin_6      // Dir PA6

/*---------Y axis pulse interface------------*/
#define Y_EN_PORT   GPIOB
#define Y_EN_PIN    GPIO_Pin_0      // En  PB0
#define Y_STP_PORT  GPIOB
#define Y_STP_PIN   GPIO_Pin_1      // Stp PB1
#define Y_DIR_PORT  GPIOB
#define Y_DIR_PIN   GPIO_Pin_10     // Dir PB10

#define LED_PORT	GPIOC					 
#define LED_PIN	GPIO_Pin_13		//LED  PC13

/*---------Speed control parameters-----------
 * target_dx / target_dy from the serial protocol are signed velocity
 * commands.  K230 typically sends a "level" in the range +/-100 (or so),
 * NOT a real step rate.  We multiply the level by SPEED_SCALE inside the
 * MCU to get the actual pulse frequency in steps per second.
 *
 *   actual_steps_per_sec = |input| * SPEED_SCALE
 *
 *   With SPEED_SCALE = 50 and input = 100 -> 5000 steps/s.
 *   With SPEED_SCALE = 50 and input =  20 -> 1000 steps/s.
 *   With SPEED_SCALE = 50 and input =   1 ->   50 steps/s (still visible).
 *
 *   If everything spins too slow, raise SPEED_SCALE.
 *   If it spins too fast / stalls, lower SPEED_SCALE.
 *
 * INPUT_DEAD_ZONE : |input| <= this -> stop and release the motor.
 *                   Set to 0 if you want even input=1 to move.
 * MAX_STEPS_PER_SEC : hard upper limit on the pulse frequency, to keep
 *                     the half period >= ~25us and avoid stalling without
 *                     an acceleration ramp.
 */
#define SPEED_SCALE        80u    /* steps/s per input unit (was 50)         */
#define INPUT_DEAD_ZONE    5      /* |input| <= this -> stop (filter jitter) */
#define MAX_STEPS_PER_SEC  20000u /* clamp; keep below ~40 kHz pulses        */

/*
 * Open-loop "angle": pulse counts from reference (velocity loop only).
 * Integer degrees (approx): (steps * 360) / STEPS_PER_REV_* — tune REV to driver/mechanics.
 */
#define STEPS_PER_REV_X    3200u
#define STEPS_PER_REV_Y    3200u

/* 1 = after boot test, jog toward mechanical stop then declare angle 0 (tune steps/direction!) */
#define HOME_ON_BOOT_OPEN_LOOP  0

#define HOME_MOVE_X_STEPS   6000u
#define HOME_MOVE_Y_STEPS   6000u
#define HOME_SLOW_DELAY     25u    /* same units as setMotorXStp(delay) */
#define HOME_DIR_X          1      /* 0=CW / +steps, 1=CCW / -steps (same as velocity DIR) */
#define HOME_DIR_Y          1

#define MOTION_CMD_ZERO_ANGLE   1u /* UART [0xBB,01,0,0,0,01] */
#define MOTION_CMD_OPEN_HOME    2u /* UART [0xBB,02,0,0,0,02] */

/* 1: any flame sensor fires -> zero serial speed targets (hardware uses PA0/PA1, see flame_sensor.c) */
#define FLAME_STOP_MOTORS_ON_DETECT  0

static volatile int32_t s_angle_steps_x;
static volatile int32_t s_angle_steps_y;

void GPIO_INIT(void);
void setMotorXEn(uint8_t enable);
void setMotorXDir(uint8_t dir);
void setMotorXStp(uint32_t steps,uint32_t delay);
void setMotorYEn(uint8_t enable);
void setMotorYDir(uint8_t dir);
void setMotorYStp(uint32_t steps,uint32_t delay);
void led_on(void);
void led_off(void);
void Motor_SelfTest(void);
static void apply_speed_X(int16_t speed);
static void apply_speed_Y(int16_t speed);
static void sync_stop_velocity(void);
static void software_zero_angle(void);
static void open_loop_home_reference(void);
static void power_on_angle_reference(void);
static void process_motion_uart_cmd(void);

static void setMotorPulse(GPIO_TypeDef* stp_port, uint16_t stp_pin, uint32_t steps, uint32_t delay)
{
	uint32_t i;
	if(delay == 0)	delay=1;
	
	for(i=0;i<steps;i++)
	{
		GPIO_SetBits(stp_port, stp_pin);
		Delay_us(5*delay);
		GPIO_ResetBits(stp_port, stp_pin);
		Delay_us(5*delay);
	}
}

/* Send a printable "dx=.. dy=.." line back over USART2 for debugging. */
static void debug_print_speed(int16_t sx, int16_t sy)
{
	char buf[96];
	int  n;
	/* sprintf is OK on F1 with newlib-nano; format a short line. */
	n = sprintf(buf, "dx=%d dy=%d ax=%ld ay=%ld\r\n", sx, sy,
	            (long)((s_angle_steps_x * 360) / (int32_t)STEPS_PER_REV_X),
	            (long)((s_angle_steps_y * 360) / (int32_t)STEPS_PER_REV_Y));
	if (n > 0) USART2_SendArray((uint8_t*)buf, (uint16_t)n);
}

int main(void)
{
	int16_t prev_dx = 0, prev_dy = 0;
	uint8_t first   = 1;

	GPIO_INIT();
	USART2_Init(115200);

	Delay_ms(1000);

	/* ===== STEP 1: Y-only power-on self test =====
	 * Independent of K230 / serial input. If Y does NOT visibly turn
	 * during this block, the problem is on the Y side of the hardware
	 * (PB1 wiring, Y driver Com / 24V / EN polarity), not in the loop
	 * below. If Y DOES turn here but stays still later, the problem is
	 * in the data flow (K230 not sending dy, or dy stuck in dead zone).
	 */
	led_on();
	setMotorYEn(0);          /* enable Y */
	setMotorYDir(0);         /* CW */
	setMotorYStp(2000, 15);  /* ~2000 pulses, half period 75us -> ~13kHz */
	setMotorYDir(1);         /* CCW back */
	setMotorYStp(2000, 15);
	setMotorYEn(1);          /* release */
	led_off();
	Delay_ms(500);

	power_on_angle_reference();

	USART2_SendArray((uint8_t*)"BOOT OK, entering serial mode\r\n", 31);

	/* ===== STEP 2: Serial-driven SPEED control =====
	 * Velocity: [0xAA, dx_H, dx_L, dy_H, dy_L, Sum]  big-endian int16
	 * Motion:   [0xBB, cmd, 0, 0, 0, Sum]  Sum = cmd & 0xFF
	 *   cmd 1 -> software angle reset (no motion)
	 *   cmd 2 -> open-loop homing jog then angle 0 (see HOME_* macros)
	 */
	while (1)
	{
		int16_t sx, sy;

		process_motion_uart_cmd();

#if FLAME_STOP_MOTORS_ON_DETECT
		if (FlameSensor_Any())
			sync_stop_velocity();
#endif

		__disable_irq();
		sx = target_dx;
		sy = target_dy;
		__enable_irq();

		/* Echo only on change (or first pass) so we don't flood the UART
		 * during high-frequency pulse output. */
		if (first || sx != prev_dx || sy != prev_dy)
		{
			debug_print_speed(sx, sy);
			prev_dx = sx;
			prev_dy = sy;
			first   = 0;
		}

		apply_speed_X(sx);
		apply_speed_Y(sy);

		if (sx == 0 && sy == 0)
		{
			led_off();
			Delay_ms(1);
		}
		else
		{
			led_on();
		}
	}
}

void GPIO_INIT(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/*-----------Enable GPIO clocks------------*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC, ENABLE);
	
	/*-----------Configure X-axis EN/STP/DIR as output------------*/
	GPIO_InitStructure.GPIO_Pin = X_EN_PIN | X_STP_PIN | X_DIR_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(X_EN_PORT, &GPIO_InitStructure);

	GPIO_SetBits(X_EN_PORT, X_EN_PIN);
	GPIO_ResetBits(X_DIR_PORT, X_DIR_PIN);
	GPIO_ResetBits(X_STP_PORT, X_STP_PIN);

	/*-----------Configure Y-axis EN/STP/DIR as output------------*/
	GPIO_InitStructure.GPIO_Pin = Y_EN_PIN | Y_STP_PIN | Y_DIR_PIN;
	GPIO_Init(Y_EN_PORT, &GPIO_InitStructure);

	GPIO_SetBits(Y_EN_PORT, Y_EN_PIN);
	GPIO_ResetBits(Y_DIR_PORT, Y_DIR_PIN);
	GPIO_ResetBits(Y_STP_PORT, Y_STP_PIN);
	
	/*-----------???? LED ???????------------*/
	GPIO_InitStructure.GPIO_Pin = LED_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_PORT, &GPIO_InitStructure);
	GPIO_SetBits(LED_PORT,LED_PIN);					//LED ????

	FlameSensor_Init();
}

/*?????????  
	enable = 0 ?????		
	enable = 1 ?????
*/
void setMotorXEn(uint8_t enable)
{
	GPIO_WriteBit(X_EN_PORT, X_EN_PIN, (BitAction)enable);
}

/*??????????	
  dir = 0 ?????		
	dir = 1 ????*/
void setMotorXDir(uint8_t dir)
{
	GPIO_WriteBit(X_DIR_PORT, X_DIR_PIN, (BitAction)dir);
}

/*??????????	
  steps ??????????		
	delay ??????????????????
	????????????????160KHz
*/
void setMotorXStp(uint32_t steps,uint32_t delay)
{
	setMotorPulse(X_STP_PORT, X_STP_PIN, steps, delay);
}

void setMotorYEn(uint8_t enable)
{
	GPIO_WriteBit(Y_EN_PORT, Y_EN_PIN, (BitAction)enable);
}

void setMotorYDir(uint8_t dir)
{
	GPIO_WriteBit(Y_DIR_PORT, Y_DIR_PIN, (BitAction)dir);
}

void setMotorYStp(uint32_t steps,uint32_t delay)
{
	setMotorPulse(Y_STP_PORT, Y_STP_PIN, steps, delay);
}

void led_on(void)
{
	GPIO_ResetBits(LED_PORT,LED_PIN);				//LED ????
}

void led_off(void)
{
	GPIO_SetBits(LED_PORT,LED_PIN);					//LED ????
}

/*
 * Motor self-test:
 *   spin X axis forward then backward, then Y axis forward then backward.
 *   Use small step counts so the carriage does not crash into endstops.
 *   Step count = 800 assumes driver is set to 800 steps/rev (~1 turn).
 *   delay = 20 -> pulse half-period = 5 * 20 = 100us -> ~5kHz, safe and slow.
 */
void Motor_SelfTest(void)
{
	led_on();

	/* ---- X axis: forward 1 turn -> backward 1 turn ---- */
	setMotorXEn(0);
	setMotorXDir(0);
	setMotorXStp(800, 20);
	Delay_ms(300);

	setMotorXDir(1);
	setMotorXStp(800, 20);
	Delay_ms(300);
	setMotorXEn(1);

	Delay_ms(500);

	/* ---- Y axis: forward 1 turn -> backward 1 turn ---- */
	setMotorYEn(0);
	setMotorYDir(0);
	setMotorYStp(800, 20);
	Delay_ms(300);

	setMotorYDir(1);
	setMotorYStp(800, 20);
	Delay_ms(300);
	setMotorYEn(1);

	led_off();
}

/*
 * Convert an input "level" magnitude (e.g. 0..100) into the half-period
 * in microseconds for one pulse edge, applying SPEED_SCALE and clamping.
 * Returns 0 when the input is in the dead zone (caller should release motor).
 */
static uint32_t speed_to_half_us(uint32_t mag_input)
{
	uint32_t steps_per_sec;
	uint32_t half_us;

	if (mag_input <= INPUT_DEAD_ZONE) return 0;

	steps_per_sec = mag_input * SPEED_SCALE;
	if (steps_per_sec > MAX_STEPS_PER_SEC) steps_per_sec = MAX_STEPS_PER_SEC;
	if (steps_per_sec == 0)               return 0;

	half_us = 500000UL / steps_per_sec;  /* 500_000us / steps_per_s */
	if (half_us < 5) half_us = 5;        /* keep pulse high >= 5us */
	return half_us;
}

/*
 * Emit ONE pulse on the X axis (or release the motor when speed=0).
 *   speed > 0 -> CW  (DIR = 0)
 *   speed < 0 -> CCW (DIR = 1)
 *   speed = 0 -> EN released, no pulse
 *
 * Caller should re-read the latest speed every iteration so direction
 * and frequency track the most recent USART command.
 */
static void apply_speed_X(int16_t speed)
{
	uint32_t mag;
	uint32_t half_us;

	if (speed == 0)
	{
		setMotorXEn(1);                  /* release X */
		return;
	}

	if (speed > 0) { setMotorXDir(0); mag = (uint32_t)speed; }
	else           { setMotorXDir(1); mag = (uint32_t)(-speed); }

	half_us = speed_to_half_us(mag);
	if (half_us == 0)                    /* below MIN_SPEED -> treat as 0 */
	{
		setMotorXEn(1);
		return;
	}

	setMotorXEn(0);                      /* keep enabled */
	GPIO_SetBits  (X_STP_PORT, X_STP_PIN);
	Delay_us(half_us);
	GPIO_ResetBits(X_STP_PORT, X_STP_PIN);
	Delay_us(half_us);

	if (speed > 0) s_angle_steps_x++;
	else           s_angle_steps_x--;
}

/*
 * Y axis pulse generator - mirror image of apply_speed_X.
 * Uses Y_DIR_PORT / Y_DIR_PIN (PB10), Y_EN_PORT / Y_EN_PIN (PB0)
 * and Y_STP_PORT / Y_STP_PIN (PB1). Identical logic to X axis.
 */
static void apply_speed_Y(int16_t speed)
{
	uint32_t mag;
	uint32_t half_us;

	if (speed == 0)
	{
		setMotorYEn(1);                  /* release Y */
		return;
	}

	if (speed > 0) { setMotorYDir(0); mag = (uint32_t)speed; }
	else           { setMotorYDir(1); mag = (uint32_t)(-speed); }

	half_us = speed_to_half_us(mag);
	if (half_us == 0)
	{
		setMotorYEn(1);
		return;
	}

	setMotorYEn(0);                      /* keep enabled */
	GPIO_SetBits  (Y_STP_PORT, Y_STP_PIN);
	Delay_us(half_us);
	GPIO_ResetBits(Y_STP_PORT, Y_STP_PIN);
	Delay_us(half_us);

	if (speed > 0) s_angle_steps_y++;
	else           s_angle_steps_y--;
}

static void sync_stop_velocity(void)
{
	__disable_irq();
	target_dx = 0;
	target_dy = 0;
	__enable_irq();
}

static void software_zero_angle(void)
{
	sync_stop_velocity();
	s_angle_steps_x = 0;
	s_angle_steps_y = 0;
	USART2_SendArray((uint8_t*)"ANGLE_RST\r\n", 11);
}

static void open_loop_home_reference(void)
{
	sync_stop_velocity();
	USART2_SendArray((uint8_t*)"HOMING...\r\n", 11);

	setMotorXEn(0);
	setMotorXDir(HOME_DIR_X);
	setMotorXStp(HOME_MOVE_X_STEPS, HOME_SLOW_DELAY);
	setMotorXEn(1);

	setMotorYEn(0);
	setMotorYDir(HOME_DIR_Y);
	setMotorYStp(HOME_MOVE_Y_STEPS, HOME_SLOW_DELAY);
	setMotorYEn(1);

	s_angle_steps_x = 0;
	s_angle_steps_y = 0;
	USART2_SendArray((uint8_t*)"HOME_OK\r\n", 9);
}

static void power_on_angle_reference(void)
{
	s_angle_steps_x = 0;
	s_angle_steps_y = 0;
#if HOME_ON_BOOT_OPEN_LOOP
	open_loop_home_reference();
#else
	(void)0;
#endif
}

static void process_motion_uart_cmd(void)
{
	uint8_t pending;
	uint8_t code;

	__disable_irq();
	pending = motion_cmd_pending;
	code    = motion_cmd_code;
	if (pending)
		motion_cmd_pending = 0;
	__enable_irq();

	if (!pending)
		return;

	if (code == MOTION_CMD_ZERO_ANGLE)
		software_zero_angle();
	else if (code == MOTION_CMD_OPEN_HOME)
		open_loop_home_reference();
}