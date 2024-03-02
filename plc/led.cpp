/*
 * led.cpp
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
 * LED variableEntrada
 * activa Led de la tarjeta (PA1)
 */
LED::LED(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=2)
    {
        nextion::enviaLog(tty,"#parametros LED");
        *hayError = 1;
        return; // error
    }
    numInput = estados::addEstado(tty, pars[1], 0, hayError);
};

LED::~LED()
{
}

const char *LED::diTipo(void)
{
    return "LED";
}

const char *LED::diNombre(void)
{
    return "LED";
}

int8_t LED::init(void)
{
    return 0;
}

uint8_t LED::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
  //#define LINE_LED                    PAL_LINE(GPIOA, 1U)
  uint8_t estado;
  estado = estados::diEstado(numInput);
  palWriteLine(LINE_LED, !estado);
  return 0;
}

void LED::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void LED::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"LED [%s-%d]",estados::nombre(numInput),numInput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void LED::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"Led:%d",estados::diEstado(numInput));
    smsPtr->addMsgRespuesta(buffer);
}



