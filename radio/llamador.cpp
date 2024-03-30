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
//#include "../radio/pozo.h"
//#include "tipoVars.h"
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

extern struct msgRx_t ultMsg;

event_source_t sensor_source;
uint8_t estadoDeseado;

extern uint16_t modoRadio;
extern uint16_t sOlvido;
extern uint16_t idLlamador;
extern uint16_t dsMaxEntreMsgsLlamador;
extern uint16_t dsMinEntreMsgsLlamador;

//extern event_source_t hayRxParaLCD_source;
//extern event_source_t hayCambiosLCD_source;

static const SPIConfig spicfgRF95 = {
    .circular         = false,
    .slave            = false,
    .data_cb          = NULL,
    .error_cb         = NULL,
    .ssport           = GPIOB,
    .sspad            = GPIOB_NSS,
    .cr1              = SPI_CR1_BR_1 | SPI_CR1_BR_0,
    .cr2              = 0U
};

thread_t *procesoLlamador;
extern "C"
{
    void llamadorInit(void);
}



int32_t randomNum(int32_t numMin, int32_t numMax)
{
   //srand((unsigned) time(&t));

   return numMin + (rand() %(numMax-numMin));
}

//uint8_t estadoBombaRequeridobah(void)
//{
//    //return palReadPad(GPIOE, 0);
//    estados::diEstado(numInput);
//    return peticionAgua.getValorNum();
//}

/*
 *   Los cambios se detectan por los mensajes del pozo
 *   Si hay cambios en estados, graba en SD y resetea
 */
/*
 *     uint16_t numInput;
    uint8_t bombaPozoOn;
    int32_t msAleatorioMinEntreMsgsLllamador;
    int32_t msAleatorioMaxEntreMsgsLllamador;
    RTCDateTime dateTimeEnvioAnterior;
    time_t timeUltConexion[NUMSATELITES];
    // tratamiento de errores de Jandro
    uint8_t numErrorAviso[MAXERRORESAVISO];
    uint8_t idEstacionAviso[MAXERRORESAVISO];
    uint8_t mensajeAviso[MAXERRORESAVISO][21];
    time_t timeInicioAvisoError[MAXERRORESAVISO];
    parametroU16Flash *idLlamador;
    parametroU16Flash *dsMaxEntreMsgsLlamador;
    parametroU16Flash *dsMinEntreMsgsLlamador;

    TODO: poner TimeOut para comunicacion con pozo sOlv

 */
// LLAMADOR zona3 [idLlam 1] [dsMin 5] [dsMax 100] [sOlvido 30] pozo.logPozo.txt bOn.radio.pic.0 comOk.radio.pic.0 llam.radio.txt act.radio.txt abu.radio.txt
llamador::llamador(void)
{
    modoRadio = MODOLLAMADOR;
    bombaPozoOn = 0;
    bombaPozoSolicitada = 0;
    calendar::getFechaHora(&dateTimeEnvioAnterior);
    estadoDeseado = 0;
    dsAleatorioMinEntreMsgsLlamador = 2;
    dsAleatorioMaxEntreMsgsLlamador = 6;
    radioPtr = this;
    estadoComms = 0;
    estadoAbusones = 0;
    estadoAbusonesOld = 0;
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
    struct msgTx_t msgTx;
    msgTx.numBytes = 3;
    msgTx.msg[0] = MSG_STATUSLLAMACIONLOCAL;
    msgTx.msg[1] = idLlamador;    //idLlamadorValor();
    if (bombaPozoSolicitada) // peticion activacion?
        msgTx.msg[2] = '1';
    else
        msgTx.msg[2] = '0';
    //putQueu(&colaMsgTx, &msgTx);
    //chEvtBroadcast(&newMsgTx_source);
    RH_RF95_send(msgTx.msg,msgTx.numBytes);
    RHGenericDriver_waitPacketSent(100);
    RH_RF95_setModeRx();
    calendar::getFechaHora(&dateTimeEnvioAnterior);
}



