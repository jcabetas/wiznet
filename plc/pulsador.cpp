/*
 * pulsador.cpp
 *
 *  Created on: 28 jun. 2020
 *      Author: jcabe
 */


/*
 *  Created on: 4 abr. 2020
 *      Author: joaquin
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "sms.h"
#include "nextion.h"

/*
 * PULSADOR  boton
 */
pulsador::pulsador(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=2)
    {
        nextion::enviaLog(tty,"#parametros PULSADOR");
        *hayError = 1;
        return; // error
    }
    numOutput = estados::addEstado(tty, pars[1], 1, hayError);
};

pulsador::~pulsador()
{
}

const char *pulsador::diTipo(void)
{
    return "Pulsador";
}

const char *pulsador::diNombre(void)
{
    return estados::nombre(numOutput);
}

int8_t pulsador::init(void)
{
    estados::ponEstado(numOutput, 0);
    return 0;
}

uint8_t pulsador::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
// LINE_PULSEXT
  uint8_t estado;
  estado = palReadLine(LINE_PULSEXT);
  estados::ponEstado(numOutput, !estado);
  return 0;
}

void pulsador::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void pulsador::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = PULSADOR",estados::nombre(numOutput),numOutput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void pulsador::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"Pulsador:%d",estados::diEstado(numOutput));
    smsPtr->addMsgRespuesta(buffer);
}


