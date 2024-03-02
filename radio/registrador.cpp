/*
 * llamador.c
 *
 *  Created on: 12/12/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"


using namespace chibios_rt;

#include "string.h"
#include "chprintf.h"
#include <stdio.h>
#include "colas.h"
#include "bloques.h"
#include "radio.h"
#include "nextion.h"

uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
int32_t randomNum(int32_t numMin, int32_t numMax);
void int2str(uint8_t valor, char *string);

extern struct queu_t colaMsgTx;
extern event_source_t newMsgTx_source;
extern struct queu_t colaMsgRx;

extern struct msgRx_t ultMsg;
extern event_source_t hayRxParaLCD_source;
extern event_source_t hayCambiosLCD_source;

// REGISTRADOR [sOlv 10] logPozo.pozo.txt bPozo.pozo.pic.0 comOk.pozo.pic.13 llam.pozo.txt act.pozo.txt abu.pozo.txt
registrador::registrador(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t tipoNextion, picBase;
    if (numPar!=8)
    {
        nextion::enviaLog(tty,"#parametros incorrectos REGISTRADOR");
        *hayError = 1;
        return;
    }
    modoRadio = MODOREGISTRADOR;
    sOlvido = parametro::addParametroU16FlashMinMax(tty, pars[1],20,3600,hayError);
    hallaNombreyDatosNextion(pars[2], TIPONEXTIONSTR, &idNombreLog, &idNombreLogWWW, &idPageLog, &tipoNextion, &picBase);
    numEstadoBomba = estados::addEstado(tty, pars[3], 1, hayError);
    numEstadoComOk = estados::addEstado(tty, pars[4], 1, hayError);
    hallaNombreyDatosNextion(pars[5], TIPONEXTIONSILENCIO, &idNombreLlamadores, &idNombreLlamadoresWWW, &idPageLlamadores, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
        nextion::enviaLog(tty,"REGISTRADOR llamadores no txt");
        *hayError = 1;
    }
    // nextion activos
    hallaNombreyDatosNextion(pars[6], TIPONEXTIONSILENCIO, &idNombreActivos, &idNombreActivosWWW, &idPageActivos, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
         nextion::enviaLog(tty,"REGISTRADOR activos no txt");
         *hayError = 1;
    }
    // nextion abusones
    hallaNombreyDatosNextion(pars[7], TIPONEXTIONSILENCIO, &idNombreAbusones, &idNombreAbusonesWWW, &idPageAbusones, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
         nextion::enviaLog(tty,"REGISTRADOR abusones no txt");
         *hayError = 1;
    }
    radioPtr = this;
}



void registrador::reseteaVariablesEspecificas(void)
{
}


/*
 *  Trata la recepci�n de una notificaci�n del pozo
 *  Devuelve 1 si hay cambios
 */
uint8_t registrador::gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg)
{
    uint8_t estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld, petBombaOld;
    estadoLlamacionesOld = estadoLlamaciones;
    estadoActivosOld = bombaPozoOn;
    estadoAbusonesOld = estadoAbusones;
    petBombaOld = bombaPozoOn;
    estadoLlamaciones = estadoLlamacionesMsg;
    estadoActivos = estadoActivosMsg;
    bombaPozoOn = petBombaMsg;
    if (estadoLlamaciones!=estadoLlamacionesOld || estadoActivos!=estadoActivosOld || bombaPozoOn!=petBombaOld)
    {
        ponEnColaRegistrador();     // guardar en SD
        actualizaNextion(estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld, petBombaOld);
        return 1;
    }
    else
        return 0;
}

