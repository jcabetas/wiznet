/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "varsFlash.h"
#include "lcd.h"
#include "chprintf.h"

extern event_source_t updateLCD_source;
extern event_source_t sensor_source;

extern event_source_t rf95int_source;
extern event_source_t newMsgRx_source;
extern event_source_t newMsgTx_source;

uint16_t initW25q16(void);
void initDisplay(void);
void initColas(void);
void initHM10(void);
void llamadorInit(void);
void pozoInit(void);
uint8_t initModbus(void);
void arrancaRadioC(void);
uint8_t initModbusSlave(void);

void leeVariablesC(void);
void ponEnLCDC(uint8_t fila, char const msg[]);
int chLcdprintfFilaC(uint8_t fila, const char *fmt, ...);
extern uint8_t hayLcd;

void tickLed(void)
{
    for (uint16_t i=0;i<4;i++)
    {
        palClearLine(LINE_LED1);       // enciende
        chThdSleepMilliseconds(50);
        palSetLine(LINE_LED1);         // apaga
        chThdSleepMilliseconds(150);
    }
}



void initSD1(void)
{
    palSetLineMode(LINE_RX1,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_TX1,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    SerialConfig configSD1;
    configSD1.speed = 115200;
    configSD1.cr1 = 0;
    configSD1.cr2 = USART_CR2_STOP1_BITS;// | USART_CR2_LINEN;;
    configSD1.cr3 = 0;
    sdStart(&SD1, &configSD1);
}


void testRele(void)
{
    palSetLineMode(LINE_RELE, PAL_MODE_OUTPUT_PUSHPULL);
    palClearLine(LINE_RELE);
    chThdSleepMilliseconds(1000);
    palSetLine(LINE_RELE);
    chThdSleepMilliseconds(1000);
    palClearLine(LINE_RELE);
}

/*
 * Application entry point.
 */


int main(void) {
  halInit();
  chSysInit();

  chEvtObjectInit(&sensor_source);
  chEvtObjectInit(&rf95int_source);
  chEvtObjectInit(&newMsgRx_source);
  chEvtObjectInit(&newMsgTx_source);
  chEvtObjectInit(&updateLCD_source);

  tickLed();

  initDisplay();
  chLcdprintfFilaC(3,"Arrancado LCD");
  initSD1();
  chThdSleepMilliseconds(100);
  chLcdprintfFilaC(3,"Inicializo MB");
  initModbusSlave();
  chLcdprintfFilaC(3,"Arranco radio");
  chThdSleepMilliseconds(100);
  arrancaRadioC();
  chLcdprintfFilaC(3,"Arranco HM10");
  chThdSleepMilliseconds(100);
  initHM10();
  while (true) {
      chThdSleepMilliseconds(50);
  }
}
