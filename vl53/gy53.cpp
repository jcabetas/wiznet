/*
 * gy53.cpp
 *
 *  Created on: 16 sept 2023
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
#include "tty.h"
#include "string.h"
#include <stdlib.h>
#include "chprintf.h"

using namespace chibios_rt;

// datos de https://img.banggood.com/file/products/20180830020532SKU645408.pdf
uint16_t distancia;
uint8_t gy53vive;

/*
 * Segun https://img.banggood.com/file/products/20180830020532SKU645408.pdf
 * Byte0: 0x5A The frame head logo
   Byte1: 0x5A The frame head logo
   Byte2: 0x15 Frame data type
   Byte3: 0x03 The amount of data
   Byte4: 0x00~0xFF High 8 digits before data
   Byte5: 0x00~0xFF Low 8 digits before data
   Byte6: 0x00~0xFF Module measurement mode
   Byte7: 0x00~0xFF Checksum (previous data sums up,only 8 bits left)
 */



// mensajes desde GY53. Empiezan en 0x5A
void chgetGY53TimeOut(BaseChannel *pSD, uint8_t *buffer, uint16_t bufferSize, systime_t timeout, uint16_t *numBytes, uint8_t *huboTimeout)
{
    uint8_t ch;
    msg_t msgReceived;
    *huboTimeout = 0;
    *numBytes = 0;
    while (1==1)
    {
        msgReceived = chnGetTimeout(pSD, timeout);
        if (msgReceived==Q_TIMEOUT)
        {
          *huboTimeout = 1;
          return;
        }
        ch = (uint8_t) msgReceived;
        if (*numBytes<bufferSize)
            buffer[(*numBytes)++] = ch;
        if (*numBytes>=8)
            break;
    }
}
/*
 * Lector GY53
 */
static THD_WORKING_AREA(waThreadGY53, 128);
static THD_FUNCTION(ThreadGY53, arg) {
  (void)arg;
  uint8_t buffer[15], huboTimeout, checksum;
  uint16_t numBytes;
  chRegSetThreadName("GY53");
  while (true) {
      // lee 8 bytes (frames se separan 100ms)
      chgetGY53TimeOut((BaseChannel *)&SD2, buffer, sizeof(buffer), TIME_MS2I(20), &numBytes, &huboTimeout);
      if (huboTimeout)
      {
          gy53vive = 0;
          continue;
      }
      // veamos si el frame esta bien
      if (buffer[0]!=0x5A || buffer[1]!=0x5A || buffer[2]!=0x15 || buffer[3]!=0x3)
      {
          gy53vive = 0;
          continue;
      }
      // checksum
      checksum = 0;
      for (uint8_t i=0;i<=6;i++)
          checksum += buffer[i];
      if (checksum!=buffer[7])
      {
          gy53vive = 0;
      }
      else
      {
          distancia = (buffer[4]<<8) | buffer[5];
          gy53vive = 1;
      }
  }
}



static const SerialConfig ser_cfg = {9600, 0, 0, 0, };
void initSerial(void) {
    palClearPad(GPIOA, GPIOA_TX2);
    palSetPad(GPIOA, GPIOA_RX2);
    palSetPadMode(GPIOA, GPIOA_RX2, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOA, GPIOA_TX2, PAL_MODE_ALTERNATE(7));
    sdStart(&SD2, &ser_cfg);
}

void initGY53(void)
{
    gy53vive = 0;
    distancia = 0;
    initSerial();
    chThdCreateStatic(waThreadGY53, sizeof(waThreadGY53), NORMALPRIO, ThreadGY53, NULL);
}

