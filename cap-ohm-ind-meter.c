#include "../Common/Include/stm32l051xx.h"
#include <stdio.h>

#define RA_OHMS 1670UL
#define RB_OHMS 1670UL

#define LCD_RS_PIN 0
#define LCD_E_PIN  1
#define LCD_D4_PIN 2
#define LCD_D5_PIN 3
#define LCD_D6_PIN 4
#define LCD_D7_PIN 5

#define FREQ_IN_PIN 8

#define BTN_NF_PIN 6
#define BTN_F_PIN  7
#define BTN_UF_PIN 5   // moved from PB0 to PB5 (pin 28)

#define MODE_UF 0
#define MODE_NF 1
#define MODE_F  2

#define OHM_ADC_PIN      1
#define OHM_ADC_CHANNEL  9
#define OHM_REF_OHMS     1000UL

#define TOGGLE_BTN_PIN   12

#define SCREEN_CAP 0
#define SCREEN_OHM 1
#define SCREEN_IND 2

#define IND_TOGGLE_BTN_PIN 13   // keep your existing toggle button assignment

// New inductance measurement method:
// PB4 (pin 27) = GPIO drive
// PB0 (pin 14) = ADC input at junction of L and R
#define IND_DRIVE_PIN      4
#define IND_ADC_PIN        0
#define IND_ADC_CHANNEL    8
#define IND_RSENSE_OHMS    1000UL

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
	gpio->PUPDR |=  (1U << (pin*2));
}

void gpio_set_analog(GPIO_TypeDef *gpio, int pin)
{
	gpio->MODER &= ~(3U << (pin*2));
	gpio->MODER |=  (3U << (pin*2));
	gpio->PUPDR &= ~(3U << (pin*2));
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
		if((now == 1U) && (prev == 0U)) edges++;
		prev = now;
		if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) ms++;
	}
	return edges * 10UL;
}

unsigned long cap_uf_x100(unsigned long f_hz)
{
	if(f_hz == 0) return 0;
	unsigned long long denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	return (denom == 0) ? 0 : (unsigned long)(144000000ULL / denom);
}

unsigned long cap_nf_x100(unsigned long f_hz)
{
	if(f_hz == 0) return 0;
	unsigned long long denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	return (denom == 0) ? 0 : (unsigned long)(144000000000ULL / denom);
}

unsigned long cap_f_x1000000000(unsigned long f_hz)
{
	if(f_hz == 0) return 0;
	unsigned long long denom = ((unsigned long long)RA_OHMS + 2ULL*(unsigned long long)RB_OHMS) * (unsigned long long)f_hz;
	return (denom == 0) ? 0 : (unsigned long)(1440000000ULL / denom);
}

int button_pressed(GPIO_TypeDef *gpio, int pin)
{
	if(pin_read(gpio, pin) == 0)
	{
		delay_ms(20);
		if(pin_read(gpio, pin) == 0) return 1;
	}
	return 0;
}

void wait_button_release(GPIO_TypeDef *gpio, int pin)
{
	while(pin_read(gpio, pin) == 0);
	delay_ms(20);
}

void adc_init_ohmmeter(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
	ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE;
	ADC1->CFGR2 |= ADC_CFGR2_CKMODE_0;
	if(ADC1->CR & ADC_CR_ADEN)
	{
		ADC1->CR |= ADC_CR_ADDIS;
		while(ADC1->CR & ADC_CR_ADEN);
	}
	ADC1->CFGR1 = 0;
	ADC1->SMPR |= ADC_SMPR_SMP;
	ADC1->ISR |= ADC_ISR_ADRDY;
	ADC1->CR |= ADC_CR_ADCAL;
	while(ADC1->CR & ADC_CR_ADCAL);
	ADC1->CR |= ADC_CR_ADEN;
	while((ADC1->ISR & ADC_ISR_ADRDY) == 0);
}

unsigned int adc_read_channel(unsigned int ch)
{
	ADC1->CHSELR = (1U << ch);
	ADC1->ISR |= ADC_ISR_EOC;
	ADC1->CR |= ADC_CR_ADSTART;
	while((ADC1->ISR & ADC_ISR_EOC) == 0);
	return (unsigned int)(ADC1->DR & 0x0FFF);
}

unsigned int adc_read_avg(unsigned int ch)
{
	unsigned long sum = 0;
	for(int i=0; i<32; i++)
	{
		sum += adc_read_channel(ch);
		delay_ms(1);
	}
	return (unsigned int)(sum / 32UL);
}

unsigned long measure_resistance_ohms(void)
{
	unsigned int adc = adc_read_avg(OHM_ADC_CHANNEL);
	if(adc <= 1) return 0;
	if(adc >= 4090) return 0xFFFFFFFFUL;
	unsigned long long rx = (unsigned long long)OHM_REF_OHMS * (unsigned long long)adc;
	rx /= (4095UL - (unsigned long long)adc);
	return (unsigned long)rx;
}

// RL step-response inductance measurement
// Wiring:
// PB4 -> unknown inductor -> junction -> 1k resistor -> GND
//                              |
//                              +-> PB0 ADC input
unsigned long measure_inductance_uh(void)
{
	unsigned int adc_final, adc_now, threshold;
	unsigned long t_us = 0;
	unsigned long max_us = 50000UL; // 50 ms timeout

	// Make sure current is zero first
	pin_low(GPIOB, IND_DRIVE_PIN);
	delay_ms(10);

	// Measure final steady-state ADC value
	pin_high(GPIOB, IND_DRIVE_PIN);
	delay_ms(10);
	adc_final = adc_read_avg(IND_ADC_CHANNEL);

	if(adc_final < 10)
	{
		pin_low(GPIOB, IND_DRIVE_PIN);
		return 0;
	}

	threshold = (unsigned int)(((unsigned long)adc_final * 632UL) / 1000UL);

	// Reset again before actual timing run
	pin_low(GPIOB, IND_DRIVE_PIN);
	delay_ms(10);

	// Apply step and time until ADC reaches 63.2%
	pin_high(GPIOB, IND_DRIVE_PIN);

	while(t_us < max_us)
	{
		adc_now = adc_read_channel(IND_ADC_CHANNEL);
		if(adc_now >= threshold) break;
		delay_us(1);
		t_us++;
	}

	pin_low(GPIOB, IND_DRIVE_PIN);

	if(t_us >= max_us) return 0;

	// L(uH) = R(ohms) * tau(us)
	return IND_RSENSE_OHMS * t_us;
}

