#include <msp430.h> 

/**
 * main.c - final proj 1
 *
 * ADC read Ax, Ay, Az. Transmit over UART every 40 ms (25 Hz)
 * simple gesture detection
 * blink light to dark for a brief amount of time for right acceleration
 * blink dark to light for a brief amount of time for left acceleration
 * otherwise dark
 *
 */

// PARAMETERS
#define LEDOUTPUT        0b00000001
#define UART_CHAR0       'a'
// myTB1CCR0 = 1 MHz / PWM frequency (Hz)
#define myTB1CCR0        2000
// milliseconds per timer interrupt (freq = 1000/TIMER_MILLISEC Hz) 
#define TIMER_MILLISEC   40
#define BUF_SIZE         50
#define MSG_SIZE         5

// CONSTANTS
#define LOWMASK         0x0F
#define HIGHMASK        0xF0
#define UART_INT_EN     0b1
#define TIMERB_LED_PORT 0b1
#define X_CH            ADC10INCH_12
#define Y_CH            ADC10INCH_13
#define Z_CH            ADC10INCH_14
#define NTC_CH          ADC10INCH_4
#define NUM_LEDS        8

// VARIABLES (PARAMETERS)
static const int myTB1CCR1 = 1000; // = duty cycle * myTB1CCR0
static const int myTB1CCR2 = 500;  // = duty cycle * myTB1CCR0
static const int myTA0CCR0 = 500;  // = TIMER_MILLISEC * 1000 - 1;
volatile unsigned int blinkFreq1 = 500;  // timer ISR freq for blinking arrows
volatile unsigned int blinkFreq2 = 300;  // timer ISR freq for blinking finger
static const int COUNT_MAX = 5500;  // count expiry [f] blink length after accel is detected
static const int TH1 = 30;  // threshold for Ax to be greater than Ax0
static const int TH2 = 20; // threshold for dAx to be greater than dAy and dAz

// VARIABLES (CONSTANTS)
unsigned char datapacket = 255;
static const unsigned char BUF_DQ_BYTE = 13;
static const unsigned char BUF_EMPTY_BYTE = 0;   // buf error indicator 
static const unsigned char BUF_FULL_BYTE  = 255; // buf error indicator 
static const unsigned char MSG_START_BYTE = 255;
static const unsigned char FREQ_CMD_BYTE = 0x01; // msg cmd
static const unsigned char LEDS_CMD_BYTE = 0x02; // msg cmd
static const unsigned char DUTY_CMD_BYTE = 0x03; // msg cmd
static const int Ax0 = 128;  // at default orientation
static const int Ay0 = 153;  // at default orientation
static const int Az0 = 128;  // at default orientation

// VARIABLES (TO BE USED)
volatile unsigned char rxByte = 0;
volatile unsigned int  prevCap = 0; // volatile tells compiler the variable value can be modified at any point outside of this code
volatile unsigned int  cap = 0;
volatile unsigned int  measurement = 0; // time between rising and falling edge of TA0.1 input
volatile unsigned char axByte = 0;
volatile unsigned char ayByte = 0;
volatile unsigned char azByte = 0;
volatile unsigned int  temp = 0;
volatile unsigned int  tempThresh = 194;
volatile unsigned char buf[BUF_SIZE];
volatile unsigned int  head = 0;
volatile unsigned int  tail = 0;
volatile unsigned int  i = 0;
volatile unsigned int  dequeuedItem = 0;
volatile unsigned char dequeuedByte = 0;
volatile unsigned char startByte = 0;
volatile unsigned char cmdByte = 0;
volatile unsigned char data_H_Byte = 0;
volatile unsigned char data_L_Byte = 0;
volatile unsigned char escByte = 0;
volatile unsigned int  byteState = 0;
//volatile unsigned int  msg_byte_count = 0;
volatile unsigned int  packetReceivedFlag = 0;
volatile unsigned int  blinkMode = 0;
volatile unsigned int  brightness1 = 0;
volatile unsigned int  brightness2 = 0;
volatile unsigned int  dark1 = 0;
volatile unsigned int  dark2 = 0;
volatile unsigned int  count1 = 0;
volatile unsigned int  count2 = 0;
volatile unsigned int  autoMode = 1;
volatile unsigned int  finger = 0;

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

