#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "string.h"
#include "stdlib.h"
#include "nextion.h"
#include "sms.h"
#include "colas.h"

/*
    int16_t numOut;
    parametroU16 *valor;
 */

extern estados est;
extern programador *programadores[MAXPROGRAMADORES];

extern struct queu_t colaMsgTxWWW;
/*
 * SINO pruebaSino.test.pic.12"nombre largo www" 0
 */
sino::sino(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t modo;
    char buff[20];
    if (numPar!=3 && numPar!=4)
    {
        nextion::enviaLog(tty, "#parametros en SINO");
        *hayError = 1;
        return; // error
    }
    modo = 0;
    if (!strcmp(pars[2],"0") || !strcmp(pars[2],"off") || !strcmp(pars[2],"no"))
        modo = 0;
    else if (!strcmp(pars[2],"1") || !strcmp(pars[2],"on") || !strcmp(pars[2],"si"))
        modo = 1;
    valor = new parametroU16Flash(pars[1], modo,0,1);
    // no dejo que el estado vaya a WWW
    strncpy(buff,nombres::nomConId(valor->idNextionVar),sizeof(buff));
    numOut = estados::addEstado(tty, buff, 1, hayError);
    estadoWWW = 4;
};

sino::~sino()
{
}

const char *sino::diTipo(void)
{
    return "sino";
}

const char *sino::diNombre(void)
{
    return nombres::nomConId(valor->idNextionVar);
//    return estados::nombre(numOut);
}

int8_t sino::init(void)
{
    estados::ponEstado(numOut, valor->valor());
    if (valor->idNextionPage!=0)
        enviaValPic(valor->idNextionPage, valor->idNextionVar,valor->tipoNextion, valor->picBase,estados::diEstado(numOut));
    return 0;
}

void sino::trataOrdenNextion(char *vars[], uint16_t numPars)
{
    // @orden,smsOn,toggle
    /*
     *     int16_t numOut;
    parametroU16Flash *valor;
     */
    if (numPars!=1)
        return;
    if (!strcmp(vars[0],"toggle"))
    {
        valor->set(!valor->valor());
        estados::ponEstado(numOut, valor->valor());
    }
    if (!strcmp(vars[0],"1") || !strcasecmp(vars[0],"si"))
    {
        estados::ponEstado(numOut, 1);
        valor->set("1");
    }
    if (!strcmp(vars[0],"0") || !strcasecmp(vars[0],"no"))
    {
        estados::ponEstado(numOut, 0);
        valor->set("0");
    }
    if (valor->idNextionPage!=0)
        enviaValPic(valor->idNextionPage, valor->idNextionVar,valor->tipoNextion, valor->picBase, estados::diEstado(numOut));
    cambiosAjusteWWW();
}


void sino::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{

}

void sino::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = SINO (%d)",estados::nombre(numOut),numOut,valor->valor());
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void sino::printStatus(char *buffer, uint8_t longBuffer)
{
    chsnprintf(buffer,longBuffer,"%s: %s",estados::nombre(numOut),estados::diEstado(numOut)?"si":"no");
}

void sino::statusSMS(sms *smsPtr)
{
    char buffer[30];
    printStatus(buffer, sizeof(buffer));
    smsPtr->addMsgRespuesta(buffer);
}


// Llamada en Init: vuelca a JSON
void sino::initWWW(BaseSequentialStream *SDPort, uint8_t *hayDatosVolcados)
{
    /*
    { tipo: 'SINO', id: 'enCasa', nombre: 'En casa', estado: 1 }
     */
    if (valor->idVarWWW!=0)
    {
        if (*hayDatosVolcados)
            chprintf(SDPort,",");
        chprintf(SDPort, "{\"tipo\":\"SINO\",\"id\":\"%s\",\"nombre\":\"%s\",\"estado\":%d}",
                   estados::nombre(numOut),nombres::nomConId(valor->idVarWWW),valor->valor());
        *hayDatosVolcados = 1;
        estadoWWW = valor->valor();
        chThdSleepMilliseconds(20);
    }
}

void sino::cambiosAjusteWWW(void)
{
    struct msgTxWWW_t message;
    message.idNombVariable = estados::diIdNombre(numOut);
    message.accion = TXWWWESTADO;
    chsnprintf(message.valor, sizeof(message.valor), "%d",valor->valor());
    putQueu(&colaMsgTxWWW, &message);
}


//void sino::cambiosAjusteWWW(void)
//{
//    *heVolcado = 0;
//    if (idNombreWWW!=0 && estadoWWW != valor->valor())
//    {
//        //  {"evento":"update","datos":[{"id":"enCasa","variable":"estado","valor":0}]}
//        chprintf((BaseSequentialStream*)&SD6,"{\"evento\":\"update\",\"datos\":[{\"id\":\"%s\",\"variable\":\"estado\",\"valor\":%d}]}\n\r",
//               estados::nombre(numOut),valor->valor());
//        estadoWWW = valor->valor();
//        *heVolcado = 1;
//    }
//}
