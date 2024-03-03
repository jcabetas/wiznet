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
uint8_t estadoDeseado;

uint16_t initW25q16(void);
void initDisplay(void);
void initColas(void);
void initHM10(void);
uint8_t esperaHM10(void);
void initSerialHM10(void);
void initRF95(void);
void initSpiPins(void);
void initLlamador(void);
void initCalendar(void);

void leeVariablesC(void);
void ponEnLCDC(uint8_t fila, char const msg[]);
int chLcdprintfFilaC(uint8_t fila, const char *fmt, ...);
extern uint8_t hayLcd;

void tickLed(uint8_t numPuls, uint16_t msEntrePuls, stm32_gpio_t *GPIO, uint32_t PAD)
{
    for (uint8_t numP=0;numP<numPuls;numP++)
    {
        palClearPad(GPIO, PAD);         // enciende
        palClearPad(GPIOC, GPIOC_LED);         // enciende
        chThdSleepMilliseconds(100);    // mantiene 100 ms
        palSetPad(GPIO, PAD);           // apagado
        palSetPad(GPIOC, GPIOC_LED);         // enciende
        if (numP<numPuls-1)             // si no es el ultimo, deja apagado 200 ms
            chThdSleepMilliseconds(200);
    }
    chThdSleepMilliseconds(msEntrePuls);
}


static const I2CConfig i2ccfg = {
  OPMODE_I2C,
  400000,
  FAST_DUTY_CYCLE_2,
};
void initI2C(void)
{
    palSetLineMode(LINE_SDA2,PAL_MODE_ALTERNATE(9) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_SCL2,PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    chThdSleepMilliseconds(50); // espera a que se inicie LCD
    i2cStart(&LCD_I2C, &i2ccfg); // LCD
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
    chprintf((BaseSequentialStream *)&SD1,"Prueba de SD1\n");
    chprintf((BaseSequentialStream *)&SD1,"Segunda linea\n");
}

void cb_sensor(void *)
{
    estadoDeseado = !palReadLine(LINE_SENSOR);
    if (estadoDeseado)
        palClearLine(LINE_LED);         // enciende
    else
        palSetLine(LINE_LED);           // apagado
    chSysLockFromISR();
    chEvtBroadcastI(&sensor_source);
    chSysUnlockFromISR();
}

void initSensor(void)
{
    estadoDeseado = !palReadLine(LINE_SENSOR);
    if (estadoDeseado)
        palClearLine(LINE_LED);         // enciende
    else
        palSetLine(LINE_LED);           // apagado
    palSetLineMode(LINE_SENSOR, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_SENSOR, PAL_EVENT_MODE_BOTH_EDGES);     // Falling edge creates event
    palSetLineCallback(LINE_SENSOR, cb_sensor, NULL); // Active callback
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

  chEvtObjectInit(&updateLCD_source);
  chEvtObjectInit(&sensor_source);

  initColas();
  initI2C();
  initSpiPins();
  initDisplay();
  initSD1();
  chprintf((BaseSequentialStream *)&SD1,"Arrancado LCD\n");
  chLcdprintfFilaC(3,"Arrancado LCD");
  chThdSleepMilliseconds(100);
  leeVariablesC();
  chprintf((BaseSequentialStream *)&SD1,"Leido variables\n");
  chLcdprintfFilaC(3,"Leido variables");
  chThdSleepMilliseconds(100);
 // testRele();
  initHM10();
  initLlamador();
  initSensor();
  while (true) {
      chThdSleepMilliseconds(50);
  }
}
