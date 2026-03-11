#include "../Common/Include/stm32l051xx.h"
#include <stdio.h>

#define RA_OHMS 1670UL
#define RB_OHMS 1670UL

// LCD pins on GPIOA
#define LCD_RS_PIN 0   // PA0
#define LCD_E_PIN  1   // PA1
#define LCD_D4_PIN 2   // PA2
#define LCD_D5_PIN 3   // PA3
#define LCD_D6_PIN 4   // PA4
#define LCD_D7_PIN 5   // PA5

// 555 output input
#define FREQ_IN_PIN 8  // PA8

// Pushbuttons
#define BTN_NF_PIN 6   // PA6
#define BTN_F_PIN  7   // PA7
#define BTN_UF_PIN 0   // PB0

#define MODE_UF 0
#define MODE_NF 1
#define MODE_F  2

void delay_cycles(volatile unsigned int d)
{
	while(d--);
}

void delay_us(unsigned int us)
{
	while(us--)
	{
		delay_cycles(4);
	}
}

void delay_ms(unsigned int ms)
{
	while(ms--)
	{
		delay_us(1000);
	}
}

void clock_init_16MHz(void)
{
	RCC->CR |= RCC_CR_HSION;
	while((RCC->CR & RCC_CR_HSIRDY)==0);

	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_HSI;
	while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}

void gpio_set_output(GPIO_TypeDef *gpio, int pin)
{
	gpio->MODER &= ~(3U << (pin*2));
	gpio->MODER |=  (1U << (pin*2));
	gpio->OTYPER &= ~(1U << pin);
	gpio->OSPEEDR |= (3U << (pin*2));
	gpio->PUPDR &= ~(3U << (pin*2));
}

void gpio_set_input(GPIO_TypeDef *gpio, int pin)
{
	gpio->MODER &= ~(3U << (pin*2));
	gpio->PUPDR &= ~(3U << (pin*2));
}

void gpio_set_input_pullup(GPIO_TypeDef *gpio, int pin)
{
	gpio->MODER &= ~(3U << (pin*2));
	gpio->PUPDR &= ~(3U << (pin*2));
	gpio->PUPDR |=  (1U << (pin*2));   // 01 = pull-up
}

void pin_high(GPIO_TypeDef *gpio, int pin)
{
	gpio->BSRR = (1U << pin);
}

void pin_low(GPIO_TypeDef *gpio, int pin)
{
	gpio->BSRR = (1U << (pin + 16));
}

int pin_read(GPIO_TypeDef *gpio, int pin)
{
	return (gpio->IDR >> pin) & 1U;
}

void lcd_pulse(void)
{
	pin_high(GPIOA, LCD_E_PIN);
	delay_us(2);
	pin_low(GPIOA, LCD_E_PIN);
	delay_us(50);
}

void lcd_write_nibble(unsigned char nib)
{
	if(nib & 0x01) pin_high(GPIOA, LCD_D4_PIN); else pin_low(GPIOA, LCD_D4_PIN);
	if(nib & 0x02) pin_high(GPIOA, LCD_D5_PIN); else pin_low(GPIOA, LCD_D5_PIN);
	if(nib & 0x04) pin_high(GPIOA, LCD_D6_PIN); else pin_low(GPIOA, LCD_D6_PIN);
	if(nib & 0x08) pin_high(GPIOA, LCD_D7_PIN); else pin_low(GPIOA, LCD_D7_PIN);
	lcd_pulse();
}

void lcd_write_byte(unsigned char rs, unsigned char val)
{
	if(rs) pin_high(GPIOA, LCD_RS_PIN);
	else   pin_low(GPIOA, LCD_RS_PIN);

	lcd_write_nibble((val >> 4) & 0x0F);
	lcd_write_nibble(val & 0x0F);
}

void lcd_cmd(unsigned char cmd)
{
	lcd_write_byte(0, cmd);
	delay_ms(2);
}

void lcd_data(unsigned char data)
{
	lcd_write_byte(1, data);
	delay_ms(1);
}

void lcd_init(void)
{
	delay_ms(20);

	pin_low(GPIOA, LCD_RS_PIN);
	pin_low(GPIOA, LCD_E_PIN);

	lcd_write_nibble(0x03);
	delay_ms(5);
	lcd_write_nibble(0x03);
	delay_ms(5);
	lcd_write_nibble(0x03);
	delay_ms(5);
	lcd_write_nibble(0x02);
	delay_ms(5);

	lcd_cmd(0x28);
	lcd_cmd(0x0C);
	lcd_cmd(0x01);
	lcd_cmd(0x06);
}

void lcd_goto(unsigned char line, unsigned char col)
{
	if(line==1) lcd_cmd(0x80 + col);
	else        lcd_cmd(0xC0 + col);
}

