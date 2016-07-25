#include <msp430.h>
#include <inttypes.h>

#define CMD			0
#define DATA		1

#define LCD_OUT		P2OUT
#define LCD_DIR		P2DIR
#define D4			BIT4
#define D5			BIT5
#define D6			BIT6
#define D7			BIT7
#define RS			BIT2
#define EN			BIT3

// Delay function for producing delay in 0.1 ms increments
void delay(uint8_t t)
{
	uint8_t i;
	for(i=t; i > 0; i--)
		__delay_cycles(100);
}

// Function to pulse EN pin after data is written
void pulseEN(void)
{
	LCD_OUT |= EN;
	delay(1);
	LCD_OUT &= ~EN;
	delay(1);
}

//Function to write data/command to LCD
void lcd_write(uint8_t value, uint8_t mode)
{
	if(mode == CMD)
		LCD_OUT &= ~RS;				// Set RS -> LOW for Command mode
	else
		LCD_OUT |= RS;				// Set RS -> HIGH for Data mode

	LCD_OUT = ((LCD_OUT & 0x0F) | (value & 0xF0));				// Write high nibble first
	pulseEN();
	delay(1);

	LCD_OUT = ((LCD_OUT & 0x0F) | ((value << 4) & 0xF0));		// Write low nibble next
	pulseEN();
	delay(1);
}

// Function to print a string on LCD
void lcd_print(char *s)
{
	while(*s)
	{
		lcd_write(*s, DATA);
		s++;
	}
}

void lcd_printNumber(unsigned long num)
{
	char buf[10];
	char *str = &buf[9];

	*str = '\0';

	do
	{
		unsigned long m = num;
		num /= 10;
		char c = (m - 10 * num) + '0';
		*--str = c;
	} while(num);

	lcd_print(str);
}

// Function to move cursor to desired position on LCD
void lcd_setCursor(uint8_t row, uint8_t col)
{
	const uint8_t row_offsets[] = { 0x00, 0x40};
	lcd_write(0x80 | (col + row_offsets[row]), CMD);
	delay(1);
}

// Initialize LCD
void lcd_init()
{
	P2SEL &= ~(BIT6+BIT7);
	LCD_DIR |= (D4+D5+D6+D7+RS+EN);
	LCD_OUT &= ~(D4+D5+D6+D7+RS+EN);

	delay(150);						// Wait for power up ( 15ms )
	lcd_write(0x33, CMD);			// Initialization Sequence 1
	delay(50);						// Wait ( 4.1 ms )
	lcd_write(0x32, CMD);			// Initialization Sequence 2
	delay(1);						// Wait ( 100 us )

	// All subsequent commands take 40 us to execute, except clear & cursor return (1.64 ms)

	lcd_write(0x28, CMD); 			// 4 bit mode, 2 line
	delay(1);

	lcd_write(0x0C, CMD); 			// Display ON, Cursor ON, Blink ON
	delay(1);

	lcd_write(0x01, CMD); 			// Clear screen
	delay(20);

	lcd_write(0x06, CMD); 			// Auto Increment Cursor
	delay(1);

	lcd_setCursor(0,0); 			// Goto Row 1 Column 1
}

void main(void)
{
	WDTCTL = WDTPW + WDTHOLD; 					// Stop Watchdog

	lcd_init();									// Initialize LCD
	lcd_setCursor(0,5);
	lcd_print("TI-CEPD");
	lcd_setCursor(1,5);
	lcd_print("MSP 430");
	__delay_cycles(2000000);					// Delay 2 s
	lcd_write(0x01, CMD); 						// Clear screen
	delay(20);

	ADC10AE0 |= BIT0;                         	// P1.1 ADC option select
	ADC10CTL1 = INCH_0;         				// ADC Channel -> 1 (P1.1)
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON;	// Ref -> Vcc, 64 CLK S&H

	volatile unsigned long avgLevel;
	const int offset = 504;						// Offset value of audio jack
	
	while(1)
	{
		lcd_write(0x01, CMD); 					// Clear screen
		delay(20);

		unsigned int i;
		avgLevel = 0;							// Clear average
		for(i = 0; i < 128; i++)
		{
			volatile unsigned int adcValue;
			volatile int level;
			ADC10CTL0 |= ENC + ADC10SC;			// Sampling and conversion start
			while(ADC10CTL1 & ADC10BUSY);		// Wait for conversion to end
			adcValue = ADC10MEM;				// Store value
			level = adcValue - offset;			// Subtract Vcc/2 offset of audio signal
			if(level < 0)
				level = level * -1;				// Get magnitude
			avgLevel += level;					// Add current level to sum
		}
		avgLevel = avgLevel / 256;				// Get average level (Scaled to max 16)

		lcd_setCursor(0,0);						// Goto first row
		for(i = 0; i < avgLevel; i++)
			lcd_write(0xFF, DATA);				// Print VU meter blocks

		lcd_setCursor(1,0);						// Goto second row
		for(i = 0; i < avgLevel; i++)
			lcd_write(0xFF, DATA);				// Print VU meter blocks

		__delay_cycles(100000);					// Delay 100 ms
	}
}
