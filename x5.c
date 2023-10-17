#include <msp430.h> 

/**
 * main.c - ex10 for exam
 *
 * [ex10] Process Message Packet (packet size = byte)
 * PC TX requirements: need MSG_SIZE=3 consecutive packets
 * blink LED5 (freq reading on P3.4)
 *
 * | MSG_START_BYTE | cmdByte       | dataByte                    |
 * |----------------|---------------|-----------------------------|
 * | 255            | 1 light2dark  | frequency between 0 and 254 |
 * | 255            | 2 dark2light  | frequency between 0 and 254 |
 * | 255            | 4 OFF         | frequency between 0 and 254 |
 *
 * [ex9] Circular Buffer
 * enqueue over UART RX, dequeue if received "BUF_DQ_BYTE"
 *     if buf FULL,  transmit "BUF_FULL_BYTE"
 *     if buf EMPTY, transmit "BUF_EMPTY_BYTE"
 * [ex9 debug] print circular buffer contents over UART
 */

// PARAMETERS
#define LEDOUTPUT        0b11111111
#define UART_CHAR0       'a'
// myTB1CCR0 = 1 MHz / PWM frequency (Hz)
#define myTB1CCR0        2000
// milliseconds per timer interrupt (freq = 1000/TIMER_MILLISEC Hz)
//#define TIMER_MILLISEC   40
#define BUF_SIZE         50
#define MSG_SIZE         3

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

// VARIABLES (CONSTANTS)
static const unsigned char datapacket = 255;
static const unsigned char BUF_DQ_BYTE = 13;
static const unsigned char BUF_EMPTY_BYTE = 0;   // buf error indicator
static const unsigned char BUF_FULL_BYTE  = 255; // buf error indicator
static const unsigned char MSG_START_BYTE = 255;
static const unsigned char FREQ_CMD_BYTE = 0x01; // msg cmd
static const unsigned char LEDS_CMD_BYTE = 0x02; // msg cmd
static const unsigned char DUTY_CMD_BYTE = 0x03; // msg cmd

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
volatile unsigned char dataByte = 0;
volatile unsigned int  data = 0;
volatile unsigned int  byteState = 0;
//volatile unsigned int  msg_byte_count = 0;
volatile unsigned int  packetReceivedFlag = 0;
volatile unsigned int  flag1 = 0;
volatile unsigned int  flag2 = 0;
volatile unsigned int  brightness = 0;
volatile unsigned int  dark = 0;

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

