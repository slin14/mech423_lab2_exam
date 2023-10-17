#include <msp430.h> 

/**
 * main.c - ex5 for exam -> q3
 *
 * [ex5] Timer B in UP mode, 1 MHz     (TB1CCR0 = 2000)
 * TB1.1  -> PWM 500 Hz 50% duty cycle (TB1CCR1 = 1000)
 * TB1.2  -> PWM 500 Hz 25% duty cycle (TB1CCR2 =  500)
 *
 * [ex5 extra] setup_timerB_UP_mode(<select_out_port>)
 * TIMERB_LED_PORT |  0b1 |  0b0
 * TB1.1           | P3.4 | P1.6
 * TB1.2           | P3.5 | P1.7
 */

// PARAMETERS
#define LEDOUTPUT        0b00000000
#define UART_CHAR0       'a'
#define myTB1CCR0        2000
// myTB1CCR0 = 1 MHz / PWM frequency (Hz)

// CONSTANTS
#define LOWMASK         0x0F
#define HIGHMASK        0xF0
#define UART_INT_EN     0b1
#define TIMERB_LED_PORT 0b1

// VARIABLES (TO CHANGE)
static const int myTB1CCR1 = 1000; // = duty cycle * myTB1CCR0
static const int myTB1CCR2 = 0;  // = duty cycle * myTB1CCR0

// VARIABLES (TO BE USED)
volatile unsigned char rxByte = 0;

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

// [ex4x] turn ON the 1's in "display_byte" on LEDs
void turn_ON_LED_ones(int display_byte) {
    P3OUT |= (display_byte & HIGHMASK); // turn ON the 1s in upper 4 bits
    PJOUT |= (display_byte & LOWMASK);  // turn ON the 1s in lower 4 bits
}

// [ex4x] turn OFF the 1's in "display_byte" on LEDs
void turn_OFF_LED_ones(int display_byte) {
    P3OUT &= ~(display_byte & HIGHMASK); // turn OFF the 1s in upper 4 bits
    PJOUT &= ~(display_byte & LOWMASK);  // turn OFF the 1s in lower 4 bits
}

// [ex3] set up buttons S1 and S2 (inputs on P4.0 and P4.1)
void setup_buttons_input() {
    P4DIR &= ~(BIT0 + BIT1);  // P4.0 and P4.1 as input
    P4REN |=  (BIT0 + BIT1);  // enable pullup
    P4OUT |=  (BIT0 + BIT1);  // enable pullup
}

// [ex3] enable buttons S1 and S2 interrupt (on P4.0 and P4.1)
void enable_buttons_interrupt() {
    // on rising edge (when user lets go of button)
    //P4IES &= ~(BIT0 + BIT1); // rising edge
    P4IES |=  (BIT0 + BIT1); // fallingedge
    P4IE  |=  (BIT0 + BIT1); // enabel interrupt
}

// set up UART 9600 baud from 8MHz
void setup_UART(int int_en) {
    P2SEL0 &= ~(BIT0 + BIT1); // UART ports P2.0 and P2.1 // redundant
    P2SEL1 |=  (BIT0 + BIT1); // UART ports P2.0 and P2.1

    UCA0CTLW0 |= UCSWRST;                   // Put the UART in software reset
    UCA0CTLW0 |= UCSSEL__ACLK;              // Run the UART using ACLK
  //UCA0CTLW0 |= UCSSEL__SMCLK;             // Run the UART using SMCLK
    UCA0MCTLW = UCOS16 + UCBRF0 + 0x4900;   // Baud rate = 9600 from an 8 MHz clock
    UCA0BRW = 52;                           // Baud rate = 9600 from an 8 MHz clock
    UCA0CTLW0 &= ~UCSWRST;                  // release UART for operation

    if (int_en)    UCA0IE |= UCRXIE;        // Enable UART Rx interrupt
}

// transmit "txByte" over UART
void txUART(unsigned char txByte)
{
    while (!(UCA0IFG & UCTXIFG)); // wait until UART not transmitting
    UCA0TXBUF = txByte;           // transmit txByte
}

