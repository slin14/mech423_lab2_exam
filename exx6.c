#include <msp430.h> 

/**
 * main.c - ex6 extra, with Timer A and B reversed
 *
 * use Timer B to measure time from rising edge to falling edge
 * of Timer A PWM outputs
 *
 * Timer B - CONTINUOUS mode, 1 MHz
 * TB0.1   - capture both edges, with ISR | P1.4
 * TIMER0_A1_VECTOR ISR takes the time measurement
 *
 * [ex6 extra] transmit measurement over UART (upper byte, lower byte)
 * TB1.x |     DECIMAL     |       HEX       | MEASUREMENT
 * CCRx  | upperB | lowerB | upperB | lowerB | 16-bit int
 * 1000  |      3 |    232 |   0x03 |   0xE8 |    1000
 *  500  |      1 |    244 |   0x01 |   0xF4 |     500
 *
 * [ex5] Timer A in UP mode, 1 MHz     (TA1CCR0 = 2000)
 * TA1.1   - PWM 500 Hz 50% duty cycle (TA1CCR1 = 1000) | P1.2
 * TA1.2   - PWM 500 Hz 25% duty cycle (TA1CCR2 =  500) | P1.3
 */

// PARAMETERS
#define LEDOUTPUT        0b00000001
#define UART_CHAR0       'a'
#define myTA1CCR0        2000
// myTA1CCR0 = 1 MHz / PWM frequency (Hz)

// CONSTANTS
#define LOWMASK         0x0F
#define HIGHMASK        0xF0
#define UART_INT_EN     0b1

// VARIABLES (TO CHANGE)
static const int myTA1CCR1 = 1000; // = duty cycle * myTA1CCR0
static const int myTA1CCR2 = 500;  // = duty cycle * myTA1CCR0

// VARIABLES (TO BE USED)
volatile unsigned char rxByte = 0;
volatile unsigned int prevCap = 0; // volatile tells compiler the variable value can be modified at any point outside of this code
volatile unsigned int cap = 0;
volatile unsigned int measurement = 0; // time between rising and falling edge of TB0.1 input


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
    P4IES &= ~(BIT0 + BIT1); // rising edge
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

// [ex5] setup Timer A Up Mode - counts up to "myTA1CCR0"
//             TA1.x    output port
//             TA1.1    P1.2
//             TA1.2    P1.3
void setup_timerA_UP_mode() {
    P1DIR  |=  (BIT2 + BIT3); // P1.2 and P1.3 as output
    P1SEL0 |=  (BIT2 + BIT3); // select TA1.1 and TA1.2
    P1SEL1 &= ~(BIT2 + BIT3); // select TA1.1 and TA1.2  // redundant

    TA1CTL |= TASSEL__ACLK + // ACLK as clock source (8 MHz)
              MC__UP       + // Up mode
              ID__8        ; // divide input clock by 8 -> timer clk 1 MHz
    TA1CTL |= TACLR;         // clr TAR, ensure proper reset of timer divider logic

    TA1CCR0 = myTA1CCR0;     // value to count up to in UP mode
}

// [ex6] setup Timer B CONTINUOUS Mode - counts up to TxR (counter length set by CNTL)
void setup_timerB_CONT_mode() {
    // configure P1.4 as input to timerB TB0.1
    P1DIR  &= ~BIT4;
    P1SEL1 &= ~BIT4;
    P1SEL0 |=  BIT4;

    // configure Timer B
    TB0CTL   = TBSSEL__ACLK   + // ACLK as clock source (8 MHz)
               MC__CONTINUOUS + // Continuous mode
               ID__8          + // divide input clock by 8 -> timer clk 1 MHz
               TBCLR          ; // Timer B counter clear, ensure proper reset of timer divider logic
    // Note:   TBIE (overflow interrupt is NOT enabled)
}

// [ex6x] setup Timer B CONTINUOUS Mode - counts up to TxR
// Timer B only: can set counter length using TBxCTL.CNTL

/////////////////////////////////////////////////
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK

    /////////////////////////////////////////////////
    // [ex5] Timer A - 1 MHz, UP mode - counts up to "myTA1CCR0"
    setup_timerA_UP_mode();  // TA1.1 and TA1.2 on P1.2 and P1.3

    // generate PWM with "DUTY_CYCLE_TA1_1" % duty cycle on TA1.1
    TA1CCTL1 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TA1CCR1 = myTA1CCR1;

    // generate PWM with "DUTY_CYCLE_TA1_2" % duty cycle on TA1.2
    TA1CCTL2 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TA1CCR2 = myTA1CCR2;

    /////////////////////////////////////////////////
    // [ex6] Timer B - 1 MHz, CONTINUOUS mode - counts up to TBxR
    setup_timerB_CONT_mode(); // P1.4 input to timerB TB0.1

    // configure TB0.1 - capture both edges, with ISR
    TB0CCTL1 |= CM_3  + // Capture mode: 1 - both edges
                CAP   + // Capture mode: 1
                SCS   + // Capture synchronize to timer clk (recommended)
                CCIE  + // Capture/compare interrupt enable
                CCIS_0; // Capture input select: 0 - CCIxA

    // [ex6x]
    setup_UART(~UART_INT_EN);  // set up UART with UART RX interrupt disabled

    /////////////////////////////////////////////////
    _EINT();         // enable global interrupt

    while(1) {
        // [ex6x] periodically transmit TB0.1 captured value over UART
        __delay_cycles(100000);
        txUART(measurement>>8);
        txUART(measurement);
    }

    return 0;
}

// ISR for capture from TB0.1
// [ex6] overflow is NOT enabled, so this will NOT fire when TBR overflows
#pragma vector=TIMER0_B1_VECTOR
__interrupt void timerB(void)
{
    if (TB0IV & TB0IV_TBCCR1){ // TB0CCR1_CCIFG is set
        cap = TB0CCR1;
        if(!(TB0CCTL1 & CCI)){ // current output is low (it was previously high)
            // save the measurement (time now - starting time)
            measurement = cap - prevCap; // time between rising and falling edge
            // TB0CCR2 = measurement; // save to a register (trying to see it in the debugger)
        }
        else if (TB0CCTL1 & CCI) { // current output is high (it was previously low)
            prevCap = cap; // reset the measurement starting time
        }
        TB0CCTL1 &= ~CCIFG; // clear IFG
    }
}

#pragma vector = PORT4_VECTOR
__interrupt void P4_ISR()
{
    switch (P4IV) {
        case P4IV_P4IFG0: // P4.0 (SW1)
            toggle_LED_zeros(LEDOUTPUT);  // toggle the zeros in LEDOUTPUT
          //turn_ON_LED_ones(LEDOUTPUT);  // turn ON the 1's in LEDOUTPUT
            P4IFG &= ~BIT0; // clear IFG
            break;
        case P4IV_P4IFG1: // P4.1 (SW2)
            toggle_LED_ones(LEDOUTPUT);   // toggle the ones in LEDOUTPUT
          //turn_OFF_LED_ones(LEDOUTPUT); // turn OFF the 1's in LEDOUTPUT
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
