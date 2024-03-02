/*
 * rele.cpp
 *
 *  Created on: 28 jun. 2020
 *      Author: jcabe
 */


#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "sms.h"
#include "nextion.h"

/*
 * RELE variableEntrada
 * activa rele de la tarjeta (PB12)
 */
rele::rele(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=2)
    {
        nextion::enviaLog(tty,"#parametros RELE");
        *hayError = 1;
        return; // error
    }
    numInput = estados::addEstado(tty, pars[1], 0, hayError);
};

rele::~rele()
{
}

const char *rele::diTipo(void)
{
    return "RELE";
}

const char *rele::diNombre(void)
{
    return "RELE";
}

int8_t rele::init(void)
{
    return 0;
}

uint8_t rele::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
  //#define LINE_RELE                    PAL_LINE(GPIOB, 12U)
  uint8_t estado;
  estado = estados::diEstado(numInput);
  palWriteLine(LINE_RELE, estado);
  return 0;
}

void rele::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void rele::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"RELE [%s-%d]",estados::nombre(numInput),numInput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void rele::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"Rele:%d",estados::diEstado(numInput));
    smsPtr->addMsgRespuesta(buffer);
}

