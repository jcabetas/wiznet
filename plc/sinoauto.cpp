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
    int16_t numOut, numInput;
    parametroU16Flash *valor;
    uint16_t idPagePic, idNombrePic;
    uint8_t picBase, estadoOld;
 */

extern estados est;
extern programador *programadores[MAXPROGRAMADORES];

extern struct queu_t colaMsgTxWWW;


/*
 * SINOAUTO pruebaSiNo pruebaSNAout.test.pic.12 pruebaSNA.test.pic.12 0 [nombre largo www]
 *           input           salida                     ajuste
 *
 */
sinoauto::sinoauto(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t modo;
    if (numPar!=5 && numPar!=6)
    {
        nextion::enviaLog(tty, "#parametros en SINOAUTO");
        *hayError = 1;
        return; // error
    }
    numInput = estados::addEstado(tty, pars[1], 0, hayError);
    numOut = estados::addEstado(tty, pars[2], 1, hayError);
    modo = 0;
    if (!strcmp(pars[4],"0") || !strcmp(pars[4],"off"))
        modo = 0;
    else if (!strcmp(pars[4],"1") || !strcmp(pars[4],"on"))
        modo = 1;
    else if (!strcmp(pars[4],"2") || !strcmp(pars[4],"auto"))
        modo = 2;
    ajuste = new parametroU16Flash(pars[3], modo,0,2);
    estadoOld = 4;
    estadoAjusteWWW = 4;
    estadoWWW = 4;
};

sinoauto::~sinoauto()
{
}

const char *sinoauto::diTipo(void)
{
    return "sinoauto";
}

const char *sinoauto::diNombre(void)
{
    return nombres::nomConId(ajuste->idNextionVar);
}

int8_t sinoauto::init(void)
{
    switch (ajuste->valor())
    {
    case 0: estados::ponEstado(numOut,0);
            break;
    case 1: estados::ponEstado(numOut,1);
            break;
    case 2: estados::ponEstado(numOut,estados::diEstado(numInput));
            break;
    }
    sinoauto::actualizaNextion();
    estadoOld = estados::diEstado(numOut);
    return 0;
}

uint8_t sinoauto::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    uint8_t statAnterior = estados::diEstado(numOut);
    switch (ajuste->valor())
    {
    case 0: estados::ponEstado(numOut,0);
            break;
    case 1: estados::ponEstado(numOut,1);
            break;
    case 2: estados::ponEstado(numOut,estados::diEstado(numInput));
            break;
    }
    if (statAnterior!=estados::diEstado(numOut))
        return 1;
    else
        return 0;
}

void sinoauto::actualizaNextion(void)
{
    uint16_t picModo;
    picModo = ajuste->valor();
//    if (picModo==2)
//        picModo = estados::diEstado(numInput);
    if (ajuste->idNextionPage!=0)
        enviaValPic(ajuste->idNextionPage, ajuste->idNextionVar,ajuste->tipoNextion, ajuste->picBase, picModo);
//    estados::actualizaNextion(numOut);
}

void sinoauto::trataOrdenNextion(char *vars[], uint16_t numPars)
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
        switch (ajuste->valor())
        {
        case 0: ajuste->set("1");
                break;
        case 1: ajuste->set("2");
                break;
        case 2: ajuste->set("0");
                break;
        }
    }
    if (!strcmp(vars[0],"0") || !strcmp(vars[0],"off") || !strcasecmp(vars[0],"no"))
        ajuste->set("0");
    if (!strcmp(vars[0],"1") || !strcmp(vars[0],"on") || !strcasecmp(vars[0],"si"))
        ajuste->set("1");
    if (!strcmp(vars[0],"2") || !strcmp(vars[0],"auto"))
        ajuste->set("2");
    sinoauto::actualizaNextion();
    cambiosAjusteWWW();
}

void sinoauto::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
    if (estadoOld!=estados::diEstado(numOut))
    {
        sinoauto::actualizaNextion();
        estadoOld = estados::diEstado(numOut);
        cambiosEstadoWWW();
    }
}

