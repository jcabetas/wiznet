#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "string.h"

#include "bloques.h"
#include "nextion.h"


extern uint8_t hayCambios;

bloque *bloque::logicHistory[MAXBLOQUES];
uint16_t bloque::numBloques = 0;

bloque::bloque()
{
    logicHistory[numBloques] = this;
    if (numBloques>=MAXBLOQUES-1)
    {
        nextion::enviaLog(NULL,"Demasiados bloques");
        chThdSleepMilliseconds(500);
    }
    numBloques++;
}

bloque::~bloque()
{
    //printf("Borrando bloque\n\r");
}

void bloque::deleteAll(void)
{
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
        if (logicHistory[blq]!=NULL)
        {
            delete logicHistory[blq];
            logicHistory[blq] = NULL;
        }
    bloque::numBloques = 0;
}


int8_t bloque::initBloques()
{
    uint8_t hayFallo = 0;
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
        if (logicHistory[blq]->init()!=0)
            hayFallo = 1;;
    return hayFallo;
}

void bloque::stop(void)
{
}

void bloque::stopBloques(void)
{
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
        logicHistory[blq]->stop();
}


uint16_t bloque::numero()
{ 
    return bloque::numBloques;
}

bloque *bloque::findBloque(char *nomBloque)
{
    uint16_t idPage, idNombre, idNombreWWW, numBlq;
    uint8_t tipoNextion, picBase, blqFound;
    const char *ptrNomBuscado;
    hallaNombreyDatosNextion(nomBloque, TIPONEXTIONSILENCIO, &idNombre, &idNombreWWW, &idPage, &tipoNextion, &picBase);
    ptrNomBuscado = nombres::nomConId(idNombre);
    blqFound = 0;
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
    {
        if (!strcasecmp(ptrNomBuscado,logicHistory[blq]->diNombre()))
        {
            if (blqFound>0) // duplicado?
            {
                blqFound = 2;
                // TODO: enviar mensaje de error y salir
            }
            else
            {
                blqFound = 1;
                numBlq = blq;
            }
        }
    }
    if (blqFound==1)
        return logicHistory[numBlq];
    else
        return NULL;
}

void bloque::procesaStatusBloque(char *nomBloque, sms *smsPtr)
{
    uint16_t idPage, idNombre, idNombreWWW;
    uint8_t tipoNextion, picBase;
    const char *ptrNomBuscado;
    hallaNombreyDatosNextion(nomBloque, TIPONEXTIONSILENCIO, &idNombre, &idNombreWWW, &idPage, &tipoNextion, &picBase);
    ptrNomBuscado = nombres::nomConId(idNombre);
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
    {
        if (!strncasecmp(ptrNomBuscado,logicHistory[blq]->diNombre(),strlen(ptrNomBuscado)))
        {
            logicHistory[blq]->statusSMS(smsPtr);
        }
    }
}

uint8_t bloque::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    return 0;
}


void bloque::trataOrdenNextion(char **, uint16_t )
{
}

void bloque::ordenNextionBlq(char *vars[],uint16_t numPars)
{
    bloque *blq = bloque::findBloque(vars[1]);
    if (blq!=NULL && numPars>2)
        blq->trataOrdenNextion(&vars[2],numPars-2);
}


void bloque::addTimeBloques(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds)
{
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
        logicHistory[blq]->addTime(dsInc, hora, min, seg, ds);
}

void bloque::printBloques(BaseSequentialStream *tty)
{
    char buffer[80];
    if (bloque::numBloques>0)
    {
        chsnprintf(buffer,sizeof(buffer),"Hay %d bloques, empezando en %X",bloque::numBloques,logicHistory[0]);
        nextion::enviaLog(tty, buffer);
    }
    else
        nextion::enviaLog(tty,"No hay bloques definidos");
    for (int16_t blq=0;blq<bloque::numBloques;blq++)
        logicHistory[blq]->print(tty);
}

void bloque::printStatus(char *buffer, uint16_t )
{
    buffer[0] = 0;
}

void bloque::statusSMS(sms *)
{
}

void bloque::initWWW(BaseSequentialStream *, uint8_t *)
{
}

void bloque::initWWW(BaseSequentialStream *SDPort)
{
    // Vuelca por SD6 valores iniciales
    uint8_t hayDatosVolcados=0;
    chprintf(SDPort, "{\"evento\":\"init\",\"datos\": [");
    // vuelca bloques
    for (uint8_t i = 0; i < bloque::numBloques; i++)
       logicHistory[i]->initWWW(SDPort,&hayDatosVolcados);
    // vuelca estados
    for (uint16_t est = 1; est <= estados::numEstados; est++)
       estados::initWWW(SDPort, est, &hayDatosVolcados);
    chprintf((BaseSequentialStream*)&SD6, "]}\n\r");
}

int8_t bloque::actualizaBloques(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds)
{
    int8_t numIter, hayCambiosEnCalculo, hayCambiosOld;
    numIter = 0;
    hayCambiosEnCalculo = 0;
    hayCambiosOld = hayCambios;
    do
    {
        hayCambios = 0;
        for (uint8_t i = 0; i < bloque::numBloques; i++)
            logicHistory[i]->calcula(hora, min, seg, ds);
        if (hayCambios)
            hayCambiosEnCalculo = 1;
    } while (hayCambios && ++numIter < 10);
    hayCambios = hayCambiosEnCalculo || hayCambiosOld;
    return numIter;
}

