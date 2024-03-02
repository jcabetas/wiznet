/*
 * pozo.c
 *
 *  Created on: 25/9/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include "string.h"
#include "chprintf.h"
#include <stdio.h>
#include "RH_RF95.h"
#include "colas.h"
#include "tipoVars.h"
#include "bloques.h"
#include "nextion.h"
#include "bloques.h"
#include "radio.h"
#include "calendar.h"

time_t GetTimeUnixSec(void);


extern RTCDateTime dateTimeEnvioAnterior;

extern struct queu_t colaMsgTx;
extern event_source_t newMsgTx_source;
extern struct queu_t colaMsgRx;

extern struct msgRx_t ultMsg;


extern char pendienteSMS[200];
extern uint8_t telefonoEnvio[16];
extern event_source_t enviarSMS_source;


uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
int32_t randomNum(int32_t numMin, int32_t numMax);
void int2str(uint8_t valor, char *string);


extern varSTR50 telefAdm;

/*
 * Envia MSG_STATUSPOZO
    //    Byte 0: MSG_STATUSPOZO
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 sat�lites llamadores
    //    Byte 2: Estado bomba del Pozo '0' '1'
    //    Byte 3: Array de 8 bits indicando si el Pozo reconoce como activos a cada uno de los llamadores. Para ello el sat�lite ha de transmitir como mucho cada 15 segundos
    //    Byte 4: Array de 8 bits indicando si el Pozo ha recibido una se�al de llamaci�n de un sat�lite. Se distribuye a todo el mundo, para que todos puedan saber la situaci�n
 *
 */
// POZO bOn.radio.pic.0 [dsMin 5] [dsMax 100] [blAbus 1] [tAbus 10] [sOlv 50] pozo.logPozo.txt llam.radio.txt act.radio.txt abu.radio.txt
pozo::pozo(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t tipoNextion, picBase;
    if (numPar!=11)
    {
        nextion::enviaLog(tty,"#parametros incorrectos POZO");
        *hayError = 1;
        return;
    }
    modoRadio = MODOPOZO;
    numOutput = estados::addEstado(tty, pars[1], 1, hayError);
    dsMinEntreMsgsPozo = parametro::addParametroU16FlashMinMax(tty, pars[2],2,60,hayError);
    dsMaxEntreMsgsPozo = parametro::addParametroU16FlashMinMax(tty, pars[3],50,3600,hayError);
    if (!strcasecmp(pars[4],"NO") || !strcasecmp(pars[4],"0"))
        bloqueoAbusones = new parametroU16Flash(pars[4], 1, 0, 1);
    else if (!strcasecmp(pars[4],"SI") || !strcasecmp(pars[4],"1"))
        bloqueoAbusones = new parametroU16Flash(pars[4], 0, 0, 1);
    else
        bloqueoAbusones = new parametroU16Flash(pars[4], 0, 0, 1);
    tiempoAbuso = parametro::addParametroU16FlashMinMax(tty, pars[5],0,600,hayError); // minutos
    sOlvido = parametro::addParametroU16FlashMinMax(tty, pars[6],10,600,hayError);    // segundos
    // nextion pozo
    hallaNombreyDatosNextion(pars[7], TIPONEXTIONSTR, &idNombreLog, &idNombreLogWWW, &idPageLog, &tipoNextion, &picBase);
    hallaNombreyDatosNextion(pars[8], TIPONEXTIONSILENCIO, &idNombreLlamadores, &idNombreLlamadoresWWW, &idPageLlamadores, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
        nextion::enviaLog(tty,"POZO llamadores no txt");
        *hayError = 1;
    }
    // nextion activos
    hallaNombreyDatosNextion(pars[9], TIPONEXTIONSILENCIO, &idNombreActivos, &idNombreActivosWWW, &idPageActivos, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
        nextion::enviaLog(tty,"POZO activos no txt");
        *hayError = 1;
    }
    // nextion abusones
    hallaNombreyDatosNextion(pars[10], TIPONEXTIONSILENCIO, &idNombreAbusones, &idNombreAbusonesWWW, &idPageAbusones, &tipoNextion, &picBase);
    if (tipoNextion!=TIPONEXTIONSTR)
    {
         nextion::enviaLog(tty,"POZO abusones no txt");
         *hayError = 1;
    }
    radioPtr = this;
}



