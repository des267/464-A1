/**
* temperatureController.c
 *
 * Author: Desmond Stular
 *   Date: February 12, 2024
*/

#include "temperatureController.h"

#define BAUD_RATE 115200
#define MS (48000000 / (8 * 1000))

// Indicates if in sleep or active mode
// Determines when UART can read next command
int activeMode = 0;

// Temperatures
int chosenTemp = 0;
int currentTemp = 0;

void setupLED() {
    // Power on peripheral domain and wait till on
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH);
    while (PRCMPowerDomainStatus(PRCM_DOMAIN_PERIPH) != PRCM_DOMAIN_POWER_ON);

    // Enable GPIO peripheral; enable sleep mode for GPIO
    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_GPIO);

    // Load PRCM configurations; wait till updated
    PRCMLoadSet();
    while ( !PRCMLoadGet() );

    // Set DIO 6 and 7 pin type to output
    IOCPinTypeGpioOutput(IOID_6);           // Red LED
    IOCPinTypeGpioOutput(IOID_7);           // Green LED
}

void setupTimer() {
    // Power on peripheral domain and wait till on
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH);
    while (PRCMPowerDomainStatus(PRCM_DOMAIN_PERIPH) != PRCM_DOMAIN_POWER_ON);

    // Set the clock division for TIMER0 to 8
    PRCMGPTimerClockDivisonSet(PRCM_CLOCK_DIV_8);
    PRCMLoadSet();
    while ( !PRCMLoadGet() );

    // Configure TIMER0 to one shot mode
    TimerConfigure(GPT0_BASE, TIMER_CFG_ONE_SHOT);

    // Clear timer interrupt
    TimerIntClear(GPT0_BASE, TIMER_TIMA_TIMEOUT);

    // Assign the interrupt handler for TIMER0
    TimerIntRegister(GPT0_BASE, TIMER_A, timerInterrupt);
}

void setupUART() {
    // Power on serial domain
    PRCMPowerDomainOn(PRCM_DOMAIN_SERIAL);
    while (PRCMPowerDomainStatus(PRCM_DOMAIN_SERIAL) != PRCM_DOMAIN_POWER_ON);

    // Enable UART0 peripheral run and sleep mode
    PRCMPeripheralRunEnable(PRCM_PERIPH_UART0);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_UART0);
    PRCMLoadSet();
    while ( !PRCMLoadGet() );

    // Enable UART pins
    IOCPinTypeUART(UART0_BASE, IOID_2, IOID_3, IOID_19, IOID_18);

    // Disable UART
    UARTDisable(UART0_BASE);

    // Configure UART
    UARTConfigSetExpClk(UART0_BASE, 48000000, BAUD_RATE, UART_CONFIG_WLEN_8|UART_CONFIG_STOP_ONE|UART_CONFIG_PAR_NONE);

    // Disable hardware flow control for CTS and RTS
    UARTHwFlowControlDisable(UART0_BASE);

    // Set FIFO threshold to 4 bytes
    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);

    // Assign UART interrupt handler
    UARTIntRegister(UART0_BASE, uartInterrupt);

    // Enable interrupts
    UARTIntEnable(UART0_BASE, UART_INT_RX);

    // Enable UART
    UARTEnable(UART0_BASE);
}

void timerInterrupt() {

}

void uartInterrupt() {
    int8_t input[INPUT_SIZE], i = 0;

    // Do not read user command if in active mode
    if (activeMode == TRUE) return;

    // if interrupt status not from RX
    if (UARTIntStatus(UART0_BASE, true) != UART_INT_RX) return;

    // Clear RX and TX interrupts
    UARTIntClear(UART0_BASE, UART_INT_RX|UART_INT_TX);

    // While characters available
    while (UARTCharsAvail(UART0_BASE)) {
        int32_t ch = UARTCharGetNonBlocking(UART0_BASE) & 0X000000FF;
        input[i] = (uint8_t) ch;
        if (i == INPUT_SIZE-1) break;
        i++;
    }
}

int isValidInput(int8_t input[INPUT_SIZE]) {
    for (int i = 0; i < INPUT_SIZE; i++) {
        switch (i) {
            case 0:
                if (input[i] != 50 || input[i] != 51) return 0;
                break;
            case 1:
                if (input[i] < 48 || input[i] > 57 ) return 0;
                break;
            default:
                if (input[i] != 32) return 0;
        }
    }
    return 1;
}

void setupAll() {
    setupLED();
    setupUART();
    setupTimer();
}