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
#include "radio.h"
#include "calendarUTC.h"
#include "modbus.h"
#include "externRegistros.h"

time_t GetTimeUnixSec(void);

extern "C"
{
    void pozoInit(void);
}

extern RTCDateTime dateTimeEnvioAnterior;

extern struct queu_t colaMsgTx;
extern event_source_t newMsgTx_source;
extern struct queu_t colaMsgRx;
extern event_source_t newMsgRx_source;
extern struct msgRx_t ultMsg;


extern char pendienteSMS[200];
extern uint8_t telefonoEnvio[16];
extern event_source_t enviarSMS_source;


uint32_t msEntreFechas(RTCDateTime *fechaNew, RTCDateTime *fechaOld);
int32_t randomNum(int32_t numMin, int32_t numMax);
void int2str(uint8_t valor, char *string);

/*
 * En pozo.ino
 *  // Si un llamador no ha mandado un mensaje durante más de este valor, se le considera apagado (5 minutos)
    #define TIMEOUT_LLAMADORES 300000L
    // 6 horas = Tiempo límite para desconectar a un llamador que abusa y tiene activa la llamación mucho tiempo.
    // Si un llamador supera este tiempo de llamada, se le ignora hasta que se ponga a cero
    #define MILLIS_DURACION_LIMITE_LLAMACION 21600000L  // 6 horas
    // #define MILLIS_DURACION_LIMITE_LLAMACION 6000L // 6 seg
    // Si cierta, ignoraremos a los abusones y dejaremos de hacerle caso a un llamador cuya llamada supere el tiempo MILLIS_DURACION_LIMITE_LLAMACION
    #define IGNORAR_LLAMADORES_ABUSIVOS 1

    En llamador.ino:
      timeBeacon = random(8000,10000);
      timeOutReintentoTx = random(1000,1200);
      // Si hace mucho que no oimos al Pozo (12s), ponemos que no nos oye y decimos que la bomba está apagada
      if (millsUltimoMensajeRecibido>0 && (now - millsUltimoMensajeRecibido >= 12000))

 *
 */



thread_t *procesoPozo = NULL;


/*
 * Envia MSG_STATUSPOZO
    //    Byte 0: MSG_STATUSPOZO
    //    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
    //    Byte 2: Estado bomba del Pozo '0' '1'
    //    Byte 3: Array de 8 bits indicando si el Pozo reconoce como activos a cada uno de los llamadores. Para ello el satelite ha de transmitir como mucho cada 15 segundos
    //    Byte 4: Array de 8 bits indicando si el Pozo ha recibido una senyal de llamacion de un satelite. Se distribuye a todo el mundo, para que todos puedan saber la situacion
 *
 */
// POZO bOn.radio.pic.0 [dsMin 5] [dsMax 100] [blAbus 1] [tAbus 10] [sOlv 50] pozo.logPozo.txt llam.radio.txt act.radio.txt abu.radio.txt
pozo::pozo(void)
{
    reseteaVariablesEspecificas();
    palSetLineMode(LINE_RELE, PAL_MODE_OUTPUT_PUSHPULL);
    bombaPozoOn = 0;
    palClearLine(LINE_RELE);
}


void pozo::reseteaVariablesEspecificas(void)
{
    dsAleatorioMinEntreMsgsPozo = dsMinEntreMsgsPozoHR->getValor() + randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
    sAleatorioMaxEntreMsgsPozo = sMaxEntreMsgsPozoHR->getValor() + randomNum(0,10); //dsMinEntreMsgsLlamadorValor()
    for (uint8_t disp=0;disp<NUMSATELITES;disp++)
        timeUltConexion[disp] = 0;
    activosIR->setValor(0);
    abusonesIR->setValor(0);
    peticionesIR->setValor(0);
    // errores de Jandro
    // tratamiento de errores de Jandro
    for (int8_t numAviso=0;numAviso<MAXERRORESAVISO;numAviso++)
    {
        numErrorAviso[numAviso] = 0;
        idEstacionAviso[numAviso] = 0;
        mensajeAviso[numAviso][0] = 0;
        timeInicioAvisoError[numAviso] = calendar::getSecUnix();
    }
}


/*
 *  Pone error en su slot coorespondiente
 */
int8_t pozo::actualizoErrorDesdePozo(uint8_t numEstacion)
{
    uint8_t encontrado, slot;
//    struct tm ahora;
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
    //calendar::getFecha(&ahora);
    //chsnprintf((char *)mensajeAviso[slot],sizeof(mensajeAviso[slot]),"%d/%d %d:%d",ahora.tm_mday, ahora.tm_mon+1,ahora.tm_hour,ahora.tm_min);
    return slot;
}


