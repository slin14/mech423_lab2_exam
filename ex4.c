#include <msp430.h> 

/**
 * main.c - ex4 for exam
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

// [ex1] divide SMCLK by 32 
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

// [ex2] toggle the 0's in "display_byte" on LEDs
void toggle_LED_zeros(int display_byte) {
	P3OUT ^= (~display_byte & HIGHMASK); // toggle the 0s in upper 4 bits	
	PJOUT ^= (~display_byte & LOWMASK);  // toggle the 0s in lower 4 bits	
}

// [ex2x] toggle the 1's in "display_byte" on LEDs
void toggle_LED_ones(int display_byte) {
    P3OUT ^= (display_byte & HIGHMASK); // toggle the 1s in upper 4 bits
    PJOUT ^= (display_byte & LOWMASK);  // toggle the 1s in lower 4 bits
}

// set up buttons S1 and S2 (inputs on P4.0 and P4.1)
void setup_buttons_input() {
	P4DIR &= ~(BIT0 + BIT1);  // P4.0 and P4.1 as input
	P4REN |=  (BIT0 + BIT1);  // enable pullup
	P4OUT |=  (BIT0 + BIT1);  // enable pullup
}

// enable buttons S1 and S2 interrupt (on P4.0 and P4.1)
void enable_buttons_interrupt() {
	// on rising edge (when user lets go of button)
	P4IES &= ~(BIT0 + BIT1); // rising edge
	P4IE  |=  (BIT0 + BIT1); // enabel interrupt
}

/////////////////////////////////////////////////
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK

	setup_LEDs();
	display_LEDs(LEDOUTPUT);

	setup_buttons_input();
	enable_buttons_interrupt();

    /////////////////////////////////////////////////
    _EINT();         // enable global interrupt

	while(1) {
		;
	}
	
	return 0;
}

#pragma vector = PORT4_VECTOR
__interrupt void P4()
{
	switch (P4IV) {
		case P4IV_P4IFG0: // P4.0
	    	toggle_LED_zeros(LEDOUTPUT);  // toggle the zeros in LEDOUTPUT
			P4IFG &= ~BIT0; // clear IFG
			break;
		case P4IV_P4IFG1: // P4.1
	    	toggle_LED_ones(LEDOUTPUT);   // toggle the ones in LEDOUTPUT
			P4IFG &= ~BIT1; // clear IFG
			break;
		default: break;
	}
}
