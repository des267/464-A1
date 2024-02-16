/**
* temperatureController.c
 *
 * Author: Desmond Stular
 *   Date: February 12, 2024
*/

#include "temperatureController.h"
#include "stdio.h"

#define BAUD_RATE 115200
#define MS (48000000 / (8 * 1000))

// Indicates if in sleep or active mode
// Determines when UART can read next command
int activeMode = FALSE;

// Heat, cooling on status
int heatOn = FALSE;
int coolOn = FALSE;

// Temperatures
int chosenTemp = 0;
int currentTemp = 0;

// Holds user command cycles
//int8_t *command = NULL;
int8_t *input = NULL;

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

    // Power on TIMER0
    PRCMPeripheralRunEnable(PRCM_PERIPH_TIMER0);

    // Allow TIMER0 to operate while processor in sleep mode
    PRCMPeripheralSleepEnable(PRCM_PERIPH_TIMER0);

    // Load TIMER0 settings
    PRCMLoadSet();
    while ( !PRCMLoadGet() );

    // Set the clock division for TIMER0 to 8
    PRCMGPTimerClockDivisionSet(PRCM_CLOCK_DIV_8);
    PRCMLoadSet();
    while ( !PRCMLoadGet() );

    // Configure TIMER0 to one shot mode
    TimerConfigure(GPT0_BASE, TIMER_CFG_ONE_SHOT);

    // Clear timer interrupt
    TimerIntClear(GPT0_BASE, TIMER_TIMA_TIMEOUT);

    // Assign the interrupt handler for TIMER0
    TimerIntRegister(GPT0_BASE, TIMER_A, timerInterrupt);

    // Enable timer interrupt
    TimerIntEnable(GPT0_BASE, TIMER_TIMA_TIMEOUT);
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
    IOCPinTypeUart(UART0_BASE, IOID_2, IOID_3, IOID_19, IOID_18);

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
    // Clear timer interrupt
    TimerIntClear(GPT0_BASE, TIMER_TIMA_TIMEOUT);

    // if not active, convert command
    if (activeMode == FALSE) {
        // If sleep command, go to sleep
        if (isSleepCommand(input) == TRUE) {
            return;
        }

        // Gets temperature from serial input
        getTempFromSerialInput();

        // If chosen temp different from current temp, change to active mode
        if (chosenTemp != currentTemp) activeMode = TRUE;
        else return;
    }

    // In heating mode if chosen > active temp
    if (chosenTemp > currentTemp) {
        if (heatOn == FALSE) {
            heatOn = TRUE;
            TimerLoadSet(GPT0_BASE, TIMER_A, 600*MS);
            UARTCharPut(UART0_BASE, (uint8_t) ('H' - 0));
            UARTCharPut(UART0_BASE, '\n' + 0);
            UARTCharPut(UART0_BASE, '\r' + 0);
        }
        else {
            heatOn = FALSE;
            TimerLoadSet(GPT0_BASE, TIMER_A, 400*MS);
            currentTemp += (rand() % 2);
            outputTemp();
        }
        GPIO_toggleDio(IOID_6);
    }
    // In cooling mode if chosen < active temp
    else {
        //
        if (coolOn == FALSE) {
            coolOn = TRUE;
            UARTCharPut(UART0_BASE, (uint8_t) ('C' - 0));
            UARTCharPut(UART0_BASE, '\n' + 0);
            UARTCharPut(UART0_BASE, '\r' + 0);
        }
        else {
            coolOn = FALSE;
            currentTemp -= (rand() % 2);
            outputTemp();
        }
        GPIO_toggleDio(IOID_7);
        TimerLoadSet(GPT0_BASE, TIMER_A, 500*MS);
    }

    // If active temp now same as chosen, disable active mode
    if (currentTemp == chosenTemp) activeMode = FALSE;

    // Re-enable interrupt and timer
    TimerIntEnable(GPT0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(GPT0_BASE, TIMER_A);
}