void pozo::reseteaVariablesEspecificas(void)
{
    dsAleatorioMinEntreMsgsPozo= dsMinEntreMsgsPozo->valor()+randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
    dsAleatorioMaxEntreMsgsPozo = dsMaxEntreMsgsPozo->valor()+randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
    // errores de Jandro
    // tratamiento de errores de Jandro
    for (int8_t numAviso=0;numAviso<MAXERRORESAVISO;numAviso++)
    {
        numErrorAviso[numAviso] = 0;
        idEstacionAviso[numAviso] = 0;
        mensajeAviso[numAviso][0] = 0;
        timeInicioAvisoError[numAviso] = calendar::getSecUnix();
    }
    estados::ponEstado(numOutput, 0);//palClearPad(GPIOB, GPIOB_RELE);
}







/*
 *  Pone error en su slot coorespondiente
 */
int8_t pozo::actualizoErrorDesdePozo(uint8_t numEstacion)
{
    uint8_t encontrado, slot;
    struct tm ahora;
    if (numEstacion<1 || numEstacion>8)
        return -1;
    encontrado = radio::buscoSlot(1, numEstacion, &slot);
    if (encontrado==3) // no hay hueco
        return -1;
    if (encontrado==2) // nuevo slot, informacion basica
    {
        numErrorAviso[slot] = 1;
        idEstacionAviso[slot] = numEstacion;
    }
    // actualizo time y mensaje del slot
    timeInicioAvisoError[slot] = calendar::getSecUnix();
    calendar::getFecha(&ahora);
    chsnprintf((char *)mensajeAviso[slot],sizeof(mensajeAviso[slot]),"%d/%d %d:%d",ahora.tm_mday, ahora.tm_mon+1,ahora.tm_hour,ahora.tm_min);
    return slot;
}






uint8_t pozo::quitarAbuso(uint8_t numEstacion)
{
    time_t ahora;
    if (numEstacion==0 || numEstacion>8 || modoRadio!=MODOPOZO)
        return 1;
    estadoAbusones &=  ~(1<<(numEstacion-1));  // si abusaba, ya no
    enviaClearErrorPozo(1, numEstacion);
    limpiaError(numEstacion, 1);
    ahora = GetTimeUnixSec();
    timeInicioPeticion[numEstacion-1] =  ahora; // reinicio temporizador
    return 0;
}

void pozo::enviaStatusPozo(void)
{
    struct msgTx_t msgTx;
    msgTx.numBytes = 5;
    msgTx.msg[0] = MSG_STATUSPOZO;
    msgTx.msg[1] = 0;                   // Id Pozo
    if (estadoPeticionBomba()>0)
        msgTx.msg[2] = '1';
    else
        msgTx.msg[2] = '0';
    msgTx.msg[3] = estadoActivos;
    msgTx.msg[4] = estadoLlamaciones;
    putQueu(&colaMsgTx, &msgTx);
    chEvtBroadcast(&newMsgTx_source);
    calendar::getFechaHora(&dateTimeEnvioAnterior);
}

// Formato del mensaje:
//    Byte 0: MSG_ERROR
//    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
//    Byte 2: Numero de error
//    Byte 3: Id estacion con problemas. 0=Pozo, 1 a 8 satelites llamadores
//    Byte 4: Numero de bytes del texto. Max 20.
//    Texto variable: de 1 a 21 bytes de texto (incluyendo el terminador 0)
void pozo::enviaErrorPozo(uint8_t slot) //uint8_t numError, uint8_t numEstProblematica, uint8_t *msg)
{
    struct msgTx_t msgTx;
    msgTx.msg[0] = MSG_ERROR;
    msgTx.msg[1] = 0;                   // Id Pozo
    msgTx.msg[2] = numErrorAviso[slot];
    msgTx.msg[3] = idEstacionAviso[slot];
    uint8_t len = strlen((char *)mensajeAviso[slot]);
    if (len>20)
        len = 20;
    msgTx.msg[4] = len;
    memcpy(&msgTx.msg[5], mensajeAviso[slot], len);
    msgTx.msg[4+len] = 0;
    msgTx.numBytes = 4+len+1;           // incluye 0 final de string
    putQueu(&colaMsgTx, &msgTx);
    chEvtBroadcast(&newMsgTx_source);
}

