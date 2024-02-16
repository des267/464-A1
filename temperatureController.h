/**
* temperatureController.h
 *
 * Author: Desmond Stular
 *   Date: February 12, 2024
*/

#ifndef TEMPERATURECONTROLLER_H_
#define TEMPERATURECONTROLLER_H_

#include "driverlib/gpio.h"
#include "driverlib/ioc.h"
#include "driverlib/prcm.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include <math.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0
#define FIFO_SIZE 4

extern int chosenTemp;
extern int8_t *input;

void setupLED();
void setupTimer();
void setupUART();
void timerInterrupt();
void uartInterrupt();
int isValidCommand();
int isSleepCommand();
void getTempFromSerialInput();
void setupAll();

#endif
