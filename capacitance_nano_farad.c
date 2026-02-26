// CapMeterEFM8.c: Measure the frequency of a signal on pin T0 and display capacitance on LCD.
//
// Based on:
//   - FreqEFM8.c (Measure frequency on T0)  Jesus Calvino-Fraga (c) 2008-2018
//   - EFM8_LCD_4bit.c (LCD 4-bit routines)
//
// NOTE: Additions for capacitance meter are marked with:  // *** ADDED ***
//
// The next line clears the "C51 command line options:" field when compiling with CrossIDE
//  ~C51~

#include <EFM8LB1.h>
#include <stdio.h>

#define SYSCLK      72000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps

// ---------------- LCD pin mapping (from EFM8_LCD_4bit.c) ----------------
#define LCD_RS P1_7
// #define LCD_RW Px_x // Not used in this code.  Connect to GND
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define CHARS_PER_LINE 16

unsigned char overflow_count;

// *** ADDED ***: 555 timing resistors (set to YOUR values)
#define RA_OHMS 1600UL
#define RB_OHMS 1600UL

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key

	VDM0CN |= 0x80;
	RSTSRC = 0x02;

	#if (SYSCLK == 48000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif

	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif

	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output

	// *** ADDED ***: LCD pins as push-pull outputs (same idea as LCD file)
	P1MDOUT |= 0b_1000_1111; // P1.7, P1.3..P1.0 push-pull
	P2MDOUT |= 0b_0000_0001; // P2.0 push-pull

	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)
	XBR1     = 0X10; // Enable T0 on P0.0
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif

	// Configure Uart 0
	SCON0 = 0x10;
	CKCON0 |= 0b_0000_0000 ; // Timer 1 uses the system clock divided by 12.
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready

	return 0;
}

// Uses Timer3 to delay <us> micro-seconds.
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter

	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;

	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow

	TMR3CN0 = 0x04;                // Start Timer3 and clear overflow flag
	for (i = 0; i < us; i++)
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0 ;                  // Stop Timer3
}

// Uses Timer3 to delay <ms> milli-seconds.
void waitms(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for(k=0; k<4; k++) Timer3us(250);
}

// ---------------- LCD routines (from EFM8_LCD_4bit.c) ----------------
void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
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

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	waitms(20); // Power on delay
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Sets it to 4-bit mode
	WriteCommand(0x28); // 2 lines, 5x7 characters
	WriteCommand(0x0c); // Display on, cursor off
	WriteCommand(0x01); // Clear screen
	waitms(20);
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

// ---------------- Frequency measurement (based on FreqEFM8.c method) ----------------

// *** ADDED ***: Measure pulses in 1 second using Timer0 as counter on T0 (P0.0)
unsigned long measure_frequency_1s(void)
{
	unsigned long count;

	overflow_count=0;

	TR0=0;          // Stop Timer0
	TMOD &= 0xf0;   // Clear Timer0 bits
	TMOD |= 0x05;   // Timer0 mode 1 (16-bit) and C/T=1 (counter)
	TH0=0;
	TL0=0;
	TF0=0;

	TR0=1;          // Start counting pulses on T0
	waitms(1000);   // Gate time = 1 second
	TR0=0;          // Stop counting

	if(TF0) overflow_count++; // If overflow occurred (rare at low freq)

	count = ((unsigned long)overflow_count<<16) | ((unsigned long)TH0<<8) | TL0;
	return count;   // Frequency in Hz (counts per 1 second)
}

// *** ADDED ***: Convert frequency to capacitance in nF using 555 astable formula:
// f = 1.44 / ((RA+2RB)*C)  =>  C = 1.44 / ((RA+2RB)*f)
// C(nF) = 1.44e9 / ((RA+2RB)*f)
unsigned int freq_to_cap_nf(unsigned long f_hz)
{
	unsigned long denom;
	if(f_hz==0) return 0;
	denom = (RA_OHMS + 2UL*RB_OHMS) * f_hz;
	if(denom==0) return 0;
	return (unsigned int)(1440000000UL / denom);
}

void main (void)
{
	char line1[17];
	char line2[17];
	unsigned long f;
	unsigned int c_nf;
	unsigned int c_uf;

	LCD_4BIT(); // Configure the LCD

	LCDprint("Cap meter (T0)", 1, 1);
	LCDprint("Waiting signal", 2, 1);
	waitms(500);

	while(1)
	{
		f = measure_frequency_1s();          // *** ADDED ***: get frequency
		c_nf = freq_to_cap_nf(f);           // *** ADDED ***: convert to nF
		c_uf = c_nf/1000;                   // *** ADDED ***: integer uF

		if(f==0)
		{
			sprintf(line1, "No signal on T0");
			sprintf(line2, "P0.0 (T0)     ");
		}
		else
		{
			sprintf(line1, "f=%lu Hz       ", f);

			if(c_uf>=1)
				sprintf(line2, "C=%u uF        ", c_uf);
			else
				sprintf(line2, "C=%u nF        ", c_nf);
		}

		LCDprint(line1, 1, 1);
		LCDprint(line2, 2, 1);
	}
}