void uartInterrupt() {
    int8_t i = 0;

    // if interrupt status not from RX
    if (UARTIntStatus(UART0_BASE, true) != UART_INT_RX) return;

    // Clear RX and TX interrupts
    UARTIntClear(UART0_BASE, UART_INT_RX|UART_INT_TX);

    // Free input first if not NULL
    if (input != NULL) {
        free(input);
    }

    // Allocate memory for input
    input = (int8_t*) malloc(FIFO_SIZE * sizeof(int8_t));

    // While characters available
    while (UARTCharsAvail(UART0_BASE)) {
        int32_t ch = UARTCharGetNonBlocking(UART0_BASE) & 0X000000FF;
        *(input+i) = (uint8_t) ch;
        if (i == FIFO_SIZE - 1) break;
        i++;
    }

    // If not valid command, free input memory and return
    if (isValidCommand(input) == FALSE) {
        free(input);
        input = NULL;
        return;
    }

    // Do not process new command if in active mode
    if (activeMode == TRUE) return;

    // Set timer to 0 seconds; enable to begin heat/cool process
    TimerLoadSet(GPT0_BASE, TIMER_A, 0x00);
    TimerEnable(GPT0_BASE, TIMER_A);
}

/**
 * Checks if the input from the UART is a valid command.
 *
 * @param input an int8_T array with the same length as the FIFO
 * @return True (int = 1) or False (int = 0)
 */
int isValidCommand() {
    for (int i = 0; i < FIFO_SIZE; i++) {
        int8_t ch = *(input+i);
        switch (i) {
            case 0:
                if (ch != 50 && ch != 51 && ch != 70) return FALSE;
                if (ch == 51 && *(input+1) != 48) return FALSE;
                if (ch == 70 && *(input+1) != 70) return FALSE;
                break;
            case 1:
                if (ch < 48 && ch > 57 && ch != 70) return FALSE;
                if (ch != 70 && *input == 70) return FALSE;
                break;
            default:
                if (ch != 32) return FALSE;
        }
    }
    return TRUE;
}

/*
 * Accepts the serial input and checks if it is the
 * sleep command, "FF  " or not. Returns True if sleep
 * command, and False if not.
 */
int isSleepCommand() {
    for (int i = 0; i < FIFO_SIZE; i++) {
        int8_t ch = *(input+i);
        switch (i) {
            case 0 ... 1:
                if (ch != 70) return FALSE;
                break;

            default:
                if (ch != 32) return FALSE;
        }
    }
    return TRUE;
}

void getTempFromSerialInput() {
    int j = 1;
    chosenTemp = 0;
    for (int i=0; i < 2; i++) {
        int num = *(input+i) - 48;
        chosenTemp += (num * pow(10, j));
        j--;
    }
}

void outputTemp() {
    char chTempStr[2];
    char currTempStr[2];
    sprintf(chTempStr, "%d", chosenTemp);
    sprintf(currTempStr, "%d", currentTemp);
    UARTCharPut(UART0_BASE, '(' + 0);
    for (int i=0; i<5; i++) {
        switch (i) {
            case 0:
                UARTCharPut(UART0_BASE, currTempStr[i] + 0);
                break;
            case 1:
                UARTCharPut(UART0_BASE, currTempStr[i] + 0);
                break;
            case 2:
                UARTCharPut(UART0_BASE, ',' + 0);
                break;
            case 3:
                UARTCharPut(UART0_BASE, chTempStr[i%3] + 0);
                break;
            case 4:
                UARTCharPut(UART0_BASE, chTempStr[i%3] + 0);
        }
    }

    UARTCharPut(UART0_BASE, ')' + 0);
    UARTCharPut(UART0_BASE, '\n' + 0);
    UARTCharPut(UART0_BASE, '\r' + 0);
}

/**
 * Called by main.c, calls all setup functions.
 */
void setupAll() {
    setupLED();
    setupTimer();
    setupUART();
}
