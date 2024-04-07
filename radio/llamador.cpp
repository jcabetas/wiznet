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

#include <stdlib.h>




//uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
//time_t GetTimeUnixSec(void);
int32_t randomNum(int32_t numMin, int32_t numMax);
void int2str(uint8_t valor, char *string);
extern struct queu_t colaMsgTx;
extern event_source_t newMsgTx_source;
extern struct queu_t colaMsgRx;
extern event_source_t newMsgRx_source;
event_source_t sensor_source;
extern event_source_t newMsgRx_source;

extern struct msgRx_t ultMsg;


uint8_t estadoDeseadoSensor;

extern uint16_t modoRadio;
extern uint16_t sOlvido;
extern uint16_t idLlamador;
extern uint16_t dsMaxEntreMsgsLlamador;
extern uint16_t dsMinEntreMsgsLlamador;


thread_t *procesoLlamador = NULL;
thread_t *procesoSensor = NULL;




extern "C"
{
    void llamadorInit(void);
}



int32_t randomNum(int32_t numMin, int32_t numMax)
{
   //srand((unsigned) time(&t));
   return numMin + (rand() %(numMax-numMin));
}

// LLAMADOR zona3 [idLlam 1] [dsMin 5] [dsMax 100] [sOlvido 30] pozo.logPozo.txt bOn.radio.pic.0 comOk.radio.pic.0 llam.radio.txt act.radio.txt abu.radio.txt
llamador::llamador(void)
{
    modoRadio = MODOLLAMADOR;
    bombaPozoOn = 0;
    bombaPozoSolicitada = 0;
    calendar::getFechaHora(&dateTimeRxPozoAnterior);
    estadoDeseado = 0;
    dsAleatorioMinEntreMsgsLlamador = 2;
    dsAleatorioMaxEntreMsgsLlamador = 6;
    //radioPtr = this;
    estadoComms = 0;
}



void llamador::reseteaVariablesEspecificas(void)
{
    dsAleatorioMinEntreMsgsLlamador= dsMinEntreMsgsLlamador + randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
    dsAleatorioMaxEntreMsgsLlamador= dsMaxEntreMsgsLlamador + randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
}


/*
 * Envia MSG_STATUSLLAMACIONLOCAL
 * Byte 0: MSG_STATUSLLAMACIONLOCAL
 * Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
 * Byte 2: Estado llamacion local '0' '1'
 * Llamador hace su peticien de bomba
 */
void llamador::enviaStatusLlamacion(void)
{
    char buffer[25];
    struct msgTx_t msgTx;
    msgTx.numBytes = 3;
    msgTx.msg[0] = MSG_STATUSLLAMACIONLOCAL;
    msgTx.msg[1] = idLlamador;    //idLlamadorValor();
    if (estadoDeseado) // peticion activacion?
        msgTx.msg[2] = '1';
    else
        msgTx.msg[2] = '0';
    putQueu(&colaMsgTx, &msgTx);
    chEvtBroadcast(&newMsgTx_source);
    if (++cntTx>99)
        cntTx = 0;
    calendar::getFechaHora(&dateTimeEnvioAnterior);
    chsnprintf(buffer,sizeof(buffer),"Envio llamacion:%d",estadoDeseado);
    escribeLCD(buffer);
}



void llamador::obsoleto(void)
{
    // ha pasado mucho tiempo sin recibir?
    if (calendar::sDiff(&dateTimeRxPozoAnterior)>sOlvido)
    {
        numEstadoComOk = 0;
    }
    // habiamos enviado el estado deseado a pozo?
    if (estadoDeseado != bombaPozoSolicitada)
    {
        update(estadoDeseado);
    }
    // tengo que refrescar datos al pozo?
    if (calendar::dsDiff(&dateTimeEnvioAnterior) > dsAleatorioMaxEntreMsgsLlamador)
    {
        enviaStatusLlamacion();
        dsAleatorioMaxEntreMsgsLlamador= dsMaxEntreMsgsLlamador + randomNum(0,10);
    }
}




/*
 *  Trata la recepcion de una notificacion del pozo
 *  Devuelve 1 si hay cambios
 */
uint8_t llamador::gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg)
{
    uint8_t estadoLlamacionesOld, estadoActivosOld, petBombaOld;
    estadoLlamacionesOld = estadoLlamaciones;
    estadoActivosOld = estadoActivos;
    petBombaOld = bombaPozoOn; //estados::diEstado(numInput);//petBomba;
    estadoLlamaciones = estadoLlamacionesMsg;
    uint8_t miId = idLlamador;      // Id llamador
    uint8_t estoyConectadoOld = (estadoActivos>>(miId-1)) & 1;
    estadoActivos = estadoActivosMsg;
    estadoAbusonesOld = estadoAbusones;
    bombaPozoOn = petBombaMsg;
    uint8_t yoEstabaPidiendoMsg = (estadoLlamacionesMsg>>(miId-1)) & 1;
    uint8_t estoyConectadoMsg = (estadoActivosMsg>>(miId-1)) & 1;
    // miro si el pozo tiene mal mi peticion y le tengo que informar de mi estado, o bien dice que no existo
    if ((yoEstabaPidiendoMsg!=bombaPozoSolicitada || !estoyConectadoMsg))
    {
        if (calendar::dsDiff(&dateTimeEnvioAnterior)>(uint32_t)dsAleatorioMinEntreMsgsLlamador)
        {
            enviaStatusLlamacion();
            dsAleatorioMinEntreMsgsLlamador= dsMinEntreMsgsLlamador+randomNum(0,10);
        }
    }
    if (estoyConectadoMsg!=estoyConectadoOld)
    {
        numEstadoComOk = estoyConectadoMsg;
    }
    if (estadoLlamaciones!=estadoLlamacionesOld || estadoActivos!=estadoActivosOld || bombaPozoOn!=petBombaOld)
    {
        return 1;
    }
    else
        return 0;
}

