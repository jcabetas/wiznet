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
#include "wizchip_port.h"
#include "MQTT_Example.h"
#include "chprintf.h"
#include "chprintf.h"
#include <stdio.h>

int W5500_Init(void);
void led_Control(int state);
void mqtt_initThread(void);

void tickLed(uint16_t numVeces)
{
    for (uint16_t i=0;i<numVeces;i++)
    {
        palClearLine(LINE_LED);       // enciende
        chThdSleepMilliseconds(50);
        palSetLine(LINE_LED);         // apaga
        chThdSleepMilliseconds(150);
    }
}

void led_Control(int state)
{
  palWriteLine(LINE_LED, !state);
}


void initSD6(void)
{
    palSetLineMode(LINE_RX6,PAL_MODE_ALTERNATE(8) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_TX6,PAL_MODE_ALTERNATE(8) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    SerialConfig configSD6;
    configSD6.speed = 115200;
    configSD6.cr1 = 0;
    configSD6.cr2 = USART_CR2_STOP1_BITS;// | USART_CR2_LINEN;;
    configSD6.cr3 = 0;
    sdStart(&SD6, &configSD6);
}


/*
 * Application entry point.
 */


/*
 * #define LINE_RSTW5500           PAL_LINE(GPIOB,GPIOB_RSTW5500)
#define LINE_INTw5500           PAL_LINE(GPIOB,GPIOB_INTw5500)
#define LINE_CSW5500            PAL_LINE(GPIOA,GPIOA_CSW5500)
#define LINE_SCKW5000           PAL_LINE(GPIOA,GPIOA_SCKW5000)
#define LINE_SPI1_MISO          PAL_LINE(GPIOA,GPIOA_SPI1_MISO)
#define LINE_SPI1_MOSI          PAL_LINE(GPIOA,GPIOA_SPI1_MOSI)
 */


static const SPIConfig spicfg = {
    .circular         = false,
    .slave            = false,
    .data_cb          = NULL,
    .error_cb         = NULL,
    .ssport           = GPIOB,
    .sspad            = GPIOB_DIO0,
    .cr1              = SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_MSTR ,
    .cr2              = 0
};




void initSPI3pins(void)
{
    // defino los pines
    /*
     * CS -   PA4
#define LINE_SPI3_SCK           PAL_LINE(GPIOB,GPIOB_SPI3_SCK)
#define LINE_SPI3_MISO          PAL_LINE(GPIOB,GPIOB_SPI3_MISO)
#define LINE_SPI3_MOSI          PAL_LINE(GPIOB,GPIOB_SPI3_MOSI)
     */
    palClearLine(LINE_SCKW5000);
    palClearLine(LINE_SPI3_MISO);
    palClearLine(LINE_SPI3_MOSI);
    palSetLineMode(LINE_SCKW5000,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* SPI SCK.             */
    palSetLineMode(LINE_SPI3_MISO,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MISO.                */
    palSetLineMode(LINE_SPI3_MOSI,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MOSI.                */
}

int main(void) {
  uint16_t ms = 0;
  uint16_t count = 0;
  char buffer[30];
  halInit();
  chSysInit();

  initSPI3pins();
  tickLed(4);
  spiStart(&SPID3, &spicfg);
//  int error = W5500_Init();
//  mqtt_network_init();
//  mqtt_connect_broker();
//  if (mqtt_subscribe("controllerstech/sub") != 0)
//      tickLed(100);
  mqtt_initThread();
  while (true) {
//      mqtt_yield();
//      if (ms>=5000)
//      {
//          snprintf (buffer, sizeof(buffer), "STM32 W5500 -> %d", count++);
//          mqtt_publish("controllerstech/pub", buffer);
//          ms = 0;
//      }
      chThdSleepMilliseconds(50);
      ms += 50;
  }
}