// [ex10x] display a byte in binary on LEDs
void byteDisplayLED(unsigned char in)
{
    char high = in & ~LOWMASK;
    char low = in & ~HIGHMASK;
    PJOUT &= ~LOWMASK;
    P3OUT &= ~HIGHMASK;
    PJOUT |= low;
    P3OUT |= high;
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
void setup_timerB_UP_mode(int led_port) {
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
    TA0CCR0 = myTA0CCR0; // overflow at X ms if 1MHz in

    // start Timer A
    TA0CTL  |= TASSEL_2 + // Timer A clock source select: 2 - SMCLK
               MC_1     + // Timer A mode control: 1 - Up Mode
               TACLR    ; // clear TA0R
}

// [ex8] display temp on LEDs
void displayTempOnLEDs()
{
    int lightUpTo = (tempThresh - temp);            // signed
    if (lightUpTo < 1)        lightUpTo = 1;        // min = 1
    if (lightUpTo > NUM_LEDS) lightUpTo = NUM_LEDS; // max = NUM_LEDS

    // PJx = 1<<x * (lightUpTo>=x) x from 1 to 4 // PJ.0-3 -> LED 1-4
    // P3x = 1<<x * (lightUpTo>=x) x from 5 to 8 // P3.4-7 -> LED 5-8
    PJOUT_L = 0b00001000*(lightUpTo>=4) + 0b00000100*(lightUpTo>=3) + 0b00000010*(lightUpTo>=2) + 0b00000001*(lightUpTo>=1);
    P3OUT   = 0b10000000*(lightUpTo>=8) + 0b01000000*(lightUpTo>=7) + 0b00100000*(lightUpTo>=6) + 0b00010000*(lightUpTo>=5);
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
    WDTCTL = WDTPW | WDTHOLD;  // stop watchdog timer

    setup_clocks();            // 8 MHz DCO on MCLK, ACLK, SMCLK
    setup_UART(UART_INT_EN);   // set up UART with UART RX interrupt enabled

    // setup Timer B [ex5] to output PWM on P1.6 and P1.7
    setup_timerB_UP_mode(TIMERB_LED_PORT); // TB1.1 and TB1.2 on P1.6 and P1.7

    // initialize PWM outputs to default: 500 Hz, 50% duty TB1.1, 25% duty TB1.2
    TB1CCTL1 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR1 = myTB1CCR1;
    TB1CCTL2 |= OUTMOD_7;    // OUTMOD 7 = reset/set (reset at CCRx, set at CCR0)
    TB1CCR2 = 0;

    TB1CCTL0 |= CCIE; // [x5] enable interrupt for CCR0

    //setup_buttons_input();
    //enable_buttons_interrupt();

    /////////////////////////////////////////////////
    _EINT();         // enable global interrupt

    //data = 254; //
    //data = 200;
    data = 500; //
    flag1 = 1;
    cmdByte = 1;
    //data = 100;
    //data = 50;

    while(1) {
        TB1CCR0 = data;
        if (dark) TB1CCR1 = 0;
        else      TB1CCR1 = brightness;

        //flag2 = 1;

        // loop if there are any items in buffer
        if (head != tail) { // if buffer not empty
            dequeuedByte = dequeue(); // dequeue
            txUART(dequeuedByte); //debug

            // [ex10] detect and store msg packet
            switch(byteState) {
                case 1:
                    cmdByte = dequeuedByte;
                    byteState = 2;
                    break;
                case 2:
                    dataByte = dequeuedByte;
                    data = 500 - dataByte; // TODO adjust
                    byteState = 0;
                    break;
                    //packetReceivedFlag = 1; // entire packet received, process packet in main

                    //printBufUART(); // print to UART for debug

                    // [ex10] execute commands
                    switch(cmdByte) {
                        case 1: // cmd 1: set Timer B CCR0 (period)
                            flag1 = 1;
                            flag2 = 0;
                            break;
                        case 2: // cmd 2: display data_L_Byte on LEDs
                            flag1 = 0;
                            flag2 = 1;
                            brightness = 0;
                            break;
                        case 4: // cmd 3: set Timer B CCR1 (duty cycle)
                            brightness = 0;
                            TB1CCR1 = 0;
                            dark = 1;
                            flag1 = 0;
                            flag2 = 0;
                            break;
                        default:
                            break;
                    } // switch (cmdByte)

                    //packetReceivedFlag = 0;
                    break;
                default:
                    if (dequeuedByte == MSG_START_BYTE) {
                        byteState = 1;
                    }
                    break;
            } // switch (byteState)
        } // if items in buffer
    } // infinite loop

    return 0;
} // end main
/////////////////////////////////////////////////
// ISRs
#pragma vector = TIMER1_B0_VECTOR
__interrupt void tb0()
{
    if (cmdByte == 2) {
            if (brightness == 0) {
                brightness = data;
                if (dark == 0) dark = 1;
                else dark = 0;
            }
            else brightness--;
        }
    else if (cmdByte == 1) {
        if (brightness == data) {
            brightness = 0;
            if (dark == 0) dark = 1;
            else dark = 0;
        }
        else brightness++;
    }
    else if (cmdByte == 4){
        dark = 1;
    }

    TB1CCTL0 &= ~CCIFG;
}

// ISR for UART receive
#pragma vector=USCI_A0_VECTOR
__interrupt void UCA0RX_ISR()
{
    rxByte = UCA0RXBUF; // get the received byte from UART RX buffer

    enqueue(rxByte); // [ex10]

    //// [ex9] circular buffer
    //if (rxByte == BUF_DQ_BYTE) { // dequeue if receive a carriage return (ASCII 13)
    //  dequeuedItem = dequeue();
    //}
    //else {
    //    enqueue(rxByte);
    //}
    //printBufUART(); // [ex9] debug


    //// [ex4] echo rxByte, rxBtye + 1
    //// transmit back the received byte
    //txUART(rxByte);                   // UART transmit
    //// transmit back rxByte + 1
    //txUART(rxByte + 1);               // UART transmit

    //// [ex4] turn LED ON if rxByte == 'j', OFF if rxByte == 'k'
    //if      (rxByte == 'j') turn_ON_LED_ones(LEDOUTPUT);  // turn ON the 1's in LEDOUTPUT
    //else if (rxByte == 'k') turn_OFF_LED_ones(LEDOUTPUT); // turn OFF the 1's in LEDOUTPUT

    // UART RX IFG is self clearing
}
