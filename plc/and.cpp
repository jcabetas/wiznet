#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "string.h"
#include "bloques.h"
#include "nextion.h"
#include "sms.h"

/*
 * AND  incoherenciaTB  BoyaTope  noBoyaBaja
 */
add::add(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar<4 || numPar>5)
    {
        if (tty!=NULL)
            chprintf(tty,"#parametros incorrecto\n\r");
        else
            nextion::enviaLog(tty,"#parametros AND");
        *hayError = 1;
        return; // error
    }
    numOut = estados::addEstado(tty, pars[1],1, hayError);
    for (uint8_t i=0;i<4;i++)
        numInputs[i] = 0;
    if (numOut==0)
        return;
    for (uint8_t i=2;i<numPar;i++)
        numInputs[i-2] = estados::addEstado(tty, pars[i],0, hayError);
};

add::~add()
{

}

const char *add::diTipo(void)
{
    return "and";
}

const char *add::diNombre(void)
{
    return estados::nombre(numOut);
}

int8_t add::init(void)
{
    estados::ponEstado(numOut, 0);
    return 0;
}

uint8_t add::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    uint8_t estat = 1;
    for (uint16_t i=0;i<4;i++)
        if (numInputs[i]>0)
            estat &= estados::diEstado(numInputs[i]);//est[numInputs[i]];
    estados::ponEstado(numOut, estat);
    return 0; // ya tiene la cuenta en estados
}

void add::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t  )
{
    estados::actualizaNextion(numOut);
}

void add::print(BaseSequentialStream *tty)
{
    char buffer[80],buff2[30];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = AND ",estados::nombre(numOut),numOut);
    for (uint8_t i=0;i<4;i++)
        if (numInputs[i]>0)
        {
            chsnprintf(buff2,sizeof(buff2),"[%s-%d]",estados::nombre(numInputs[i]),numInputs[i]);
            strncat(buffer,buff2,sizeof(buffer)-1);
        }
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}


void add::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"%s:%d",estados::nombre(numOut),estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
