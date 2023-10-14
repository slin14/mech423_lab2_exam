#include <msp430.h> 

/**
 * main.c - ex2 for exam
 *
 * Display LEDOUTPUT on LEDs
 * Blink the 0's in LEDOUTPUT
 */

// PARAMETERS
#define LEDOUTPUT 0b11010011

// CONSTANTS
#define LOWMASK  0x0F
#define HIGHMASK 0xF0

/////////////////////////////////////////////////
// FUNCTIONS

// DCO (8 MHz) -> MCLK
//             -> ACLK
//             -> SMCLK
void setup_clocks() {
	CSCTL0  = CSKEY;            // password to access CS registers
	CSCTL1 |= DCOFSEL_3;        // DCO 8 MHz
	CSCTL2  = SELM__DCOCLK +    // DCO -> MCLK 
		      SELA__DCOCLK +    // DCO -> ACLK 
			  SELS__DCOCLK ;    // DCO -> SMCLK
}

// divide SMCLK by 32 - ex1
void div_32_SMCLK() {
	CSCTL3 |= DIVS__32;
}

// output on P3
void output_P3(int bit) {
	P3DIR  |= bit;
	P3SEL1 |= bit;
	P3SEL0 |= bit;
}

// configure LED8 to LED1 as digital outputs (P3.7 to P3.4, PJ.3 to PJ.0)
void setup_LEDs() {
    P3DIR |= BIT7 + BIT6 + BIT5 + BIT4; // LED8 to LED5
    PJDIR |= BIT3 + BIT2 + BIT1 + BIT0; // LED4 to LED1
}

// display "display_byte" on LEDs
void display_LEDs(int display_byte) {
	P3OUT &= ~HIGHMASK;                 // clear LED8 to LED5
	PJOUT &= ~LOWMASK;                  // clear LED4 to LED1
	P3OUT |= (display_byte & HIGHMASK); // display_byte upper 4 bits
	PJOUT |= (display_byte & LOWMASK);  // display_byte lower 4 bits
}

/////////////////////////////////////////////////
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK

	setup_LEDs();
	display_LEDs(LEDOUTPUT);

	while(1) {
		;
	}
	
	return 0;
}