// [ex5] setup Timer B Up Mode - counts up to "myTB1CCR0"
// led_port    TB1.x    output port
// 1           TB1.1    P3.4
// 1           TB1.2    P3.5
// 0           TB1.1    P1.6
// 0           TB1.2    P1.7
void setup_timerB1_UP_mode(int led_port) {
    if (led_port == TIMERB_LED_PORT) {
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
    TB1CTL |= TBCLR;         // clr TBR, ensure proper reset of timer divider logic

    TB1CCR0 = myTB1CCR0;     // value to count up to in UP mode
}


void setup_timerB2_UP_mode() { // TB2.1 on P3.6
   	P3DIR  |=  (BIT6); // P3.6 and P3.7 as output
   	P3SEL0 |=  (BIT6); // select TB2.1 and TB2.2
   	P3SEL1 &= ~(BIT6); // select TB2.1 and TB2.2  // redundant

    TB2CTL |= TBSSEL__ACLK + // ACLK as clock source (8 MHz)
              MC__UP       + // Up mode
              ID__8        ; // divide input clock by 8 -> timer clk 1 MHz
    TB2CTL |= TBCLR;         // clr TBR, ensure proper reset of timer divider logic

    TB2CCR0 = myTB1CCR0;     // value to count up to in UP mode
}

// [ex6] setup Timer A CONTINUOUS Mode - counts up to TxR (counter length set by CNTL)
void setup_timerA_CONT_mode() {
	// configure P1.0 as input to timerA TA0.1
    P1DIR &= ~BIT0;
    P1SEL1 &= ~BIT0;
    P1SEL0 |= BIT0;

    // configure Timer A
    TA0CTL   = TASSEL__ACLK   + // ACLK as clock source (8 MHz)
               MC__CONTINUOUS + // Continuous mode
               ID__8          + // divide input clock by 8 -> timer clk 1 MHz
               TACLR          ; // Timer A counter clear, ensure proper reset of timer divider logic
    // Note:   TAIE (overflow interrupt is NOT enabled)
}

// Timer B only: can set counter length using TBxCTL.CNTL

// Setup ADC (for accelerometer and NTC)
void setup_ADC() {
	// Power the Accelerometer and/or NTC
    P2DIR |= BIT7; // configure P2.7 as output
    P2OUT |= BIT7; // output high to provide power

    // set up A12, A13, A14 (accelerometer X, Y, Z) as input for ADC
    P3SEL0 |= BIT0 + BIT1 + BIT2;
    P3SEL1 |= BIT0 + BIT1 + BIT2;

    // set up A4 (NTC) as input for ADC on P1.4
    P1SEL0 |= BIT4;
    P1SEL1 |= BIT4;

    ADC10CTL0 |= ADC10ON    + // turn on ADC
                 ADC10SHT_2 ; // sample & hold 16 clks
    ADC10CTL1 |= ADC10SHP;    // sampling input is sourced from timer
    ADC10CTL2 |= ADC10RES;    // 10-bit conversion results
}

// ADC to read a specified channel
unsigned int adcReadChannel(int channel)
{
    ADC10MCTL0 |= channel;  // channel select; Vref=AVCC
    // sample ADC (enable conversion)
    ADC10CTL0 |= ADC10ENC + // enable conversion
                 ADC10SC  ; // start conversion

    // wait for ADC conversion complete
    while(ADC10CTL1 & ADC10BUSY);
    int result = ADC10MEM0;

    // ADC disable conversion to switch channel
    ADC10CTL0 &= ~ADC10ENC;

    ADC10MCTL0 &= ~channel; // clear channel selection

    return result;
}

// [ex7,8] Set up TA0.1 to interrupt every "TIMER_MILLISEC" (1000/TIMER_MILLISEC Hz)
void timerA_interrupt(int millisec) {
	// configure TA0.1
    TA0CCTL0 |= CCIE;    // Capture/compare interrupt enable
    TA0CCR0 = TIMER_MILLISEC * 1000 - 1; // overflow at X ms if 1MHz in

    // start Timer A
    TA0CTL  |= TASSEL_2 + // Timer A clock source select: 2 - SMCLK
               MC_1     + // Timer A mode control: 1 - Up Mode
               TACLR    ; // clear TA0R
}

// circular buffer enqueue
void enqueue(int val)
{
    if ((head + 1) % BUF_SIZE == tail) { // buffer FULL (head + 1 == tail)
        txUART(BUF_FULL_BYTE); // Error: buffer full
    }
    else {
        buf[head] = val; // enqueue
        head = (head + 1) % BUF_SIZE; // head++;
    }
}

// circular buffer dequeue (FIFO)
char dequeue() {
    unsigned char result = 0;
    if (head == tail) { // buffer empty
        txUART(BUF_EMPTY_BYTE); // Error: buffer empty
    }
    else {
        result = buf[tail]; // dequeue
        tail = (tail + 1) % BUF_SIZE; // tail++;
    }
    return result;
}

// circular buffer dequeue (LIFO)
char dequeue_LIFO() {
    unsigned char result = 0;
    if (head == tail) { // buffer empty
        txUART(BUF_EMPTY_BYTE); // Error: buffer empty
    }
    else {
        result = buf[head]; // dequeue
        head = (head + BUF_SIZE - 1) % BUF_SIZE; // head--;
    }
    return result;
}

// debugging: print circular buffer contents over UART
void printBufUART()
{
    for (i = tail; i != head; i = (i + 1) % BUF_SIZE) {
        txUART(buf[i]);
    }
}

/////////////////////////////////////////////////
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    setup_clocks();  // 8 MHz DCO on MCLK, ACLK, SMCLK
	setup_UART(~UART_INT_EN);  // set up UART with UART RX interrupt disabled
	setup_ADC();     // set up ADC to read from accelerometer and NTC
	// timer interrupt every 40 ms (25 Hz)
	timerA_interrupt(TIMER_MILLISEC); // TA0.1 interrupt every TIMER_MILLISEC

	setup_LEDs(); // dev board LED 1 to 8

	setup_buttons_input();
	enable_buttons_interrupt();

	setup_timerB1_UP_mode(TIMERB_LED_PORT); // TB1.1 and TB1.2 on P3.4 and P3.5
	setup_timerB2_UP_mode();                // TB2.1 on P3.6

    // initialize PWM outputs to default: 500 Hz, 50% duty TB1.1, 25% duty TB1.2
    TB1CCTL1 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR1 = myTB1CCR1;
    TB1CCTL2 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR2 = myTB1CCR1;
    TB2CCTL1 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)

    TB1CCTL0 |= CCIE; // [x5] enable interrupt for CCR0
    //TB2CCTL0 |= CCIE; // [x5] enable interrupt for CCR0


	// housekeeping settings
	TB1CCR0 = blinkFreq1;
	P3OUT  &= ~(BIT4 + BIT5);
	// for finger:
	TB2CCR0 = blinkFreq2;
	P3OUT  &= ~(BIT6);
	TB2CCR1 = blinkFreq2;

    /////////////////////////////////////////////////
    _EINT();         // enable global interrupt

    while(1) {
		//if (P2OUT & BIT0) P3OUT |=  BIT7; // turn ON LED to indicate autoMode enabled
		//else              P3OUT &= ~BIT7; // turn OFF LED to indicate autoMode disabled

		if (finger == 1) { // flash finger
		    blinkMode = 3;
		    finger = 0;
		}
		else if (P3OUT & BIT7) { // autoMode is enabled
			// FSM states: blinkMode
			if ((axByte > Ax0+TH1) && (axByte-Ax0+TH2 > ayByte-Ay0) && (axByte-Ax0+TH2 > azByte-Az0)) { // (dAx > TH1) && (dAx > dAy) && (dAx > dAz)
				PJOUT |=  BIT0; // ON
				PJOUT &= ~BIT1;

				blinkMode = 2;
			}
			else if ((axByte < Ax0 - TH1) && (axByte-Ax0-TH2 < ayByte-Ay0) && (axByte-Ax0-TH2 < axByte-Az0)) { // (dAx < TH1) && (dAx < dAy) && (dAx < dAz)
				PJOUT &= ~BIT0;
				PJOUT |=  BIT1; // ON

				blinkMode = 1;
			}
			else {
				PJOUT &= ~BIT0;
				PJOUT &= ~BIT1;

			}
		} //if autoMode

    	/////////////////////////////////////////////////
		// FSM output
		// for arrows:
		TB1CCR1 = brightness1;
		TB1CCR2 = brightness1;


		//else if (dark2) {
		//	P3SEL0 &= ~(BIT6);
		//}

		if (blinkMode == 1) {
			if (dark1) P3SEL0 &= ~(BIT4 + BIT5);
			else {
		    	P3SEL0 |=  BIT4;
		    	P3SEL0 &= ~BIT5;
			}
		    P3SEL0 &= ~BIT6;
		}
		else if (blinkMode == 2) {
			if (dark1) P3SEL0 &= ~(BIT4 + BIT5);
			else {
		    	P3SEL0 &= ~BIT4;
		    	P3SEL0 |=  BIT5;
			}
		    P3SEL0 &= ~BIT6;
		}
		else if (blinkMode == 3) {
		    if (dark2) P3SEL0 &= ~BIT6;
            else       P3SEL0 |=  BIT6;
		    P3SEL0 &= ~BIT4;
		    P3SEL0 &= ~BIT5;
		}
		else {
			P3SEL0 &= ~(BIT4 + BIT5);
			P3SEL0 &= ~(BIT6);
		}

    }

    return 0;
}