void lcd_print(char *s)
{
	while(*s) lcd_data(*s++);
}

void systick_init(void)
{
	SysTick->LOAD = 16000 - 1;
	SysTick->VAL  = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

unsigned long measure_frequency_hz(void)
{
	unsigned long edges = 0;
	unsigned int ms = 0;
	unsigned int prev, now;

	prev = pin_read(GPIOA, FREQ_IN_PIN);

	SysTick->VAL = 0;
	while(ms < 100)
	{
		now = pin_read(GPIOA, FREQ_IN_PIN);

		if((now == 1U) && (prev == 0U))
			edges++;

		prev = now;

		if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
			ms++;
	}

	return edges * 10UL;
}

unsigned long cap_uf_x100(unsigned long f_hz)
{
	unsigned long long denom;

	if(f_hz == 0) return 0;

	denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	if(denom == 0) return 0;

	return (unsigned long)(144000000ULL / denom);
}

unsigned long cap_nf_x100(unsigned long f_hz)
{
	unsigned long long denom;

	if(f_hz == 0) return 0;

	denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	if(denom == 0) return 0;

	return (unsigned long)(144000000000ULL / denom);
}

unsigned long cap_f_x1000000000(unsigned long f_hz)
{
	unsigned long long denom;

	if(f_hz == 0) return 0;

	denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	if(denom == 0) return 0;

	return (unsigned long)(1440000000ULL / denom);
}

int button_pressed(GPIO_TypeDef *gpio, int pin)
{
	if(pin_read(gpio, pin) == 0)
	{
		delay_ms(20);
		if(pin_read(gpio, pin) == 0)
			return 1;
	}
	return 0;
}

void wait_button_release(GPIO_TypeDef *gpio, int pin)
{
	while(pin_read(gpio, pin) == 0);
	delay_ms(20);
}

int main(void)
{
	char line1[17];
	char line2[17];

	unsigned long f;
	unsigned long value;
	unsigned long whole;
	unsigned long decimal;

	unsigned char display_mode = MODE_UF;

	clock_init_16MHz();

	RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;

	// LCD outputs
	gpio_set_output(GPIOA, LCD_RS_PIN);
	gpio_set_output(GPIOA, LCD_E_PIN);
	gpio_set_output(GPIOA, LCD_D4_PIN);
	gpio_set_output(GPIOA, LCD_D5_PIN);
	gpio_set_output(GPIOA, LCD_D6_PIN);
	gpio_set_output(GPIOA, LCD_D7_PIN);

	// 555 input
	gpio_set_input(GPIOA, FREQ_IN_PIN);

	// Pushbuttons with internal pull-ups
	gpio_set_input_pullup(GPIOA, BTN_NF_PIN);
	gpio_set_input_pullup(GPIOA, BTN_F_PIN);
	gpio_set_input_pullup(GPIOB, BTN_UF_PIN);

	systick_init();
	lcd_init();

	while(1)
	{
		if(button_pressed(GPIOA, BTN_NF_PIN))
		{
			display_mode = MODE_NF;
			wait_button_release(GPIOA, BTN_NF_PIN);
		}

		if(button_pressed(GPIOA, BTN_F_PIN))
		{
			display_mode = MODE_F;
			wait_button_release(GPIOA, BTN_F_PIN);
		}

		if(button_pressed(GPIOB, BTN_UF_PIN))
		{
			display_mode = MODE_UF;
			wait_button_release(GPIOB, BTN_UF_PIN);
		}

		f = measure_frequency_hz();

		lcd_goto(1, 0);
		if(f == 0)
		{
			sprintf(line1, "No signal       ");
			lcd_print(line1);

			lcd_goto(2, 0);
			sprintf(line2, "PA8 input       ");
			lcd_print(line2);
		}
		else
		{
			sprintf(line1, "f=%lu Hz        ", f);
			lcd_print(line1);

			lcd_goto(2, 0);

			if(display_mode == MODE_UF)
			{
				value = cap_uf_x100(f);
				whole = value / 100;
				decimal = value % 100;
				sprintf(line2, "C=%lu.%02lu uF   ", whole, decimal);
			}
			else if(display_mode == MODE_NF)
			{
				value = cap_nf_x100(f);
				whole = value / 100;
				decimal = value % 100;
				sprintf(line2, "C=%lu.%02lu nF   ", whole, decimal);
			}
			else
			{
				value = cap_f_x1000000000(f);
				whole = value / 1000000000UL;
				decimal = value % 1000000000UL;
				sprintf(line2, "C=%lu.%09luF  ", whole, decimal);
			}

			lcd_print(line2);
		}

		delay_ms(100);
	}
}
