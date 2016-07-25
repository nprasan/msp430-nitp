#ifndef LCD_H_
#define LCD_H_

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

void lcd_init(void);
void lcd_clear(void);
void lcd_setCursor(uint8_t, uint8_t);
void lcd_print(char *);
void lcd_printNumber(unsigned long);
void lcd_write(uint8_t, uint8_t);
void write4bits(uint8_t);
void pulseEN(void);
void delay(uint8_t);

#endif /* LCD_H_ */