// Formato del mensaje:
//    Byte 0: MSG_CLEARERROR
//    Byte 1: Id del originador: 0=Pozo, 1 a 8 sat�lites llamadores
//    Byte 2: N�mero de error
//    Byte 3: Id estaci�n con problemas resueltos. 0=Pozo, 1 a 8 sat�lites llamadores
//
void pozo::enviaClearErrorPozo(uint8_t numError, uint8_t numEstProblematica)
{
    struct msgTx_t msgTx;
    msgTx.numBytes = 4;
    msgTx.msg[0] = MSG_CLEARERROR;
    msgTx.msg[1] = 0;                   // Id Pozo
    msgTx.msg[2] = numError;
    msgTx.msg[3] = numEstProblematica;
    putQueu(&colaMsgTx, &msgTx);
    chEvtBroadcast(&newMsgTx_source);
}

/*
 * ver estado de la bomba atendiendo a los mensajes de llamaciones
 */
uint8_t pozo::estadoPeticionBomba(void)
{
    if (bloqueoAbusones->valor())
        return (uint8_t ) ((estadoLlamaciones & ~estadoAbusones)>0);
    else
        return (uint8_t ) (estadoLlamaciones>0);
}

/*
 *  Trata la recepci�n de petici�n de una estaci�n
 *  Devuelve 1 si precisa notificar cambios
 */
uint8_t pozo::gestionaPeticionPozo(uint8_t estacionMsg, uint8_t petBombaMsg)
{
    uint8_t estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld, estadoBombaOld;
//    char nombre[20], telef[15];
//    struct tm fechaIni;
    estadoLlamacionesOld = estadoLlamaciones;
    estadoActivosOld = estadoActivos;
    estadoAbusonesOld = estadoAbusones;
    estadoBombaOld = estadoPeticionBomba();
    uint8_t estacionMsgMenosUno = estacionMsg-1;
    time_t ahora = calendar::getSecUnix();
    timeUltConexion[estacionMsgMenosUno] = ahora;
    estadoActivos |= (1<<estacionMsgMenosUno);
    uint8_t estabaPidiendo = (estadoLlamaciones>>estacionMsgMenosUno) & 1;
    uint8_t estabaAbusando = (estadoAbusones>>estacionMsgMenosUno) & 1;
    if (!bloqueoAbusones->valor() && estabaPidiendo && !petBombaMsg && estabaAbusando)  // abusador que deja de pedir
    {
        estadoAbusones &=  ~(1<<estacionMsgMenosUno);  // si abusaba, ya no
        enviaClearErrorPozo(1, estacionMsg);
        limpiaError(estacionMsg, 1);
    }
    // posible abuso?
    if (bloqueoAbusones->valor() && !estabaAbusando && estabaPidiendo && petBombaMsg
            && (ahora-timeInicioPeticion[estacionMsgMenosUno]>tiempoAbuso->valor()))
    {
        estadoAbusones |= (1<<estacionMsgMenosUno);
        uint8_t slot = actualizoErrorDesdePozo(estacionMsg);
        enviaErrorPozo(slot);
        if (avisaAbuso->valor() && bloqueoAbusones->valor())
        {
            // ToDo avisa abuso por SMS
//            localtime_r(&timeInicioPeticion[estacionMsg],&fechaIni);
//            devuelveTlfyNombre(estacionMsg, (char *) telef, sizeof(telef), nombre, sizeof(nombre));
//            strncpy((char *)telefonoEnvio, telefAdm.getValor20Str(), sizeof(telefonoEnvio));
//            chsnprintf(pendienteSMS,sizeof(pendienteSMS),"%s [#%d] lleva pidiendo agua desde %d/%d %d:%2d\nNo considero su llamacion",
//                       nombre, estacionMsg, fechaIni.tm_mday, fechaIni.tm_mon+1, fechaIni.tm_hour, fechaIni.tm_min);
//            chEvtBroadcast(&enviarSMS_source);
        }
    }
    if (petBombaMsg==0)
        estadoLlamaciones &= ~(1<<(estacionMsgMenosUno));
    else
        estadoLlamaciones |= (1<<(estacionMsgMenosUno));
    if (estadoLlamaciones!=estadoLlamacionesOld)
        radio::registraCambiosPeticion(estadoLlamacionesOld, estadoLlamaciones);  // registra cambios en memoria permanente
    if (estadoLlamaciones!=estadoLlamacionesOld || estadoActivos!=estadoActivosOld || estadoAbusones!=estadoAbusonesOld
            || estadoBombaOld!=estadoPeticionBomba() || estadoBombaOld!=estadoPeticionBomba())
    {
        estados::ponEstado(numOutput, estadoPeticionBomba());
        ponEnColaRegistrador();     // guardar en SD
        actualizaNextion(estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld, estadoPeticionBomba());
        return 1;
    }
    else
        return 0;
}