void sinoauto::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = SINOAUTO (ajuste:%d, input:%s-%d)",estados::nombre(numOut),numOut,ajuste->valor(),estados::nombre(numInput),numInput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void sinoauto::printStatus(char *buffer, uint8_t longBuffer)
{
    uint8_t estado;
    char estStr[][6]={"off","on","auto"};
    const char *ptrNom = nombres::nomConId(ajuste->idNextionVar);
    switch (ajuste->valor())
    {
    case 0: chsnprintf(buffer,longBuffer,"%s: off",ptrNom);
            break;
    case 1: chsnprintf(buffer,longBuffer,"%s: on",ptrNom);
            break;
    case 2:  estado = estados::diEstado(numOut);
            if (estado<=2)
                chsnprintf(buffer,longBuffer,"%s: auto [%s]",ptrNom,estStr[estado]);
            break;
    }
}

void sinoauto::statusSMS(sms *smsPtr)
{
    char buffer[30];
    printStatus(buffer, sizeof(buffer));
    smsPtr->addMsgRespuesta(buffer);
}

// Llamada en Init: vuelca a JSON
void sinoauto::initWWW(BaseSequentialStream *SDPort, uint8_t *hayDatosVolcados)
{
    /*
    {tipo: 'SINOAUTO',id:'cale',nombre:'Calefaccion',estado: 0,estadoVar: 0}
     */
    if (ajuste->idVarWWW!=0)
    {
        if (*hayDatosVolcados)
            chprintf(SDPort,",");
        chprintf(SDPort,"{\"tipo\":\"SINOAUTO\",\"id\":\"%s\",\"nombre\":\"%s\",\"estado\":%d,\"salida\":%d}",
                   nombres::nomConId(ajuste->idNextionVar),nombres::nomConId(ajuste->idVarWWW),ajuste->valor(),estados::diEstado(numOut));
        *hayDatosVolcados = 1;
        estadoAjusteWWW = ajuste->valor();
        estadoWWW = estados::diEstado(numOut);
        chThdSleepMilliseconds(20);
    }
}

void sinoauto::cambiosEstadoWWW(void)
{
    struct msgTxWWW_t message;
    message.idNombVariable = ajuste->idNextionVar;
    message.accion = TXWWWESTADOVAR;
    //message.valor = estados::diEstado(numOut);
    chsnprintf(message.valor, sizeof(message.valor), "%d",estados::diEstado(numOut));
    putQueu(&colaMsgTxWWW, &message);
}

void sinoauto::cambiosAjusteWWW(void)
{
    struct msgTxWWW_t message;
    message.idNombVariable = ajuste->idNextionVar;
    message.accion = TXWWWESTADO;
    //message.valor = ajuste->valor();
    chsnprintf(message.valor, sizeof(message.valor), "%d",ajuste->valor());
    putQueu(&colaMsgTxWWW, &message);
}

//void sinoauto::cambiosEstadoWWW(void)
//{
//    if (idNombreWWW!=0 && estadoWWW = estados::diEstado(numOut))
//    {
//        //  {"evento":"update","datos":[{"id":"cale","variable":"estadoVar","valor":0}]}
//        chprintf((BaseSequentialStream*)&SD6,"{\"evento\":\"update\",\"datos\":[{\"id\":\"%s\",\"variable\":\"estadoVar\",\"valor\":%d}]}\n\r",
//                   estados::nombre(numOut),estados::diEstado(numOut));
//        estadoWWW = estados::diEstado(numOut);
//    }
//}

//void sinoauto::cambiosAjusteWWW(uint8_t *heVolcado)
//{
//    *heVolcado = 0;
//    if (idNombreWWW!=0 && estadoAjusteWWW != ajuste->valor())
//    {
//        //  {"evento":"update","datos":[{"id":40,"variable":"estado","valor":0}]}
//        chprintf((BaseSequentialStream*)&SD6,"{\"evento\":\"update\",\"datos\":[{\"id\":\"%s\",\"variable\":\"estado\",\"valor\":%d}]}\n\r",
//                 estados::nombre(numOut),ajuste->valor());
//        estadoAjusteWWW = ajuste->valor();
//        *heVolcado = 1;
//    }
//}