// [ex7,8] ISR for TA0.1 CCR0 overflow every TIMER_MILLISEC 
#pragma vector = TIMER0_A0_VECTOR
__interrupt void timerA0()
{
    // [ex7] every 40 ms (250 Hz)
    // sample accelerometer Ax, Ay, Az
    axByte = adcReadChannel(X_CH) >> 2;
    ayByte = adcReadChannel(Y_CH) >> 2;
    azByte = adcReadChannel(Z_CH) >> 2;

	// [f1]
	enqueue(axByte);

    // [ex7] transmit 255, Ax, Ay, Az over UART
    txUART(datapacket);
    txUART(axByte);
    txUART(ayByte);
    txUART(azByte);

    TA0CCTL0 &= ~CCIFG; // clear IFG
}

#pragma vector = TIMER1_B0_VECTOR
__interrupt void tb1_ccr0()
{
	// glow arrows from dark to bright
    if (brightness1 == blinkFreq1) {
        brightness1 = 0;
        if (dark1 == 0) dark1 = 1;
        else            dark1 = 0;
    }
    else brightness1++;

    count1++;

    if (count1 > COUNT_MAX) { // count expired
        blinkMode = 0;
        count1 = 0;
    }

    // blink finger
    if (brightness2 == blinkFreq2) {
        brightness2 = 0;
        if (dark2 == 0) dark2 = 1;
        else            dark2 = 0;
    }
    else brightness2++;

    TB1CCTL0 &= ~CCIFG;
}

