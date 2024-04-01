/*
 * tratamientoMsgRF95.c
 *
 *  Created on: 28/12/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include <RH_RF95.h>
#include "string.h"
#include "chprintf.h"
#include <stdio.h>
#include "colas.h"
#include "lcd.h"
//#include "../radio/pozo.h"
//#include "tipoVars.h"
//#include "bloques.h"
//#include "nextion.h"
#include "radio.h"

extern struct queu_t colaMsgRx;
extern struct queu_t colaMsgTx;
extern event_source_t newMsgRx_source;
extern event_source_t newMsgTx_source;
//extern RTCDateTime dateTimeEnvioAnterior;
extern uint8_t cnt;
//extern varMODOPOZO modopozo;
//extern uint8_t checkTrataMsgRf95;


extern event_source_t rf95int_event;


void initRF95(void);

thread_t *procesoMsgRf95;
extern thread_t *procesoRf95Int;
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







void radio::paraRadio(void)
{
    if (procesoMsgRf95 != NULL)
    {
        chThdTerminate(procesoMsgRf95);
        chThdWait(procesoMsgRf95);
        procesoMsgRf95 = NULL;
    }
    if (procesoRf95Int != NULL)
    {
        chThdTerminate(procesoRf95Int);
        chThdWait(procesoRf95Int);
        procesoRf95Int = NULL;
        palDisableLineEvent(LINE_DIO0);
        spiStop(&SPID3);
        return;
    }
    //BaseSequentialStream *tty, uint16_t idPageLog, uint16_t idNomLog, const char *msg
    chLcdprintfFila(3,"Radio parada");
}

void radio::arrancaRadio(void)
{
    radio::reseteaVariables();
    if (!procesoMsgRf95)
    {
        initRF95();
    }
}

















