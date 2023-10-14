#include <msp430.h> 


/**
 * main.c - ex1 for exam
 *
 * DCO (8Mz) -> SMCLK (8MHz/32 = 0.25MHz = 250kHz)
 * output SMCLK on P3.4
 */

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

/////////////////////////////////////////////////
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK
	div_32_SMCLK();  // divide SMCLK by 32 -> 250kHz
	output_P3(BIT4); // output SMCLK on P3.4

	while(1) {
		;
	}
	
	return 0;
}
