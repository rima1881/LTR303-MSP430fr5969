#include <msp430.h> 

#define SDA_PIN                         BIT4
#define SCL_PIN                         BIT5

#define LTR303_ADDRESS                  0x29
#define LTR303_ALS_CONTR_REG            0x80
#define LTR303_ALS_MEAS_RATE_REG        0x85
#define LTR303_ALS_DATA_CH1_REG         0x88



volatile enum { IDLE, START, STOP, WRITE ,READ } i2cState = IDLE;
volatile unsigned char i2cByte;
volatile int i2cBitCount = 0;


void setupI2CPins() {

    PM5CTL0 &= ~LOCKLPM5;       // Disable the GPIO power-on default high-impedance mode

    P1DIR |= SCL_PIN;  // SCL as output
    P1OUT &= ~SCL_PIN; // SCL Low

    P1DIR &= ~SDA_PIN; // SDA as input (high)
    P1OUT &= ~SDA_PIN; // Ensure pull-up is disabled

}


void setupTimer() {
    TACTL = TASSEL_2 + MC_1; // SMCLK, Up mode
    TACCR0 = 10; // Adjust for appropriate I2C speed
    TACCTL0 = CCIE; // Enable Timer_A interrupt
}


#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A_ISR(void) {
    switch (i2cState) {

        //*********************************************************
        case START:

            if (i2cBitCount == 0)
                P1DIR |= SDA_PIN;  // SDA Low

            else if (i2cBitCount == 1) {
                P1DIR &= ~SDA_PIN; // SDA High for repeated start
                i2cState = WRITE;
                i2cBitCount = -1;
            }

            break;
        //*********************************************************
        case STOP:
            if (i2cBitCount == 0) {
                P1DIR |= SDA_PIN;  // SDA Low
                P1OUT |= SCL_PIN;  // SCL High
            } else if (i2cBitCount == 1) {
                P1DIR &= ~SDA_PIN; // SDA High
                i2cState = IDLE;
            }
            break;
        //*********************************************************
        case WRITE:
            if (i2cBitCount < 8) {

                if (i2cByte & 0x80)
                    P1DIR &= ~SDA_PIN; // SDA High
                else{
                    //SDA LOW
                    P1DIR |= SDA_PIN;
                    P1OUT &= ~SDA_PIN;
                }
                i2cByte <<= 1;
            }
            else if (i2cBitCount == 8)
                P1DIR &= ~SDA_PIN; // Release SDA for ACK

            else if (i2cBitCount >= 9)
                i2cState = IDLE; // Done with byte

            break;
        //*********************************************************
        case READ:
            if (i2cBitCount < 8) {

                P1DIR &= ~SDA_PIN; // Set SDA to input
                P1OUT |= SCL_PIN;  // SCL High
                i2cByte <<= 1;

                if (P1IN & SDA_PIN)
                    i2cByte |= 0x01; // Read bit

            }
            else if (i2cBitCount == 8)
                P1DIR |= SDA_PIN;  // Set SDA Low for ACK

            else if (i2cBitCount >= 9)
                i2cState = IDLE; // Done with byte

            break;
        //*********************************************************
    }

    //  toggle clock when reading and writing
    if (i2cState == READ || i2cState == WRITE)
            P1OUT ^= SCL_PIN;

    i2cBitCount++;
}

void i2cStart() {
    i2cState = START;
    i2cBitCount = 0;
}

void i2cStop() {
    i2cState = STOP;
    i2cBitCount = 0;
}

void i2cWriteByte(unsigned char byte) {
    while (i2cState != IDLE); // Wait for I2C line to be idle
    i2cByte = byte;
    i2cBitCount = 0;
    i2cState = WRITE;
}

void i2cReadByte(unsigned char *byte) {
    while (i2cState != IDLE); // Wait for I2C line to be idle
    i2cState = READ;
    i2cBitCount = 0;
    while (i2cState != IDLE); // Wait for read to complete
    *byte = i2cByte;
}


int main(void)
{

	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	setupI2CPins();
	
	setupTimer();
	__bis_SR_register(GIE); // Enable interrupts

	i2cStart();
	i2cWriteByte(0x55); // Example byte to write
	i2cStop();


	return 0;
}
