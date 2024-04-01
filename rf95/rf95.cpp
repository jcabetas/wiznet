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

event_source_t rf95int_source;
extern event_source_t newMsgRx_source;
extern event_source_t newMsgTx_source;
thread_t *procesoRf95Int, *procesoRf95rx;

extern volatile int16_t _lastRssi;
extern volatile uint8_t _bufLen;
extern uint8_t _buf[RH_RF95_MAX_PAYLOAD_LEN];

extern uint16_t modoRadio;
extern llamador *llamadorObj;
extern pozo *pozoObj;


//extern volatile uint8_t _bufLen;
//extern uint8_t _buf[RH_RF95_MAX_PAYLOAD_LEN];
//
//extern uint16_t modoRadio;
//extern llamador *llamadorObj;
//extern pozo *pozoObj;errores

//extern uint8_t checkRf95int;


time_t GetTimeUnixSec(void);
uint8_t putQueu(struct queu_t *colaMed, void *ptrStructOrigen);
uint8_t getQueu(struct queu_t *colaMed, void *ptrStructDestino);
void ponEnLCD(uint8_t fila, char const msg[]);

extern struct queu_t colaMsgRx, colaMsgTx;

/*
 * Interrupciones rf95. Activa proceso rf95int para leer datos
 */
static void f2_cb(void *arg) {
  (void)arg;
  uint8_t estDIO0 = palReadLine(LINE_DIO0);
  chSysLockFromISR();
  // activa gestor interrupciones
  chEvtBroadcastI(&rf95int_source);
  chSysUnlockFromISR();
}



///*
// * rf95int thread.
// * gestiona interrupciones
// * lee datos de rf95 y los almacena en cola de mensajes
// */
//static THD_WORKING_AREA(rf95int_wa, 2000);
//static THD_FUNCTION(rf95int, p) {
//  (void)p;
//  event_listener_t el0;
//  paquete_t tipoPaq;
//  struct msgRx_t msgRx;
//  chRegSetThreadName("rf95int");
//  chEvtRegister(&rf95int_event, &el0, 0);
//  while(!chThdShouldTerminateX()) {
//    eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
////    checkRf95int = 1;
//    if (chThdShouldTerminateX())
//        chThdExit((msg_t) 1);
//    if (evt == 0)  // timeout
//        continue;
//    RH_RF95_send(msgTx.msg,msgTx.numBytes);
//    RHGenericDriver_waitPacketSent(100);
//    RH_RF95_setModeRx();
//
//    tipoPaq = RH_RF95_handleInterrupt();
//    RH_RF95_setModeRx();
//    if (tipoPaq==paqRx)
//    {
//        uint8_t size = _bufLen-4;
//        if (size>sizeof(msgRx.msg))
//            size = sizeof(msgRx.msg);
//        msgRx.timet = calendar::getSecUnix();
//        msgRx.numBytes = size;
//        msgRx.rssi = _lastRssi;
//        memcpy(msgRx.msg, &_buf[4], size);
//        putQueu(&colaMsgRx, &msgRx);
//        chEvtBroadcast(&newMsgRx_source);
//        _bufLen = 0;
//    }
//  }
//  chEvtUnregister(&rf95int_event, &el0);
//  procesoRf95rx = NULL;
//}

/*
 * Trata mensajes recibidos por rf95     (en la colaRx)
 * Envia mensajes que pidan ser enviados (en la colaTx)
 * Llama cada segundo a rutinas para vigilar cambios
 */
static THD_WORKING_AREA(trataRf95_wa,2000);
static THD_FUNCTION(trataRf95, p) {
    (void)p;
    struct msgRx_t msgRx;
    struct msgTx_t msgTx;
    event_listener_t newMsgTx_lis, rf95int_lis;
    paquete_t tipoPaq;

    chRegSetThreadName("trataRf95");
    chEvtRegister(&rf95int_source, &rf95int_lis, 1);
    chEvtRegister(&newMsgTx_source, &newMsgTx_lis, 2);
    RH_RF95_setModeRx();
    //    msMaxEntreMsgsLlamador = randomNum(100*dsMaxEntreMsgsLlamadorValor()-2000,100*dsMaxEntreMsgsLlamadorValor());
//    chEvtRegister(&newMsgRx_source, &newMsgRx_lis, EVENT_MASK(0));  //
    do {
        eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(1000));
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
        if (evt==0) // timeout, llamo a rutinas
        {
            switch (modoRadio)
            {
            case 1:
                if (llamadorObj)
                    llamadorObj->trataObsoletoLlamador();
                break;
            case 2:
                if (pozoObj)
                    pozoObj->trataObsoletoPozo();
                break;
            }
            continue;
        }
        if (evt==EVENT_MASK(1)) // hay una interrupcion, vamos a ver si he recibido algo por radio
        {
            uint8_t estDIO0 = palReadLine(LINE_DIO0);
            tipoPaq = RH_RF95_handleInterrupt();
            uint8_t estDIO01 = palReadLine(LINE_DIO0);
            RH_RF95_setModeRx();
            uint8_t estDIO02 = palReadLine(LINE_DIO0);
            if (tipoPaq==paqRx)    // hemos recibido un mensaje, lo enviamos a la cola de recepcion
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
                chLcdprintfFila(2,"en rf95 msg de %d",msgRx.msg[1]);
                _bufLen = 0;
            }
            continue;
        }
        if (evt == EVENT_MASK(2)) // Tengo que enviar mensajes rf95, en colaMsgTx
        {
            while (getQueu(&colaMsgTx, &msgTx))
            {
                RH_RF95_send(msgTx.msg,msgTx.numBytes);
                RHGenericDriver_waitPacketSent(100);
                RH_RF95_setModeRx();
                chLcdprintfFila(2,"en rf95 envio msg");
            }
            continue;
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
    if (!RH_RF95_init())
    {
        chLcdprintfFila(3,"RF95init failed!!");
        osalThreadSleepMilliseconds(2000);
        spiStop(&SPID3);
        palDisableLineEvent(LINE_DIO0);
        return;
    }
    RH_RF95_setFrequency(434.0f);
    RH_RF95_setModeRx();
    if (!procesoRf95Int)
        procesoRf95Int = chThdCreateStatic(trataRf95_wa, sizeof(trataRf95_wa), NORMALPRIO + 7,  trataRf95, NULL);
    chLcdprintfFila(3,"RF95 a 434.0MHz");
}
