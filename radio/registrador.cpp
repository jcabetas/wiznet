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
#include "RH_RF95.h"
#include "radio.h"
#include "calendarUTC.h"
#include "lcd.h"
#include "modbus.h"
#include "externRegistros.h"

#include <stdlib.h>




//uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
//time_t GetTimeUnixSec(void);
int32_t randomNum(int32_t numMin, int32_t numMax);
void int2str(uint8_t valor, char *string);
extern struct queu_t colaMsgRx;
extern event_source_t newMsgRx_source;
extern event_source_t newMsgRx_source;

extern struct msgRx_t ultMsg;

thread_t *procesoRegistrador = NULL;

extern "C"
{
    void registradorInit(void);
}


registrador::registrador(void)
{
    bombaPozoOn = 0;
    calendar::getFechaHora(&dateTimeRxPozoAnterior);
    estadoComms = 0;
}




void registrador::obsoleto(void)
{
    // ha pasado mucho tiempo sin recibir?
    if (calendar::sDiff(&dateTimeRxPozoAnterior)>sTimeOutLlamadoresHR->getValor())
    {
        numEstadoComOk = 0;
    }
}




/*
 *  Trata la recepcion de una notificacion del pozo
 *  Devuelve 1 si hay cambios
 */
uint8_t registrador::gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg)
{
    uint8_t estadoLlamacionesOld, estadoActivosOld, petBombaOld;
    estadoLlamacionesOld = peticionesIR->getValor();
    estadoActivosOld = activosIR->getValor();
    petBombaOld = bombaPozoOn; //estados::diEstado(numInput);//petBomba;
    peticionesIR->setValor(estadoLlamacionesMsg);
    activosIR->setValor(estadoActivosMsg);
    estadoAbusonesOld = abusonesIR->getValor();
    bombaPozoOn = petBombaMsg;
    if (peticionesIR->getValor()!=estadoLlamacionesOld || activosIR->getValor()!=estadoActivosOld || bombaPozoOn!=petBombaOld)
    {
        return 1;
    }
    else
        return 0;
}