void llamador::trataObsoletoLlamador(void)
{
    // ha pasado mucho tiempo sin recibir?
    if (calendar::sDiff(&dateTimeRxPozoAnterior)>sOlvido)
    {
        numEstadoComOk = 0;
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
    if (estadoLlamaciones!=estadoLlamacionesOld)
        radio::registraCambiosPeticion(estadoLlamaciones, estadoLlamacionesOld);  // registra cambios en memoria permanente
    if (estadoLlamaciones!=estadoLlamacionesOld || estadoActivos!=estadoActivosOld || bombaPozoOn!=petBombaOld)
    {
//        ponEnColaRegistrador();     // guardar en SD
        return 1;
    }
    else
        return 0;
}

void llamador::trataRx(struct msgRx_t *msgRx)
{
    uint8_t msgId = msgRx->msg[0];
    uint8_t estProblematica;
    uint16_t numBytes;
    uint8_t bufError[25];
    char buffer[40];
    struct tm fechHora;
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
            chsnprintf(buffer,sizeof(buffer),"%2d:%02d:%02d Msg pozo. Bomba:%d RSSI:%d",fechHora.tm_hour, fechHora.tm_min, fechHora.tm_sec, petBombaMsg, msgRx->rssi);
            uint8_t varNext = numEstMsg;
            if (varNext>6) varNext=6;
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
            chsnprintf(buffer,sizeof(buffer),"%2d:%02d:%02d Msg de #%d. Llamacion:%d RSSI:%d",fechHora.tm_hour, fechHora.tm_min, fechHora.tm_sec, numEstMsg,petBombaMsg, msgRx->rssi);
//            enviaTxtSiEnPage(idPageLog,idNombreLog, buffer);
            uint8_t varNext = numEstMsg;
            if (varNext>6) varNext=6;
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
        uint8_t estadoAbusonesOld = estadoAbusones;
        estProblematica = msgRx->msg[3];
        estadoAbusones |= (1<<(estProblematica-1));
        numBytes = msgRx->msg[4];
        if (numBytes>sizeof(bufError))
            numBytes = sizeof(bufError);
        memcpy(bufError, &msgRx->msg[5],numBytes);
        bufError[numBytes] = 0; // fin de cadena
        actualizoErrorDesdeLlamador(estProblematica, numError, bufError);
        memcpy(&ultMsg, msgRx, msgRx->numBytes);
        if (estadoAbusones != estadoAbusonesOld)
        {
            calendar::gettm(&fechHora);
            chsnprintf(buffer,sizeof(buffer),"%2d:%02d:%02d Abusa #%d msg:'%s'",fechHora.tm_hour, fechHora.tm_min, fechHora.tm_sec,estProblematica,bufError);
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
            chsnprintf(buffer,sizeof(buffer),"%2d:%02d:%02d Deja de abusar #%d",fechHora.tm_hour, fechHora.tm_min, fechHora.tm_sec, estProblematica);
            //enviaTxt(idPageLog,idNombreLog, buffer);
        }
    }
}

void llamador::trataRxRf95(eventmask_t evt)
{
    struct msgRx_t msgRx;
    struct msgTx_t msgTx;
    if (evt==0) // timeout, llamo a rutinas
    {
        trataObsoletoLlamador();
    }
    if (evt & EVENT_MASK(0)) // He recibido un mensaje rf95
    {
        while (getQueu(&colaMsgRx, &msgRx))
        {
            if (++cnt>99) cnt=0;
            trataRx(&msgRx);
        }
    }
    if (evt & EVENT_MASK(1)) // Tengo que enviar mensajes rf95
    {
        while (getQueu(&colaMsgTx, &msgTx))
        {
            RH_RF95_send(msgTx.msg,msgTx.numBytes);
            RHGenericDriver_waitPacketSent(100);
            RH_RF95_setModeRx();
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


int8_t llamador::init(void)
{
    arrancaRadio();
    calendar::getFechaHora(&dateTimeRxPozoAnterior);
    return 0;
}

void llamador::stop(void)
{
    paraRadio();
}

void llamador::update(uint8_t estadoDeseado)
{
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

void cb_sensor(void *)
{
    estadoDeseado = !palReadLine(LINE_SENSOR);
    if (estadoDeseado)
        palClearLine(LINE_LED);         // enciende
    else
        palSetLine(LINE_LED);           // apagado
    chSysLockFromISR();
    chEvtBroadcastI(&sensor_source);
    chSysUnlockFromISR();
}

void initSensor(void)
{
    estadoDeseado = !palReadLine(LINE_SENSOR);
    if (estadoDeseado)
        palClearLine(LINE_LED);         // enciende
    else
        palSetLine(LINE_LED);           // apagado
    palSetLineMode(LINE_SENSOR, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_SENSOR, PAL_EVENT_MODE_BOTH_EDGES);     // Falling edge creates event
    palSetLineCallback(LINE_SENSOR, cb_sensor, NULL); // Active callback
}


llamador *llamadorObj;
/*
 * Gestor llamacion
 */
static THD_WORKING_AREA(waThreadLlamador, 1024);
static THD_FUNCTION(ThreadLlamador, arg) {
  (void)arg;
  uint16_t dsTimeOut = 50;
  event_listener_t el0;
  chRegSetThreadName("llamador");
  llamadorObj = new llamador();
  llamadorObj->init();
  chEvtRegister(&sensor_source, &el0, 0);
  while(!chThdShouldTerminateX()) {
    eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
    if (chThdShouldTerminateX())
        chThdExit((msg_t) 1);
    dsTimeOut--;
    if (evt == 0 && dsTimeOut>0)  // timeout
        continue;
    if (dsTimeOut>0)
        chLcdprintfFila(3,"Envio estado %d",estadoDeseado);
    else
        chLcdprintfFila(3,"Reenvio estado %d",estadoDeseado);
    dsTimeOut = 50;
    llamadorObj->update(estadoDeseado);
   }
}


void llamadorInit(void)
{
    llamadorObj = new llamador();
    llamadorObj->init();
    if (!procesoLlamador)
        procesoLlamador = chThdCreateStatic(waThreadLlamador, sizeof(waThreadLlamador), NORMALPRIO + 7,  ThreadLlamador, NULL);
    initSensor();
}

