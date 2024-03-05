/*
 * rf95.c
 *
 *  Created on: 21 jul. 2019
 *      Author: joaquin
 */

#include "hal.h"
#include "ch.hpp"
#include "string.h"
#include "chprintf.h"
#include "lcd.h"
#include "colas.h"
#include "calendarUTC.h"
#include <RH_RF95.h>

using namespace chibios_rt;

extern "C"
{
    void initSpiPins(void);
}


/*
 * Maximum speed SPI configuration (27MHz -BR0==Fpclk/4-, CPHA=0, CPOL=0, MSb first).
  * Low speed SPI configuration (421.875kHz, CPHA=0, CPOL=0, MSb first).
 */

// en pozo     .cr1              = SPI_CR1_BR_1 | SPI_CR1_BR_0,

static const SPIConfig spicfgRF95 = {
    .circular         = false,
    .slave            = false,
    .data_cb          = NULL,
    .error_cb         = NULL,
    .ssport           = GPIOB,
    .sspad            = GPIOB_NSS,
    .cr1              = SPI_CR1_BR_1 | SPI_CR1_BR_0,
    .cr2              = 0U
};

extern event_source_t rf95int_event;
extern event_source_t newMsgRx_source;
thread_t *procesoRf95Int, *procesoRf95rx;
extern uint8_t _buf[RH_RF95_MAX_PAYLOAD_LEN];
extern volatile int16_t _lastRssi;
extern volatile uint8_t _bufLen;

//extern uint8_t checkRf95int;

extern event_source_t newMsgRx_source;
extern event_source_t newMsgTx_source;

time_t GetTimeUnixSec(void);
uint8_t putQueu(struct queu_t *colaMed, void *ptrStructOrigen);
uint8_t getQueu(struct queu_t *colaMed, void *ptrStructDestino);
void ponEnLCD(uint8_t fila, char const msg[]);



extern struct queu_t colaMsgRx;

/*
 * Interrupciones rf95. Activa proceso rf95int para leer datos
 */
static void f2_cb(void *arg) {
  (void)arg;
  chSysLockFromISR();
  // activa gestor interrupciones
  chEvtBroadcastI(&rf95int_event);
  chSysUnlockFromISR();
}



/*
 * rf95int thread.
 * gestiona interrupciones
 * lee datos de rf95 y los almacena en cola de mensajes
 */
static THD_WORKING_AREA(rf95int_wa, 1224);
static THD_FUNCTION(rf95int, p) {
  (void)p;
  event_listener_t el0;
  paquete_t tipoPaq;
  struct msgRx_t msgRx;
  chRegSetThreadName("rf95int");
  chEvtRegister(&rf95int_event, &el0, 0);
  while(!chThdShouldTerminateX()) {
    eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
//    checkRf95int = 1;
    if (chThdShouldTerminateX())
        chThdExit((msg_t) 1);
    if (evt == 0)  // timeout
        continue;
    spiStart(&SPID1, &spicfgRF95);
    tipoPaq = RH_RF95_handleInterrupt();
    RH_RF95_setModeRx();
    spiStop(&SPID1);
    if (tipoPaq==paqRx)
    {
        uint8_t size = _bufLen-4;
        if (size>sizeof(msgRx.msg))
            size = sizeof(msgRx.msg);
        msgRx.timet = calendar::getSecUnix();
        msgRx.numBytes = size;
        msgRx.rssi = _lastRssi;
        memcpy(msgRx.msg, &_buf[4], size);
        putQueu(&colaMsgRx, &msgRx);
        chEvtBroadcast(&newMsgRx_source);
        _bufLen = 0;
    }
  }
  chEvtUnregister(&rf95int_event, &el0);
  procesoRf95rx = NULL;
}


void initSpiPins(void)
{
    /*
     * SCK -  PA5    SPI1_SCK
     * MISO - PA6    SPI1_MISO
     * MOSI - PA7    SPI1_MOSI
     */
    palClearLine(LINE_SPI1_SCK);
    palClearLine(LINE_SPI1_MISO);
    palClearLine(LINE_SPI1_MOSI);
    palSetLineMode(LINE_SPI1_SCK,
                     PAL_MODE_ALTERNATE(5) |
                     PAL_STM32_OSPEED_HIGHEST);         /* SPI SCK.             */
    palSetLineMode(LINE_SPI1_MISO,
                     PAL_MODE_ALTERNATE(5) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MISO.                */
    palSetLineMode(LINE_SPI1_MOSI,
                     PAL_MODE_ALTERNATE(5) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MOSI.                */
}

void initSpiPinsCPP(void)
{
    initSpiPins();
}

void initRF95(void)
{
    chEvtObjectInit(&rf95int_event);
    palSetLine(GPIOB_NSS);
    palSetLineMode(GPIOB_NSS, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOB, GPIOB_RST_RFM, PAL_MODE_OUTPUT_PUSHPULL);
    palClearPad(GPIOB, GPIOB_RST_RFM);
    chThdSleepMilliseconds(2);
    palSetPad(GPIOB, GPIOB_RST_RFM);
    chThdSleepMilliseconds(15);
    palSetLineMode(LINE_PA10, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_PA10, PAL_EVENT_MODE_RISING_EDGE);
    palSetLineCallback(LINE_PA10, f2_cb, NULL);
    chEvtObjectInit(&newMsgRx_source);
    chEvtObjectInit(&newMsgTx_source);
    if (!procesoRf95Int)
        procesoRf95Int = chThdCreateStatic(rf95int_wa, sizeof(rf95int_wa), NORMALPRIO + 7,  rf95int, NULL);
    spiStart(&SPID1, &spicfgRF95);
    if (!RH_RF95_init())
    {
        chLcdprintfFila(3,"RF95init failed!!");
        osalThreadSleepMilliseconds(2000);
        spiStop(&SPID1);
        palDisableLineEvent(LINE_PA10);
        return;
    }
    RH_RF95_setFrequency(434.0f);
    RH_RF95_setModeRx();
    spiStop(&SPID1);
    chLcdprintfFila(3,"RF95 a 434.0MHz");
}
