/*
 * pozoComun.c
 *
 *  Created on: 12/12/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include "string.h"
#include "chprintf.h"

#include "colas.h"
#include "radio.h"
#include "calendarUTC.h"

#include <stdlib.h>

thread_t *procesoDisplayPozo, *procesoMsgPozo;
event_source_t enviaLlamacionMsg_source;
event_source_t enviaPozoMsg_source;
event_source_t registraMsgPozo_source;
event_source_t rf95int_event;
event_source_t newMsgRx_source;
event_source_t newMsgTx_source;


extern struct queu_t colaCambiosPozo;

uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
int32_t randomNum(int32_t numMin, int32_t numMax);
uint8_t esValle(void);


extern float  kWhtOld, kW;

event_source_t hayRxParaLCD_source;
event_source_t hayCambiosLCD_source;



/*
 * Registra cambios segun estadoLlamaciones
 */
void radio::registraCambiosPeticion(uint8_t estLlamaciones, uint8_t estLlamacionesOld)
{
    time_t ahora = calendar::getSecUnix();
    for (uint8_t est=0;est<8;est++)
    {
        uint8_t estadoOld = (estLlamacionesOld>>est) & 1;
        uint8_t estadoNew = (estLlamaciones>>est) & 1;
        if (estadoNew && !estadoOld) // empieza a pedir
            timeInicioPeticion[est] = ahora;
        if (!estadoNew && estadoOld) // deja de pedir
        {
            // registro en memoria permanente
            uint32_t tiempoOn = ahora - timeInicioPeticion[est];
//            struct datosPozoGuardados *datos = (struct datosPozoGuardados *) BKPSRAM_BASE;
//            datos->datosId[est].segundosPeticion += tiempoOn;
//            datos->datosId[est].numPeticiones += 1;
        }
    }
}