// setup Timer B Up Mode - counts up to "myTB1CCR0"
// led_port    TB1.x    output port
// 1           TB1.1    P3.4
// 1           TB1.2    P3.5
// 0           TB1.1    P1.6
// 0           TB1.2    P1.7
void setup_timerB_UP_mode(int led_port) {
    if (led_port) {
        P3DIR  |=  (BIT4 + BIT5); // P3.4 and P3.5 as output
        P3SEL0 |=  (BIT4 + BIT5); // select TB1.1 and TB1.2
        P3SEL1 &= ~(BIT4 + BIT5); // select TB1.1 and TB1.2  // redundant
    } else {
        P1DIR  |=  (BIT6 + BIT7); // P1.6 and P1.7 as output
        P1SEL0 |=  (BIT6 + BIT7); // select TB1.1 and TB1.2
        P1SEL1 &= ~(BIT6 + BIT7); // select TB1.1 and TB1.2  // redundant
    }

    TB1CTL |= TBSSEL__ACLK + // ACLK as clock source (8 MHz)
              MC__UP       + // Up mode
              ID__8        ; // divide input clock by 8 -> timer clk 1 MHz
    TB1CTL |= TBCLR;         // clr TBR, ensure proper reset of the timer divider logic

    TB1CCR0 = myTB1CCR0;     // value to count up to in UP mode
}

/////////////////////////////////////////////////
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK

    setup_LEDs();
    display_LEDs(LEDOUTPUT);

    setup_buttons_input();
    enable_buttons_interrupt();
    /////////////////////////////////////////////////
    // Timer B - 1 MHz, UP mode - counts up to "myTB1CCR0"
    setup_timerB_UP_mode(TIMERB_LED_PORT);  // TB1.1 and TB1.2 on P3.4 and P3.5
  //setup_timerB_UP_mode(~TIMERB_LED_PORT); // TB1.1 and TB1.2 on P1.6 and P1.7

    // generate PWM with "DUTY_CYCLE_TB1_1" % duty cycle on TB1.1
    TB1CCTL1 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR1 = myTB1CCR1;

    // generate PWM with "DUTY_CYCLE_TB1_2" % duty cycle on TB1.2
    TB1CCTL2 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR2 = myTB1CCR2;

    /////////////////////////////////////////////////
    _EINT();         // enable global interrupt

    while(1) {
        // periodically transmit "UART_CHAR0"
        __delay_cycles(100000);
        txUART(UART_CHAR0);
    }

    return 0;
}

#pragma vector = PORT4_VECTOR
__interrupt void P4_ISR()
{
    switch (P4IV) {
        case P4IV_P4IFG0: // P4.0 (SW1)
            //toggle_LED_zeros(LEDOUTPUT);  // toggle the zeros in LEDOUTPUT
          //turn_ON_LED_ones(LEDOUTPUT);  // turn ON the 1's in LEDOUTPUT

            // S1 increases brightness 1/8
            if (TB1CCR1 < myTB1CCR0) TB1CCR1 = TB1CCR1 + 250;

            P4IFG &= ~BIT0; // clear IFG
            break;
        case P4IV_P4IFG1: // P4.1 (SW2)
            toggle_LED_ones(LEDOUTPUT);   // toggle the ones in LEDOUTPUT
          //turn_OFF_LED_ones(LEDOUTPUT); // turn OFF the 1's in LEDOUTPUT

            if (TB1CCR1 > 0) TB1CCR1 = TB1CCR1 - 250;

            P4IFG &= ~BIT1; // clear IFG
            break;
        default: break;
    }
}

#pragma vector=USCI_A0_VECTOR
__interrupt void UCA0RX_ISR()
{
    rxByte = UCA0RXBUF; // get the received byte from UART RX buffer

    // transmit back the received byte
    txUART(rxByte);                   // UART transmit

    // transmit back rxByte + 1
    txUART(rxByte + 1);               // UART transmit

    // actions based on rxByte
    if      (rxByte == 'j') turn_ON_LED_ones(LEDOUTPUT);  // turn ON the 1's in LEDOUTPUT
    else if (rxByte == 'k') turn_OFF_LED_ones(LEDOUTPUT); // turn OFF the 1's in LEDOUTPUT

    // UART RX IFG is self clearing
}