void registrador::trataRx(struct msgRx_t *msgRx)
{
    uint8_t msgId = msgRx->msg[0];
    uint8_t estProblematica;
    uint16_t numBytes;
    uint8_t bufError[25];
    // Formato del mensaje:
    //    Byte 0: MSG_STATUSPOZO
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 sat�lites llamadores
    //    Byte 2: Estado bomba del Pozo '0' '1'
    //    Byte 3: Array de 8 bits indicando si el Pozo reconoce como activos a cada uno de los llamadores. Para ello el sat�lite ha de transmitir como mucho cada 15 segundos
    //    Byte 4: Array de 8 bits indicando si el Pozo ha recibido una se�al de llamaci�n de un sat�lite. Se distribuye a todo el mundo, para que todos puedan saber la situaci�n
    if (msgId==MSG_STATUSPOZO && msgRx->numBytes==5 && msgRx->msg[1]==0 && (msgRx->msg[2] == '0' || msgRx->msg[2] =='1'))
    {
        uint8_t numEstMsg = msgRx->msg[1];
        uint8_t petBombaMsg = msgRx->msg[2]-'0';
        uint8_t estadoActivosMsg = msgRx->msg[3];
        uint8_t estadoLlamacionesMsg = msgRx->msg[4];
        calendar::getFechaHora(&dateTimeRxPozoAnterior);
        estados::ponEstado(numEstadoComOk, 1);
        estados::actualizaNextion(numEstadoComOk);
        if (numEstMsg==0 && (petBombaMsg==0 || petBombaMsg==1))
        {
            gestionaEstadoPozo(petBombaMsg, estadoLlamacionesMsg, estadoActivosMsg);
            memcpy(&ultMsg, msgRx, sizeof(ultMsg));
            chEvtBroadcast(&hayRxParaLCD_source);
        }
    }
    if (msgId==MSG_STATUSLLAMACIONLOCAL && msgRx->numBytes==3)
    {
        uint8_t numEstMsg = msgRx->msg[1];
        uint8_t petBombaMsg = msgRx->msg[2]-'0';
        // incorpora peticion
        if (numEstMsg>=1 && numEstMsg<=8 && (petBombaMsg==0 || petBombaMsg==1))
        {
            memcpy(&ultMsg, msgRx, sizeof(ultMsg));
            chEvtBroadcast(&hayRxParaLCD_source);
        }
    }
    // Tipo de Mensaje MSG_ERROR. Longitud=variable, de 4 a un maximo de 24 bytes
    // Este mensaje es un beacon que se envia desde el satelite, indicando errores
    //
    // Formato del mensaje:
    //    Byte 0: MSG_ERROR
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
    //    Byte 2: Numero de error
    //    Byte 3: Id estacion con problemas. 0=Pozo, 1 a 8 satelites llamadores
    //    Byte 4: Numero de bytes del texto. Max 20.
    //    Texto variable: de 1 a 21 bytes de texto (incluyendo el terminador 0)
    if (msgId==MSG_ERROR && msgRx->numBytes>=5)
    {
        uint8_t numEst = msgRx->msg[1];
        uint8_t numError = msgRx->msg[2];
        if (numEst!=0 || numError!=1)
            return;
        estProblematica = msgRx->msg[3];
        estadoAbusones |= (1<<(estProblematica-1));
        numBytes = msgRx->msg[4];
        if (numBytes>sizeof(bufError))
            numBytes = sizeof(bufError);
        memcpy(bufError, &msgRx->msg[5],numBytes);
        bufError[numBytes] = 0; // fin de cadena
        actualizoErrorDesdeLlamador(estProblematica, numError, bufError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        chEvtBroadcast(&hayRxParaLCD_source);
    }
    // Tipo de Mensaje MSG_CLEARERROR. Longitud=4
    // Este mensaje lo envia el Pozo si ha desaparecido un error de una estaci�n
    //
    // Formato del mensaje:
    //    Byte 0: MSG_CLEARERROR
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 sat�lites llamadores
    //    Byte 2: Numero de error
    //    Byte 3: Id estacion con problemas resueltos. 0=Pozo, 1 a 8 sat�lites llamadores
    //
    if (msgId==MSG_CLEARERROR && msgRx->numBytes==4)
    {
        uint8_t numEst = msgRx->msg[1];
        uint8_t numError = msgRx->msg[2];
        if (numEst!=0 || numError!=1)
            return;
        estProblematica = msgRx->msg[3];
        estadoAbusones &= ~(1<<(estProblematica-1));
        limpiaError(estProblematica, numError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        chEvtBroadcast(&hayRxParaLCD_source);
    }
}



void registrador::trataRxRf95(eventmask_t evt)
{
    struct msgRx_t msgRx;
    if (evt==0) // timeout
    {
        if (calendar::sDiff(&dateTimeRxPozoAnterior)>sOlvido->valor())
        {
            estados::ponEstado(numEstadoComOk, 0);
        }
    }
    // if (evt & EVENT_MASK(1)) // Tengo que enviar mensajes rf95 (como registrador, no puedo)
    if (evt & EVENT_MASK(0)) // He recibido un mensaje rf95
    {
        while (getQueu(&colaMsgRx, &msgRx))
        {
            if (++cnt>99) cnt=0;
                trataRx(&msgRx);
        }
    }
}


registrador::~registrador()
{
    radioPtr = NULL;
}

const char *registrador::diTipo(void)
{
    return "REGISTRADOR";
}

const char *registrador::diNombre(void)
{
    return "REGISTRADOR";
}

int8_t registrador::init(void)
{
    arrancaRadio();
    return 0;
}

void registrador::stop(void)
{
    paraRadio();
}

void registrador::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void registrador::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"REGISTRADOR bOn:%s-%d commOk:%s-%d",estados::nombre(numEstadoBomba),numEstadoBomba,\
               estados::nombre(numEstadoComOk),numEstadoComOk);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}














