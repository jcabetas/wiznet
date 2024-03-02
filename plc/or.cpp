/*
 * OR.cpp
 *
 *  Created on: 4 abr. 2020
 *      Author: joaquin
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "string.h"

#include "bloques.h"
#include "nextion.h"
#include "sms.h"

extern estados est;

/*
 * OR  incoherenciaTB  BoyaTope  noBoyaBaja
 */
OR::OR(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar<4 || numPar>5)
    {
        nextion::enviaLog(tty,"#parametros OR");
        *hayError = 1;
        return; // error
    }
    numOut = estados::addEstado(tty, pars[1], 1, hayError);
    for (uint8_t i=0;i<4;i++)
        numInputs[i] = 0;
    if (numOut==0)
        return;
    for (uint8_t i=2;i<numPar;i++)
        numInputs[i-2] = estados::addEstado(tty, pars[i], 0, hayError);
};

OR::~OR()
{
}

const char *OR::diTipo(void)
{
    return "OR";
}

const char *OR::diNombre(void)
{
    return estados::nombre(numOut);
}

int8_t OR::init(void)
{
    estados::ponEstado(numOut, 0);
    return 0;
}

uint8_t OR::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    uint8_t estat = 0;
    for (uint16_t i=0;i<4;i++)
        if (numInputs[i]>0)
            estat |= estados::diEstado(numInputs[i]);
    estados::ponEstado(numOut, estat);
    return 0;
}

void OR::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
    estados::actualizaNextion(numOut);
}

void OR::print(BaseSequentialStream *tty)
{
    char buffer[80],buff2[30];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = OR ",estados::nombre(numOut),numOut);
    for (uint8_t i=0;i<4;i++)
        if (numInputs[i]>0)
        {
            chsnprintf(buff2,sizeof(buff2),"[%s-%d]",estados::nombre(numInputs[i]),numInputs[i]);
            strncat(buffer,buff2,sizeof(buffer)-1);
        }
    if (tty!=NULL)
        nextion::enviaLog(tty,buffer);
}

void OR::statusSMS(sms *smsPtr)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"%s:%d", estados::nombre(numOut), estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
