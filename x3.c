#include <msp430.h> 


/**
 * main.c - exam 3
 */
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	
	return 0;
}
