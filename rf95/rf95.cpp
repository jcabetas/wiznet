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
#include "radio.h"
#include <RH_RF95.h>

using namespace chibios_rt;


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


event_source_t newMsgRx_source;
event_source_t newMsgTx_source;

event_source_t rf95int_source;
thread_t *procesoRf95Int = NULL;
thread_t *procesoRf95rx = NULL;

extern uint16_t modoRadio;
extern llamador *llamadorObj;
extern pozo *pozoObj;


RH_RF95 rf95;


time_t GetTimeUnixSec(void);
void ponEnLCD(uint8_t fila, char const msg[]);

extern struct queu_t colaMsgRx, colaMsgTx;

/*
 * Interrupciones rf95. Activa proceso rf95int para leer datos
 */
static void f2_cb(void *arg) {
    (void)arg;
    chSysLockFromISR();
    chEvtBroadcastI(&rf95int_source);   //para que procese la interrupcion
    chSysUnlockFromISR();
}


void procesaRx(void)
{
    struct msgRx_t msgRx;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t bufLen = sizeof(buf);
    if (rf95.recv(buf, &bufLen))
    {
        uint8_t size = bufLen;
        if (size>sizeof(msgRx.msg))
          size = sizeof(msgRx.msg);
        msgRx.timet = calendar::getSecUnix();
        msgRx.numBytes = size;
        msgRx.rssi = rf95._lastRssi;
        memcpy(msgRx.msg, buf, size);
        putQueu(&colaMsgRx, &msgRx);
        bufLen = 0;
        chEvtBroadcast(&newMsgRx_source);
    }
}


/*
 * Si hay una interrupcion de rf95, la proceso. Si es mensaje recibido, lo lee y guarda en cola
 * Si hay que enviar algo, lee de la cola y envia
 */
static THD_WORKING_AREA(trataRf95_wa,2000);
static THD_FUNCTION(trataRf95, p) {
    (void)p;

    struct msgTx_t msgTx;
    event_listener_t newMsgTx_lis, rf95int_lis;

    chRegSetThreadName("trataRf95");
    chEvtRegister(&rf95int_source, &rf95int_lis, 1);
    chEvtRegister(&newMsgTx_source, &newMsgTx_lis, 2);
    do {
        eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(50));
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
        if (evt==0) // timeout, llamo a rutinas
        {
            // hemos recibido algo ? (por si las moscas)
            if (rf95.available())
                procesaRx();
//            radio::obsoleto();
//            switch (modoRadio)
//            {
//            case 1:
//                if (llamadorObj)
//                    llamadorObj->trataObsoletoLlamador();
//                break;
//            case 2:
//                if (pozoObj)
//                    pozoObj->trataObsoletoPozo();
//                break;
//            }
            continue;
        }
        if (evt==EVENT_MASK(1)) // hay una interrupcion de rf95, la procesamos
        {
            rf95.handleInterrupt();
            if (rf95.available()) // nuevo, cambia a recepcion de cualquier forma
               procesaRx();
        }
        if (evt == EVENT_MASK(2)) // Tengo que enviar mensajes rf95, en colaMsgTx
        {
            if(getQueu(&colaMsgTx, &msgTx))
            {
                rf95.send(msgTx.msg,msgTx.numBytes);
            }
        }
    } while (1==1);
}

void initSpi3Pins(void)
{
    palClearLine(LINE_SPI3_SCK);
    palClearLine(LINE_SPI3_MISO);
    palClearLine(LINE_SPI3_MOSI);
    palSetLineMode(LINE_SPI3_SCK,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* SPI SCK.             */
    palSetLineMode(LINE_SPI3_MISO,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MISO.                */
    palSetLineMode(LINE_SPI3_MOSI,
                     PAL_MODE_ALTERNATE(6) |
                     PAL_STM32_OSPEED_HIGHEST);         /* MOSI.                */
}


void initRF95(void)
{
    if (procesoRf95Int!=NULL) // debe haber sido arrancado antes
        return;
    initSpi3Pins();
    spiStart(&SPID3, &spicfgRF95);
    palSetLine(LINE_NSS);
    palSetLineMode(LINE_NSS, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(LINE_RST_RFM, PAL_MODE_OUTPUT_PUSHPULL);
    palClearLine(LINE_RST_RFM);
    chThdSleepMilliseconds(2);
    palSetLine(LINE_RST_RFM);
    chThdSleepMilliseconds(15);
    palSetLineMode(LINE_DIO0, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_DIO0, PAL_EVENT_MODE_RISING_EDGE);
    palSetLineCallback(LINE_DIO0, f2_cb, NULL);
    if (!rf95.init())
    {
        chLcdprintfFila(3,"RF95init failed!!");
        osalThreadSleepMilliseconds(2000);
        spiStop(&SPID3);
        palDisableLineEvent(LINE_DIO0);
        return;
    }
    rf95.setFrequency(434.0f);
    if (!procesoRf95Int)
        procesoRf95Int = chThdCreateStatic(trataRf95_wa, sizeof(trataRf95_wa), NORMALPRIO + 7,  trataRf95, NULL);
    chLcdprintfFila(3,"RF95 a 434.0MHz");
}
