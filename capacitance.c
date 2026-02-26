// ================= CAPACITANCE METER (ELEC291 SAFE VERSION) =================

#include <EFM8LB1.h>
#include <stdio.h>

#define SYSCLK 72000000L

#define RA_OHMS 1600UL
#define RB_OHMS 1600UL

#define LCD_RS P1_7
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0

#define CHARS_PER_LINE 16

unsigned char ovf0;

// ================= STARTUP =================
char _c51_external_startup (void)
{
    WDTCN = 0xDE;
    WDTCN = 0xAD;

    VDM0CN |= 0x80;
    RSTSRC  = 0x02;

    P1MDOUT |= 0b_10001111;
    P2MDOUT |= 0b_00000001;

    XBR0 = 0x00;
    XBR1 = 0x10;   // T0 on P0.0
    XBR2 = 0x40;

    return 0;
}

// ================= TIMER DELAY =================
void Timer3us(unsigned char us)
{
    unsigned char i;
    CKCON0 |= 0x40;
    TMR3RL = -72;
    TMR3 = TMR3RL;
    TMR3CN0 = 0x04;
    for(i=0;i<us;i++)
    {
        while(!(TMR3CN0 & 0x80));
        TMR3CN0 &= ~0x80;
    }
    TMR3CN0 = 0;
}

void waitms(unsigned int ms)
{
    unsigned int j;
    unsigned char k;
    for(j=0;j<ms;j++)
        for(k=0;k<4;k++)
            Timer3us(250);
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

void WriteCommand(unsigned char x)
{
    LCD_RS=0;
    LCD_byte(x);
    waitms(5);
}

void WriteData(unsigned char x)
{
    LCD_RS=1;
    LCD_byte(x);
    waitms(2);
}

void LCD_4BIT(void)
{
    LCD_E=0;
    waitms(20);
    WriteCommand(0x33);
    WriteCommand(0x33);
    WriteCommand(0x32);
    WriteCommand(0x28);
    WriteCommand(0x0C);
    WriteCommand(0x01);
    waitms(20);
}

void LCD_goto(unsigned char r, unsigned char c)
{
    WriteCommand((r==0?0x80:0xC0)+c);
}

void LCD_print(char *s)
{
    while(*s) WriteData(*s++);
}

// ================= TIMER0 COUNTER =================
void TIMER0_InitCounter(void)
{
    TR0=0;
    TF0=0;
    TMOD &= ~0x0F;
    TMOD |= 0x05;
    TH0=0;
    TL0=0;
    ovf0=0;
}

unsigned long measure_frequency_1s(void)
{
    unsigned long count;
    TH0=0;
    TL0=0;
    TF0=0;
    ovf0=0;

    TR0=1;
    waitms(1000);
    TR0=0;

    if(TF0) ovf0++;

    count = ((unsigned long)ovf0<<16) | ((unsigned long)TH0<<8) | TL0;

    return count;
}

// ================= MAIN =================
void main(void)
{
    unsigned long f_hz;
    unsigned long denom;
    unsigned int c_nf;
    unsigned int c_uf;

    char line0[17];
    char line1[17];

    TIMER0_InitCounter();
    LCD_4BIT();

    while(1)
    {
        f_hz = measure_frequency_1s();

        if(f_hz==0)
        {
            sprintf(line0,"No signal      ");
            sprintf(line1,"on P0.0        ");
        }
        else
        {
            denom = (RA_OHMS + 2UL*RB_OHMS) * f_hz;

            // ---- THIS IS THE MATH ----
            // C(nF) = 1.44e9 / ((RA+2RB)*f)
            c_nf = 1440000000UL / denom;
            c_uf = c_nf / 1000;

            sprintf(line0,"f=%lu Hz      ",f_hz);

            if(c_uf>=1)
                sprintf(line1,"C=%u uF      ",c_uf);
            else
                sprintf(line1,"C=%u nF      ",c_nf);
        }

        LCD_goto(0,0);
        LCD_print(line0);
        LCD_goto(1,0);
        LCD_print(line1);
    }
}