#pragma vector = TIMER2_B0_VECTOR
__interrupt void tb2_ccr0()
{
	// glow dark to bright
    if (brightness2 == blinkFreq2) {
        brightness2 = 0;
        if (dark2 == 0) dark2 = 1;
        else            dark2 = 0;
    }
    else brightness2++;

    //count2++;

    //if (count2 > COUNT_MAX) { // count expired
    //    blinkMode = 0;
    //    count2 = 0;
    //}

    TB2CCTL0 &= ~CCIFG;
}


// ISR for capture from TA0.1
// [ex6] overflow is NOT enabled, so this will NOT fire when TAR overflows
#pragma vector=TIMER0_A1_VECTOR
__interrupt void timerA(void)
{
    if (TA0IV & TA0IV_TACCR1){ // TA0CCR1_CCIFG is set
        cap = TA0CCR1;
        if(!(TA0CCTL1 & CCI)){ // current output is low (it was previously high)
			// save the measurement (time now - starting time)
            measurement = cap - prevCap; // time between rising and falling edge
            // TA0CCR2 = measurement; // save to a register (trying to see it in the debugger)
        }
        else if (TA0CCTL1 & CCI) { // current output is high (it was previously low)
            prevCap = cap; // reset the measurement starting time
        }
    	TA0CCTL1 &= ~CCIFG; // clear IFG
    }
}

#pragma vector = PORT4_VECTOR
__interrupt void P4_ISR()
{
    switch (P4IV) {
        case P4IV_P4IFG0: // P4.0 (SW1)
			//if (autoMode == 1) autoMode = 0;
			//else               autoMode = 1;
			//P2OUT ^= BIT0;
            P3OUT ^= BIT7;
			
            P4IFG &= ~BIT0; // clear IFG
            break;
        case P4IV_P4IFG1: // P4.1 (SW2)

			finger = 1;

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
