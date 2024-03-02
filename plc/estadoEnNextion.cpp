#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "string.h"
#include "stdlib.h"
#include "nextion.h"
#include "sms.h"

/*
    uint16_t idPage, idNombre, numEstado;
    uint8_t picBase, tipoNextion, estadoOld;
 */

extern estados est;
extern programador *programadores[MAXPROGRAMADORES];

/*
 * ESTADO boyaAlta nextionPic.pic.12
 */
estadoEnNextion::estadoEnNextion(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t error;
    if (numPar!=3)
    {
        nextion::enviaLog(tty, "#parametros en ESTADO");
        *hayError = 1;
        return; // error
    }
    numEstado = estados::addEstado(tty, pars[1], 0, hayError);
    error = hallaNombreyDatosNextion(pars[2], TIPONEXTIONSILENCIO, &idNombre, &idNombreWWW, &idPage, &tipoNextion, &picBase);
    if (error)
        *hayError = 1;
};

estadoEnNextion::~estadoEnNextion()
{
}

const char *estadoEnNextion::diTipo(void)
{
    return "ESTADO";
}

const char *estadoEnNextion::diNombre(void)
{
    return "ESTADO";
}

int8_t estadoEnNextion::init(void)
{
    estadoOld = 99;
    return 0;
}


void estadoEnNextion::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
    if (estados::diEstado(numEstado)!=estadoOld)
    {
        enviaValPic(idPage, idNombre, tipoNextion, picBase, estados::diEstado(numEstado));
        estadoOld = estados::diEstado(numEstado);
    }
}

void estadoEnNextion::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"ESTADO [%s-%d]",estados::nombre(numEstado),numEstado);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void estadoEnNextion::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"%s:%d",estados::nombre(numEstado),estados::diEstado(numEstado));
    smsPtr->addMsgRespuesta(buffer);
}