void llamador::trataRx(struct msgRx_t *msgRx)
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
        uint8_t estadoAbusonesOld = estadoAbusones;
        estProblematica = msgRx->msg[3];
        estadoAbusones |= (1<<(estProblematica-1));
        uint8_t numBytes = msgRx->msg[4];
        if (numBytes>sizeof(bufError))
            numBytes = sizeof(bufError);
        memcpy(bufError, &msgRx->msg[5],numBytes);
        bufError[numBytes] = 0; // fin de cadena
        actualizoErrorDesdeLlamador(estProblematica, numError, bufError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        if (estadoAbusones != estadoAbusonesOld)
        {
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"Abusa #%d msg:'%s'",estProblematica,bufError);
            escribeLCD(buffer);
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
        uint8_t estadoAbusonesOld = estadoAbusones;
        estProblematica = msgRx->msg[3];
        estadoAbusones &= ~(1<<(estProblematica-1));
        radio::limpiaError(estProblematica, numError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        if (estadoAbusones != estadoAbusonesOld)
        {
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"Deja de abusar #%d", estProblematica);
            escribeLCD(buffer);
//            chLcdprintfFila(1,"");
        }
    }
}


llamador::~llamador()
{
    paraRadio();
}

const char *llamador::diTipo(void)
{
    return "LLAMADOR";
}

const char *llamador::diNombre(void)
{
    return "LLAMADOR";
}


void llamador::stop(void)
{
    paraRadio();
}

void llamador::update(uint8_t estadoDeseadoPar)
{
    estadoDeseado = estadoDeseadoPar;
    if (estadoDeseado != bombaPozoSolicitada)
    {
        if (calendar::dsDiff(&dateTimeEnvioAnterior)>(uint32_t)dsAleatorioMinEntreMsgsLlamador)
        {
            bombaPozoSolicitada = estadoDeseado;
            enviaStatusLlamacion();
            dsAleatorioMinEntreMsgsLlamador= dsMinEntreMsgsLlamador+randomNum(0,10);
        }
    }
}

/*
 * Gestor sensor
 * Comprueba estado que dure mas de 500ms y notifica cambio
 */
static THD_WORKING_AREA(waThreadSensor, 256);
static THD_FUNCTION(ThreadSensor, arg) {
    (void)arg;
    uint16_t msNuevoEstado;
    uint8_t nuevoEstado;
    chRegSetThreadName("sensor");
    nuevoEstado = !palReadLine(LINE_SENSOR);
    msNuevoEstado = 0;
    while(!chThdShouldTerminateX()) {
        nuevoEstado = !palReadLine(LINE_SENSOR);
        if (nuevoEstado != estadoDeseadoSensor)
        {
            msNuevoEstado += 100;
            if (msNuevoEstado > 500)
            {
                estadoDeseadoSensor = nuevoEstado;
                chLcdprintfFila(3,"Llamacion:%d",estadoDeseadoSensor);
                msNuevoEstado = 0;
                chEvtBroadcast(&sensor_source);
            }
        }
        else
            msNuevoEstado = 0;
        chThdSleepMilliseconds(100);
    }
}

llamador *llamadorObj;

/*
 * Gestor llamacion
 * Espero cambios en sensor (para enviar estado) y trato mensajes recibidos
 */
static THD_WORKING_AREA(waThreadLlamador, 2000);
static THD_FUNCTION(ThreadLlamador, arg) {
    (void)arg;
    struct msgRx_t msgRx;
    event_listener_t el0, el1;
    chRegSetThreadName("llamador");
    chEvtRegister(&sensor_source, &el0, 1);
    chEvtRegister(&newMsgRx_source, &el1, 2);
    while(!chThdShouldTerminateX()) {
        eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
        if (evt == 0)  // timeout
        {
            llamadorObj->obsoleto();
            continue;
        }
        if (evt == EVENT_MASK(1))  // ha cambiado el sensor, enviamos a cola de transmision
        {
            llamadorObj->update(estadoDeseadoSensor); //enviaStatusLlamacion();
            continue;
        }
        if (evt == EVENT_MASK(2))  // he recibido un mensaje, que esta en la cola
        {
            while (getQueu(&colaMsgRx, &msgRx))
            {
                //llamador::llamadorPtr->trataRx(&msgRx);
                llamadorObj->trataRx(&msgRx);
            }
        }
    }
}


uint8_t llamador::init(void)
{
    modo = Llamador;
    calendar::getFechaHora(&dateTimeRxPozoAnterior);
    if (!procesoLlamador)
        procesoLlamador = chThdCreateStatic(waThreadLlamador, sizeof(waThreadLlamador), NORMALPRIO + 7,  ThreadLlamador, NULL);
    if (!procesoSensor)
        procesoSensor = chThdCreateStatic(waThreadSensor, sizeof(waThreadSensor), NORMALPRIO + 7,  ThreadSensor, NULL);
    return 0;
}

