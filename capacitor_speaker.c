// ================= CAP METER + AUDIO =================
//  ~C51~

#include <EFM8LB1.h>
#include <stdio.h>

#define SYSCLK 72000000L

#define LCD_RS P1_7
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define SPKR P2_1

#define RA_OHMS 1600UL
#define RB_OHMS 1600UL



unsigned char overflow_count;

// ================= STARTUP =================
char _c51_external_startup (void)
{
	SFRPAGE = 0x00;
	WDTCN = 0xDE;
	WDTCN = 0xAD;

	VDM0CN |= 0x80;
	RSTSRC  = 0x02;

	SFRPAGE = 0x10;
	PFE0CN  = 0x20;
	SFRPAGE = 0x00;

	CLKSEL = 0x00;
	CLKSEL = 0x00;
	while ((CLKSEL & 0x80) == 0);
	CLKSEL = 0x03;
	CLKSEL = 0x03;
	while ((CLKSEL & 0x80) == 0);

	P1MDOUT |= 0b_10001111;
	P2MDOUT |= 0b_00000011;

	XBR0 = 0x00;
	XBR1 = 0x10;
	XBR2 = 0x40;

	return 0;
}

// ================= DELAY =================
void Timer3us(unsigned char us)
{
	unsigned char i;
	CKCON0|=0b_01000000;
	TMR3RL = -72;
	TMR3 = TMR3RL;
	TMR3CN0 = 0x04;
	for(i=0;i<us;i++)
	{
		while (!(TMR3CN0 & 0x80));
		TMR3CN0 &= ~(0x80);
	}
	TMR3CN0 = 0;
}

void waitms(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0;j<ms;j++)
		for(k=0;k<4;k++) Timer3us(250);
}

// ================= LCD =================
void LCD_pulse(void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte(unsigned char x)
{
	ACC=x;
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);
	ACC=x;
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData(unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand(unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT(void)
{
	LCD_E=0;
	waitms(20);
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32);
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01);
	waitms(20);
}

void LCDprint(char * string, unsigned char line)
{
	int j;
	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);
}

// ================= FREQUENCY =================
unsigned long measure_frequency_1s(void)
{
	unsigned long count;

	overflow_count=0;

	TR0=0;
	TMOD &= 0xf0;
	TMOD |= 0x05;
	TH0=0;
	TL0=0;
	TF0=0;

	TR0=1;
	waitms(1000);
	TR0=0;

	if(TF0) overflow_count++;

	count = ((unsigned long)overflow_count<<16) | ((unsigned long)TH0<<8) | TL0;
	return count;
}

// ================= CAPACITANCE =================
unsigned int freq_to_cap_uf_x100(unsigned long f_hz)
{
	unsigned long denom;
	if(f_hz==0) return 0;
	denom = (RA_OHMS + 2UL*RB_OHMS) * f_hz;
	if(denom==0) return 0;
	return (unsigned int)(144000000UL / denom);
}

// ================= SPEAKER =================
void play_tone(unsigned int freq)
{
	unsigned int i;
	unsigned int period;

	if(freq==0) return;

	period = 500000UL / freq;

	for(i=0;i<200;i++)
	{
		SPKR=1;
		Timer3us(period);
		SPKR=0;
		Timer3us(period);
	}
}

// ================= MAIN =================
void main(void)
{
	char line1[17];
	char line2[17];

	unsigned long f;
	unsigned int c_uf_x100;
	unsigned int whole;
	unsigned int decimal;

	LCD_4BIT();

	while(1)
	{
		f = measure_frequency_1s();

		c_uf_x100 = freq_to_cap_uf_x100(f);
		whole   = c_uf_x100 / 100;
		decimal = c_uf_x100 % 100;

		if(f==0)
		{
			sprintf(line1,"No signal on T0");
			sprintf(line2,"P0.0 (T0)     ");
		}
		else
		{
			sprintf(line1,"f=%lu Hz      ",f);
			sprintf(line2,"C=%u.%02u uF  ",whole,decimal);
			play_tone(f/10);   // ?? play 1/10th frequency
		}

		LCDprint(line1,1);
		LCDprint(line2,2);
	}
}