void registrador::trataRx(struct msgRx_t *msgRx)
{
    uint8_t msgId = msgRx->msg[0];
    uint8_t estProblematica;
    char buffer[40];
    uint8_t bufError[10];
    struct tm fechHora;
    if (++cntRx>99)
        cntRx = 0;
    // Formato del mensaje:
    //    Byte 0: MSG_STATUSPOZO
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
    //    Byte 2: Estado bomba del Pozo '0' '1'
    //    Byte 3: Array de 8 bits indicando si el Pozo reconoce como activos a cada uno de los llamadores. Para ello el satelite ha de transmitir como mucho cada 15 segundos
    //    Byte 4: Array de 8 bits indicando si el Pozo ha recibido una senyal de llamacion de un satelite. Se distribuye a todo el mundo, para que todos puedan saber la situacion
    if (msgId==MSG_STATUSPOZO && msgRx->numBytes==5 && msgRx->msg[1]==0 && (msgRx->msg[2] == '0' || msgRx->msg[2] =='1'))
    {
        uint8_t numEstMsg = msgRx->msg[1];
        uint8_t petBombaMsg = msgRx->msg[2]-'0';
        uint8_t estadoActivosMsg = msgRx->msg[3];
        uint8_t estadoLlamacionesMsg = msgRx->msg[4];
        calendar::getFechaHora(&dateTimeRxPozoAnterior);
        numEstadoComOk = 1;
        if (numEstMsg==0 && (petBombaMsg==0 || petBombaMsg==1))
        {
            gestionaEstadoPozo(petBombaMsg, estadoLlamacionesMsg, estadoActivosMsg);
            memcpy(&ultMsg, msgRx, sizeof(ultMsg));
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"Pozo B:%d RSSI:%d", petBombaMsg, msgRx->rssi);
            escribeLCD(buffer);
            calendar::printHora(buffer,sizeof(buffer));
            chprintf((BaseSequentialStream *)&SD1,"%s  Pozo Bomba:%d RSSI:%d\n",buffer, petBombaMsg, msgRx->rssi);
//            chLcdprintfFila(0,"%02d:%02d Msg%02d de pozo", fechHora.tm_min, fechHora.tm_sec, getCntRx());
//            chLcdprintfFila(1,"Bomba:%d RSSI:%d", petBombaMsg, msgRx->rssi);
//            chLcdprintfFila(2,"Bomba:%d",petBombaMsg);
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
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"#%d Llam:%d RSSI:%d",numEstMsg,petBombaMsg, msgRx->rssi);
            escribeLCD(buffer);
            calendar::printHora(buffer,sizeof(buffer));
            chprintf((BaseSequentialStream *)&SD1,"%s  #%d Llamacion:%d RSSI:%d\n",buffer,numEstMsg,petBombaMsg, msgRx->rssi);
//            chsnprintf(buffer,sizeof(buffer),"%02d:%02d Msg%02d de #%d",fechHora.tm_min, fechHora.tm_sec, getCntRx(), numEstMsg);
//            chLcdprintfFila(0,buffer);
//            chsnprintf(buffer,sizeof(buffer),"Llamacion:%d RSSI:%d\n", petBombaMsg, msgRx->rssi);
//            chLcdprintfFila(1,buffer);
        }
    }
    // Tipo de Mensaje MSG_ERROR. Longitud=variable, de 4 a un maximo de 24 bytes
    // Este mensaje es un beacon que se envia desde el satelite, indicando extern volatile int16_t _lastRssi;
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
        uint8_t estadoAbusonesOld = abusonesIR->getValor();
        estProblematica = msgRx->msg[3];
        abusonesIR->setValor(abusonesIR->getValor() | (1<<(estProblematica-1)));
        uint8_t numBytes = msgRx->msg[4];
        if (numBytes>sizeof(bufError))
            numBytes = sizeof(bufError);
        memcpy(bufError, &msgRx->msg[5],numBytes);
        bufError[numBytes] = 0; // fin de cadena
        actualizoErrorDesdeLlamador(estProblematica, numError, bufError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        if (abusonesIR->getValor() != estadoAbusonesOld)
        {
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"Abusa #%d msg:'%s'",estProblematica,bufError);
            escribeLCD(buffer);
            chprintf((BaseSequentialStream *)&SD1,"%d Abusa #%d msg:'%s'\n",calendar::getSecUnix(),estProblematica,bufError);
//            chLcdprintfFila(0,"%02d:%02d Msg%02d Abusa #%d", fechHora.tm_min, fechHora.tm_sec, getCntRx(), estProblematica);
//            chLcdprintfFila(1,"msg:'%s'",bufError);
        }
    }
    // Tipo de Mensaje MSG_CLEARERROR. Longitud=4
    // Este mensaje lo envia el Pozo si ha desaparecido un error de una estacion
    //
    // Formato del mensaje:
    //    Byte 0: MSG_CLEARERROR
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
    //    Byte 2: Nemero de error
    //    Byte 3: Id estacion con problemas resueltos. 0=Pozo, 1 a 8 satelites llamadores
    //
    if (msgId==MSG_CLEARERROR && msgRx->numBytes==4)
    {
        uint8_t numEst = msgRx->msg[1];
        uint8_t numError = msgRx->msg[2];
        if (numEst!=0 || numError!=1)
            return;
        uint8_t estadoAbusonesOld = abusonesIR->getValor();
        estProblematica = msgRx->msg[3];
        abusonesIR->setValor(abusonesIR->getValor() & ~(1<<(estProblematica-1)));
        radio::limpiaError(estProblematica, numError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        if (abusonesIR->getValor() != estadoAbusonesOld)
        {
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"Deja de abusar #%d", estProblematica);
            escribeLCD(buffer);
            chprintf((BaseSequentialStream *)&SD1,"%d Deja de abusar #%d\n", calendar::getSecUnix(),estProblematica);
//            chLcdprintfFila(1,"");
        }
    }
}


registrador::~registrador()
{
    paraRadio();
}

const char *registrador::diTipo(void)
{
    return "LLAMADOR";
}

const char *registrador::diNombre(void)
{
    return "LLAMADOR";
}


void registrador::stop(void)
{
    paraRadio();
}


registrador *registradorObj;

/*
 * Gestor registrador
 * Trato mensajes recibidos
 */
static THD_WORKING_AREA(waThreadRegistrador, 2000);
static THD_FUNCTION(ThreadRegistrador, arg) {
    (void)arg;
    struct msgRx_t msgRx;
    event_listener_t el0;
    chRegSetThreadName("registrador");
    chEvtRegister(&newMsgRx_source, &el0, 1);
    while(!chThdShouldTerminateX()) {
        eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
        if (evt == 0)  // timeout
        {
            registradorObj->obsoleto();
            continue;
        }
        if (evt == EVENT_MASK(1))  // he recibido un mensaje, que esta en la cola
        {
            while (getQueu(&colaMsgRx, &msgRx))
            {
                //llamador::llamadorPtr->trataRx(&msgRx);
                registradorObj->trataRx(&msgRx);
            }
        }
    }
}


uint8_t registrador::init(void)
{
    calendar::getFechaHora(&dateTimeRxPozoAnterior);
    if (!procesoRegistrador)
        procesoRegistrador = chThdCreateStatic(waThreadRegistrador, sizeof(waThreadRegistrador), NORMALPRIO + 7,  ThreadRegistrador, NULL);
    return 0;
}

