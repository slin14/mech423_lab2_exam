#include <msp430.h> 


/**
 * main.c - ex10 for exam
 */
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	
	return 0;
}
