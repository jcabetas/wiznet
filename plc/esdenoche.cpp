#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "nextion.h"
#include "sms.h"
#include "calendar.h"
#include "math.h"
#include "string.h"


/*
 * ESDENOCHE output latitud(grad)  longit(grad)
 * ESDENOCHE esdenoche  40.4167 -3.703
 */
esdenoche::esdenoche(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=4)
    {
        nextion::enviaLog(tty,"#parametros incorrecto");
        *hayError = 1;
        return;
    }
    numOut = estados::addEstado(tty, pars[1], 1, hayError);
    latitudRad = M_PI/180.0f*atof(pars[2]);
    longitudRad = M_PI/180.0f*atof(pars[3]);
};

esdenoche::~esdenoche()
{
}

const char *esdenoche::diTipo(void)
{
	return "ESDENOCHE";
}

const char *esdenoche::diNombre(void)
{
	return estados::nombre(numOut);
}

int8_t esdenoche::init(void)
{
    calendar::setLatLong(latitudRad, longitudRad);
    estados::ponEstado(numOut, calendar::esDeNoche());
    return 0;
}

uint8_t esdenoche::calcula(uint8_t , uint8_t , uint8_t seg, uint8_t ds)
{
    if (seg==0 && ds==6)
    {
        if (calendar::esDeNoche())
            estados::ponEstado(numOut, 1);
        else
            estados::ponEstado(numOut, 0);
    }
    return 0; // en estados tiene la cuenta
}

void esdenoche::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void esdenoche::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = ESDENOCHE Lat:%.3f Long:%.3f",estados::nombre(numOut),numOut,latitudRad*180.0f/M_PI,longitudRad*180.0f/M_PI);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}


void esdenoche::statusSMS(sms *smsPtr)
{
    char buffer[50];
    uint16_t posBuffer;
    chsnprintf(buffer,sizeof(buffer),"%s:%d ",estados::nombre(numOut),estados::diEstado(numOut));
    posBuffer = strlen(buffer);
    calendar::printHoras(&buffer[posBuffer], sizeof(buffer)-posBuffer-1);
    smsPtr->addMsgRespuesta(buffer);
}
