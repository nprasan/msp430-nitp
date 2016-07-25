#include <msp430.h> 
#include <inttypes.h>						// Import explicit _t data types
#include "lcd.h"							// Import LCD functions
#include "table.h"							// Import sine lookup table

#define POINTS	32							// Define N-point DFT

#define sin(D)	sine[D]						// Define sine using lookup table
#define cos(D)	sine[D+8]					// Define cosine using sine + 90 deg offset


volatile int16_t fx[32];					// Global input sample array
volatile uint8_t fxCount;					// Pointer to input sample array
volatile int32_t Fw[16];					// Global DFT output array
volatile uint8_t tFlag;						// Transform Operation Flag
const int16_t offset = 504;					// DC offset value of Audion IN

// Performs N-point DFT on Input Sample Array
void transform(void)
{
	uint16_t u,k;
	for (u=0; u < POINTS/2; u++)
	{
		int32_t re = 0, im = 0;
		for (k=0; k < POINTS; k++)
		{
			uint16_t index = u * k;
			index -= (index/POINTS)*POINTS;	// Angle pointer for sine/cosine lookup
			int16_t x = fx[k];
			int32_t c = cos(index);
			int32_t s = sin(index);
			re +=  x * c;
			im += -1* x * s;
        }
        re /= 1000000;						// Scaling down computed values
        im /= 1000000;
        if(re < 0)							// Get magnitude using (|re| + |im|)/4 approximation
        	re *= -1;
        if(im < 0)
        	im *= -1;
        Fw[u] = (re + im)/4;				// Store result in global DFT array
	}
}

// Displays the DFT output as 16 bars
void display(void)
{
	uint8_t i;
	for(i = 0; i < 16; i++)
	{
		if(Fw[i] > 14)
			Fw[i] = 14;						// Clip maximum output value

		if(Fw[i] > 7)						// If value > 8,
		{
			lcd_setCursor(0, i);
			lcd_write(Fw[i]-7, DATA);		// Top row shows (value-8) block
			lcd_setCursor(1, i);
			lcd_write(0xFF, DATA);			// Bottom row shows full block
		}
		else
		{
			lcd_setCursor(0, i);
			lcd_write(' ', DATA);			// Top row shows blank space
			lcd_setCursor(1, i);
			lcd_write(Fw[i], DATA);			// Bottom row shows value block
		}
	}
}

// Store 8 block patterns in CGRAM of LCD
void lcd_fill_custom()
{
    uint8_t i,j;
    i=0;j=0;
    lcd_write(0x40, CMD);					// Send data to CGRAM
    for(i=1;i<=8;i++)						// Generate 8 block patterns
    {
        for(j=8;j>i;j--)
        	lcd_write(0x00, DATA);
        for(j=i;j>0;j--)
        	lcd_write(0xFF, DATA);
    }
}

void main(void) {
    WDTCTL = WDTPW | WDTHOLD;				// Stop watchdog timer
	
	if (CALBC1_16MHZ==0xFF)					// Check if calibration constant erased
	{
		while(1);							// do not load program
	}
	DCOCTL = 0;								// Select lowest DCO settings
	BCSCTL1 = CALBC1_16MHZ;					// Set DCO to 16 MHz
	DCOCTL = CALDCO_16MHZ;

    lcd_init();								// Initialize LCD
    lcd_fill_custom();						// Fill CGRAM with custom block patterns
	lcd_setCursor(0,5);						// Print Intro Text
	lcd_print("TI-CEPD");
	lcd_setCursor(1,5);
	lcd_print("MSP 430");
	__delay_cycles(2000000);				// Delay 2 s
	lcd_write(0x01, CMD); 					// Clear screen
	delay(20);

	ADC10AE0 |= BIT0;                       // P1.0 ADC option select
	ADC10CTL1 = INCH_0 + ADC10DIV_3;        // ADC Channel -> 0 (P1.0), CLK/4
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;
	// Ref -> Vcc, S/H -> 64 clocks, Interrupt Enable

	CCR0 = 1600;							// Set ADC Sampling Rate at 10kHz
    CCTL0 |= CCIE;							// Enable Overflow Interrupt
	TACTL = TASSEL_2 + MC_1 + TACLR;		// Clock -> SMCLK, Mode -> Up Count, Timer Clear

	__bis_SR_register(GIE);					// Enable interrupts

	while(1)
	{
		if(tFlag)							// Check if data is ready for DFT
		{
			transform();					// Perform DFT
			tFlag = 0;						// Reset transform flag
			TACTL |= MC_1 + TACLR;			// Clear and Restart ADC Sampling Timer
			__bis_SR_register(GIE);			// Enable Interrupts
			display();						// Update LCD display
			__delay_cycles(1600000);		// Wait 100ms for next update
		}
	}
}

#pragma vector = TIMER0_A0_VECTOR			// CCR0 Interrupt Vector
__interrupt void CCR0_ISR(void)
{
	if(!(ADC10CTL1 & ADC10BUSY))			// Check if ADC Conversion is in progress
		ADC10CTL0 |= ENC + ADC10SC;			// Sampling and conversion start
}

#pragma vector=ADC10_VECTOR					// ADC Conversion Complete Interrupt Vector
__interrupt void ADC10_ISR (void)
{
	fx[fxCount] = (ADC10MEM - offset)<<3;	// Store current value - offset to Global Sample Array
	fxCount++;								// Increment Sample Array Pointer
	if(fxCount == POINTS)					// When sufficient samples for DFT is acquired
	{
		__bic_SR_register(GIE);				// Disable further interrupts
		TACTL &= ~MC_3;						// Turn off ADC Sampling Timer
		fxCount = 0;						// Reset Sample Array Pointer
		tFlag = 1;							// Set transform flag
	}
}