/*
 * Nos llaman cada segundo, para comprobar obsolescencias
 */
void pozo::trataObsoletoPozo(void)
{
    // llevan mucho tiempo sin conexion?
    uint8_t envioEstado = 0;
    uint8_t hayCambios = 0;
    for (uint8_t disp=0;disp<NUMSATELITES;disp++)
    {
        uint8_t estabaActivo = (estadoActivos>>disp) & 1;
        if (estabaActivo && calendar::sDiff(&timeUltConexion[disp]) > sOlvido->valor())
        {
            estadoActivos &= ~(1<<disp);
            estadoAbusones &= ~(1<<disp);
            estadoLlamaciones &= ~(1<<disp);
            envioEstado = 1;
            hayCambios = 1;
        }
    }
    if (envioEstado==0)  // hace mucho tiempo que no env�o nada?
    {
        uint32_t dsDif = calendar::dsDiff(&dateTimeEnvioAnterior);
        if (dsDif>dsMaxEntreMsgsPozo->valor())
            envioEstado = 1;
    }
    if (hayCambios)
    {
        estados::ponEstado(numOutput, estadoPeticionBomba());//actualizaRele();
//        chEvtBroadcast(&hayCambiosLCD_source);
    }
    if (envioEstado)
    {
        enviaStatusPozo();
    }
}



/*
 *  - Decodifica mensaje de llamacion
 *  - Hace cambios en variables
 *  - Pide enviar mensaje si:
 *     * Si ha cambiado su peticion un satelite
 *     * si cambia algun estado por la peticion
 */

void pozo::trataRx(struct msgRx_t *msgRx)
{
    uint8_t msgId = msgRx->msg[0];
    uint8_t petBombaMsg, numEst;
    // siendo el Pozo, no hare nada con mensajes de otro pozo
    if (msgId==MSG_STATUSPOZO && msgRx->numBytes==5 && msgRx->msg[1]==0 && (msgRx->msg[2] == '0' || msgRx->msg[2] =='1'))
    {
        nextion::enviaLog(NULL,"Error, varios POZO");
        return;
    }
    if (msgId==MSG_STATUSLLAMACIONLOCAL && msgRx->numBytes==3)
    {
        numEst = msgRx->msg[1];
        petBombaMsg = msgRx->msg[2]-'0';
        // incorpora peticion
        if (numEst>=1 && numEst<=8 && (petBombaMsg==0 || petBombaMsg==1))
        {
            // actualizo variables
            if (pozo::gestionaPeticionPozo(numEst, petBombaMsg))  // si hay cambios que notificar
            {
                enviaStatusPozo();
            }
            memcpy(&ultMsg, msgRx, sizeof(ultMsg));
            // TODO enviar a NExtion
//            chEvtBroadcast(&hayRxParaLCD_source);
        }
    }
}

void pozo::trataRxRf95(eventmask_t evt)
{
    struct msgRx_t msgRx;
    struct msgTx_t msgTx;
    if (evt==0) // timeout, llamo a rutinas
    {
        trataObsoletoPozo();
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

pozo::~pozo()
{
    radioPtr = NULL;
}

const char *pozo::diTipo(void)
{
    return "POZO";
}

const char *pozo::diNombre(void)
{
    return "POZO";
}

int8_t pozo::init(void)
{
    arrancaRadio();
    return 0;
}

void pozo::stop(void)
{
    paraRadio();
}

uint8_t pozo::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    estados::ponEstado(numOutput, estadoPeticionBomba());
    return 0;
}

void pozo::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void pozo::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"POZO output:%s-%d",estados::nombre(numOutput),numOutput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