uint8_t pozo::quitarAbuso(uint8_t numEstacion)
{
    time_t ahora;
    char buffer[30];
    if (numEstacion==0 || numEstacion>8 || modoRadioHR->getValor()!=1)
        return 1;
    uint16_t estAbus = abusonesIR->getValor();
    estAbus &=  ~(1<<(numEstacion-1));  // si abusaba, ya no
    abusonesIR->setValor(estAbus);
    enviaClearErrorPozo(1, numEstacion);
    limpiaError(numEstacion, 1);
    ahora = GetTimeUnixSec();
    timeInicioPeticion[numEstacion-1] =  ahora; // reinicio temporizador
    calendar::printHora(buffer,sizeof(buffer));
    chprintf((BaseSequentialStream *)&SD1,"%s  #%d le quito abuso\n",buffer,numEstacion);
    return 0;
}

void pozo::enviaStatusPozo(void)
{
    struct msgTx_t msgTx;
    char buffer[25];
    msgTx.numBytes = 5;
    msgTx.msg[0] = MSG_STATUSPOZO;
    msgTx.msg[1] = 0;                   // Id Pozo
    if (bombaPozoOn == 1)
        msgTx.msg[2] = '1';
    else
        msgTx.msg[2] = '0';
    msgTx.msg[3] = activosIR->getValor();
    msgTx.msg[4] = peticionesIR->getValor();
    putQueu(&colaMsgTx, &msgTx);
    chEvtBroadcast(&newMsgTx_source);
    if (++cntTx>99)
        cntTx = 0;
    calendar::getFechaHora(&dateTimeEnvioAnterior);
    chsnprintf(buffer,sizeof(buffer),"Envio estado:%d",bombaPozoOn);
    escribeLCD(buffer);
    calendar::printHora(buffer,sizeof(buffer));
    chprintf((BaseSequentialStream *)&SD1,"%s  Envio estado\n",buffer);
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
//    Byte 1: Id del originador: 0=Pozo, 1 a 8 satelites llamadores
//    Byte 2: Numero de error
//    Byte 3: Id estacion con problemas resueltos. 0=Pozo, 1 a 8 satelites llamadores
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
 * actualiza estado de la bomba atendiendo a los mensajes de llamaciones
 */
void pozo::updateEstadoBomba(void) // estadoPeticionBomba
{
    if (bloqueaAbusonesHR->getValor())
        bombaPozoOn = ((peticionesIR->getValor() & ~abusonesIR->getValor())>0);
    else
        bombaPozoOn = (peticionesIR->getValor()>0);
}

/*
 *  Trata la recepcion de peticion de una estacion
 *  Devuelve 1 si precisa notificar cambios
 */
uint8_t pozo::gestionaPeticionPozo(uint8_t estacionMsg, uint8_t petBombaMsg)
{
    uint8_t estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld, estadoBombaOld;
//    char nombre[20], telef[15];
//    struct tm fechaIni;
    estadoLlamacionesOld = peticionesIR->getValor();
    estadoActivosOld = activosIR->getValor();
    estadoAbusonesOld = abusonesIR->getValor();
    estadoBombaOld = bombaPozoOn;
    uint8_t estacionMsgMenosUno = estacionMsg-1;
    time_t ahora = calendar::getSecUnix();
    timeUltConexion[estacionMsgMenosUno] = ahora;
    activosIR->setValor(activosIR->getValor() | (1<<estacionMsgMenosUno)); //    estadoActivos |= (1<<estacionMsgMenosUno);
    uint8_t estabaPidiendo = (peticionesIR->getValor()>>estacionMsgMenosUno) & 1;
    uint8_t estabaAbusando = (abusonesIR->getValor()>>estacionMsgMenosUno) & 1;
    if (!bloqueaAbusonesHR->getValor() && estabaPidiendo && !petBombaMsg && estabaAbusando)  // abusador que deja de pedir
    {
        abusonesIR->setValor(abusonesIR->getValor() & ~(1<<estacionMsgMenosUno));  // si abusaba, ya no
        enviaClearErrorPozo(1, estacionMsg);
        limpiaError(estacionMsg, 1);
    }
    // posible abuso?
    if (bloqueaAbusonesHR->getValor() && !estabaAbusando && estabaPidiendo && petBombaMsg
            && (ahora-timeInicioPeticion[estacionMsgMenosUno]>60*minutosAbusoHR->getValor()))
    {
        abusonesIR->setValor(abusonesIR->getValor() | (1<<estacionMsgMenosUno));
        uint8_t slot = actualizoErrorDesdePozo(estacionMsg);
        enviaErrorPozo(slot);
//        if (avisaAbuso && bloqueaAbusonesHR->getValor())
//        {
            // ToDo avisa abuso por SMS
//            localtime_r(&timeInicioPeticion[estacionMsg],&fechaIni);
//            devuelveTlfyNombre(estacionMsg, (char *) telef, sizeof(telef), nombre, sizeof(nombre));
//            strncpy((char *)telefonoEnvio, telefAdm.getValor20Str(), sizeof(telefonoEnvio));
//            chsnprintf(pendienteSMS,sizeof(pendienteSMS),"%s [#%d] lleva pidiendo agua desde %d/%d %d:%2d\nNo considero su llamacion",
//                       nombre, estacionMsg, fechaIni.tm_mday, fechaIni.tm_mon+1, fechaIni.tm_hour, fechaIni.tm_min);
//            chEvtBroadcast(&enviarSMS_source);
//        }
    }
    if (petBombaMsg==0)
        peticionesIR->setValor(peticionesIR->getValor() & ~(1<<(estacionMsgMenosUno)));
    else
        peticionesIR->setValor(peticionesIR->getValor() | (1<<(estacionMsgMenosUno)));
    updateEstadoBomba();
    if (peticionesIR->getValor()!=estadoLlamacionesOld || activosIR->getValor()!=estadoActivosOld || abusonesIR->getValor()!=estadoAbusonesOld
            || estadoBombaOld!=bombaPozoOn)
    {
        if (bombaPozoOn)
            palSetLine(LINE_RELE);
        else
            palClearLine(LINE_RELE);
        return 1;
    }
    else
        return 0;
}

/*
 * Nos llaman cada segundo, para comprobar obsolescencias
 */
void pozo::obsoleto(void)
{
    // llevan mucho tiempo sin conexion?
    uint8_t envioEstado = 0;
    uint8_t hayCambios = 0;
    char buffer[30];
    for (uint8_t disp=0;disp<NUMSATELITES;disp++)
    {
        uint8_t estabaActivo = (activosIR->getValor()>>disp) & 1;
        if (estabaActivo && calendar::sDiff(&timeUltConexion[disp]) > sTimeOutLlamadoresHR->getValor())
        {
            activosIR->setValor(activosIR->getValor() & ~(1<<disp));
            abusonesIR->setValor(abusonesIR->getValor() & ~(1<<disp));
            peticionesIR->setValor(peticionesIR->getValor() & ~(1<<disp));
            envioEstado = 1;
            hayCambios = 1;
            calendar::printHora(buffer,sizeof(buffer));
            chprintf((BaseSequentialStream *)&SD1,"%s  #%d sin mensajes desde %d segundos\n",buffer,disp,calendar::sDiff(&timeUltConexion[disp]));
        }
    }
    if (envioEstado==0)  // hace mucho tiempo que no envio nada?
    {
        uint32_t sDif = calendar::sDiff(&dateTimeEnvioAnterior);
        if (sDif>sMaxEntreMsgsPozoHR->getValor())
            envioEstado = 1;
    }
    if (hayCambios)
    {
        updateEstadoBomba();
        if (bombaPozoOn)
            palSetLine(LINE_RELE);
        else
            palClearLine(LINE_RELE);
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
    char buffer[30];
    uint8_t msgId = msgRx->msg[0];
    uint8_t petBombaMsg, numEst;
    // siendo el Pozo, no hare nada con mensajes de otro pozo
    if (msgId==MSG_STATUSPOZO && msgRx->numBytes==5 && msgRx->msg[1]==0 && (msgRx->msg[2] == '0' || msgRx->msg[2] =='1'))
    {
        //nextion::enviaLog(NULL,"Error, varios POZO");
        return;
    }
    if (++cntRx>99)
        cntRx = 0;
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
            chsnprintf(buffer,sizeof(buffer),"#%d Llam:%d RSSI:%d",numEst,petBombaMsg, msgRx->rssi);
            escribeLCD(buffer);
            calendar::printHora(buffer,sizeof(buffer));
            chprintf((BaseSequentialStream *)&SD1,"%s  #%d Llam:%d RSSI:%d\n",buffer,numEst,petBombaMsg, msgRx->rssi);
        }
    }
}


pozo::~pozo()
{

}

const char *pozo::diTipo(void)
{
    return "POZO";
}

const char *pozo::diNombre(void)
{
    return "POZO";
}


void pozo::stop(void)
{
    paraRadio();
}




pozo *pozoObj;

/*
 * Gestor pozo
 */
static THD_WORKING_AREA(waThreadPozo, 2000);
static THD_FUNCTION(ThreadPozo, arg) {
    (void)arg;
    struct msgRx_t msgRx;
    event_listener_t el0;
    chRegSetThreadName("pozo");
    chEvtRegister(&newMsgRx_source, &el0, 1);
    while(!chThdShouldTerminateX()) {
        eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
        if (evt == 0)  // timeout
        {
            pozoObj->obsoleto();
            continue;
        }
        if (evt == EVENT_MASK(1))  // he recibido un mensaje, que esta en la cola
        {
            while (getQueu(&colaMsgRx, &msgRx))
            {
                pozoObj->trataRx(&msgRx);
            }
        }
    }
}


uint8_t pozo::init(void)
{
    if (!procesoPozo)
        procesoPozo = chThdCreateStatic(waThreadPozo, sizeof(waThreadPozo), NORMALPRIO + 7,  ThreadPozo, NULL);
    return 0;
}