int main(void)
{
	char line1[17], line2[17];
	unsigned long f, value;
	unsigned char display_mode = MODE_UF;
	unsigned char screen_mode = SCREEN_CAP;

	clock_init_16MHz();
	RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;

	gpio_set_output(GPIOA, LCD_RS_PIN);
	gpio_set_output(GPIOA, LCD_E_PIN);
	gpio_set_output(GPIOA, LCD_D4_PIN);
	gpio_set_output(GPIOA, LCD_D5_PIN);
	gpio_set_output(GPIOA, LCD_D6_PIN);
	gpio_set_output(GPIOA, LCD_D7_PIN);

	gpio_set_input(GPIOA, FREQ_IN_PIN);
	gpio_set_input_pullup(GPIOA, BTN_NF_PIN);
	gpio_set_input_pullup(GPIOA, BTN_F_PIN);
	gpio_set_input_pullup(GPIOB, BTN_UF_PIN);

	gpio_set_analog(GPIOB, OHM_ADC_PIN);
	gpio_set_input_pullup(GPIOA, TOGGLE_BTN_PIN);

	// New inductor pins
	gpio_set_input_pullup(GPIOA, IND_TOGGLE_BTN_PIN);
	gpio_set_output(GPIOB, IND_DRIVE_PIN);
	pin_low(GPIOB, IND_DRIVE_PIN);
	gpio_set_analog(GPIOB, IND_ADC_PIN);

	adc_init_ohmmeter();
	systick_init();
	lcd_init();

	while(1)
	{
		if(button_pressed(GPIOA, TOGGLE_BTN_PIN))
		{
			screen_mode = (screen_mode == SCREEN_OHM) ? SCREEN_CAP : SCREEN_OHM;
			wait_button_release(GPIOA, TOGGLE_BTN_PIN);
			lcd_cmd(0x01);
		}

		if(button_pressed(GPIOA, IND_TOGGLE_BTN_PIN))
		{
			screen_mode = (screen_mode == SCREEN_IND) ? SCREEN_CAP : SCREEN_IND;
			wait_button_release(GPIOA, IND_TOGGLE_BTN_PIN);
			lcd_cmd(0x01);
		}

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

		if(screen_mode == SCREEN_OHM)
		{
			value = measure_resistance_ohms();
			lcd_goto(1, 0);
			lcd_print("Ohmmeter PB1    ");
			lcd_goto(2, 0);
			if(value == 0xFFFFFFFFUL)
			{
				lcd_print("R=Open          ");
			}
			else if(value >= 1000000UL)
			{
				sprintf(line2, "R=%lu.%02lu Mohm", value/1000000UL, (value%1000000UL)/10000UL);
				lcd_print(line2);
			}
			else if(value >= 1000UL)
			{
				sprintf(line2, "R=%lu.%02lu kohm", value/1000UL, (value%1000UL)/10UL);
				lcd_print(line2);
			}
			else
			{
				sprintf(line2, "R=%lu ohm       ", value);
				lcd_print(line2);
			}
			delay_ms(100);
			continue;
		}

		if(screen_mode == SCREEN_IND)
		{
			unsigned long ind_uh = measure_inductance_uh();

			lcd_goto(1, 0);
			lcd_print("Inductor RL PB0 ");

			lcd_goto(2, 0);
			if(ind_uh == 0)
			{
				lcd_print("No/low L signal ");
			}
			else if(ind_uh >= 1000000UL)
			{
				sprintf(line2, "L=%lu.%02lu H    ", ind_uh/1000000UL, (ind_uh%1000000UL)/10000UL);
				lcd_print(line2);
			}
			else if(ind_uh >= 1000UL)
			{
				sprintf(line2, "L=%lu.%02lu mH   ", ind_uh/1000UL, (ind_uh%1000UL)/10UL);
				lcd_print(line2);
			}
			else
			{
				sprintf(line2, "L=%lu uH        ", ind_uh);
				lcd_print(line2);
			}

			delay_ms(100);
			continue;
		}

		f = measure_frequency_hz();
		lcd_goto(1, 0);
		if(f == 0)
		{
			lcd_print("No signal       ");
			lcd_goto(2, 0);
			lcd_print("PA8 input       ");
		}
		else
		{
			sprintf(line1, "f=%lu Hz        ", f);
			lcd_print(line1);
			lcd_goto(2, 0);

			if(display_mode == MODE_UF)
			{
				value = cap_uf_x100(f);
				sprintf(line2, "C=%lu.%02lu uF   ", value/100, value%100);
			}
			else if(display_mode == MODE_NF)
			{
				value = cap_nf_x100(f);
				sprintf(line2, "C=%lu.%02lu nF   ", value/100, value%100);
			}
			else
			{
				value = cap_f_x1000000000(f);
				sprintf(line2, "C=%lu.%09luF", value/1000000000UL, value%1000000000UL);
			}
			lcd_print(line2);
		}
		delay_ms(100);
	}
}
