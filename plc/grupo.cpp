#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "sms.h"
#include "nextion.h"
#include "string.h"

/*
    uint16_t idGrupo;
    uint8_t numComponentes;
    uint16_t idNombres[10];
 */

extern estados est;

/*
 * GRUPO deposito boyaAlta boyaBaja boyaMedia boyaOk
 */
grupo::grupo(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar<3 || numPar>12)
    {
        nextion::enviaLog(tty,"#parametros grupo incorrecto");
        *hayError = 1;
        return;
    }
    idGrupo = nombres::incorpora(pars[1]);
    numComponentes = numPar-2;
    if (numComponentes>10)
        numComponentes = 10;
    for (uint8_t comp=0;comp<numComponentes;comp++)
        idNombres[comp] = nombres::incorpora(pars[comp+2]);
};

grupo::~grupo()
{

}

const char *grupo::diTipo(void)
{
	return "GRUPO";
}

const char *grupo::diNombre(void)
{
	return nombres::nomConId(idGrupo);
}

int8_t grupo::init(void)
{
    return 0;
}

void grupo::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void grupo::print(BaseSequentialStream *tty)
{
    char buffer[90];
    chsnprintf(buffer,sizeof(buffer),"[%s] = GRUPO",nombres::nomConId(idGrupo));
    for (uint8_t comp=0;comp<numComponentes;comp++)
        chsnprintf(&buffer[strlen(buffer)],sizeof(buffer)-strlen(buffer)," %s",nombres::nomConId(idNombres[comp]));
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void grupo::statusSMS(sms *smsPtr)
{
    for (int8_t bl=0; bl<numComponentes;bl++)
    {
         smsPtr->procesaStatusYAlgo((char *) nombres::nomConId(idNombres[bl]));
    }
}
