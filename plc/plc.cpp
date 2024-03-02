#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include <stdio.h>
#include "stdlib.h"
#include "string.h"
#include "tty.h"
#include "gets.h"
#include "bloques.h"
#include "dispositivos.h"
#include "ff.h"
#include "w25q16.h"
#include "nextion.h"
#include "radio.h"
#include "sms.h"
#include "colas.h"
#include "version.h"

extern uint8_t hayCambios;
extern uint8_t fs_ready;
thread_t *procesoPlc;

void rtcGetTM(RTCDriver *rtcp, struct tm *tim, uint16_t *ds);
uint8_t leePlcFlash(BaseSequentialStream *tty, uint8_t sector);
uint8_t leePlcSD(BaseSequentialStream *tty,const char *nomFich);
uint8_t checkSD(BaseSequentialStream *tty);
uint8_t mataPlc(BaseSequentialStream *tty);
void stopNextionCom(void);
void stopSerial(void);


extern "C"
{
  uint8_t leePlc(BaseSequentialStream *tty,const char *nomFich);
  void initPlcC(void);
  void stopSDCc(void);
}



// con cargadores

uint8_t trataBloque(BaseSequentialStream *tty,char *buffer, uint16_t numLinea)
{
    uint8_t numPar, hayError;
    char *par[20], bufferLog[40];
    if (buffer[0] == '#')
        return 0;
    divideString(tty, buffer, &numPar, par);
    if (numPar == 0)
        return 0;
    hayError = 0;
    // prueba parametros
    if (!strcasecmp("AND", par[0]))
        new add(tty, numPar, par, &hayError);
    if (!strcasecmp("OR", par[0]))
        new OR(tty, numPar, par, &hayError);
    if (!strcasecmp("LED", par[0]))
        new LED(tty, numPar, par, &hayError);
    if (!strcasecmp("PULSADOR", par[0]))
        new pulsador(tty, numPar, par, &hayError);
    if (!strcasecmp("SINO", par[0]))
        new sino(tty, numPar, par, &hayError);
    if (!strcasecmp("SINOAUTO", par[0]))
        new sinoauto(tty, numPar, par, &hayError);
    if (!strcasecmp("RELE", par[0]))
        new rele(tty, numPar, par, &hayError);
    if (!strcasecmp("TIMER", par[0]))
        new timer(tty, numPar, par, &hayError);
    if (!strcasecmp("TIMERNOREDISP", par[0]))
        new timerNoRedisp(tty, numPar, par, &hayError);
    if (!strcasecmp("PULSO", par[0]))
        new pulso(tty, numPar, par, &hayError);
    if (!strcasecmp("INPUTTEST", par[0]))
        new inputTest(tty, numPar, par, &hayError);
    if (!strcasecmp("PROGRAMADOR", par[0]))
        new programador(tty, numPar, par, &hayError);
    if (!strcasecmp("ZONA", par[0])) //numPar == 7 &&
       new zona(tty, numPar, par, &hayError);
    if (!strcasecmp("START", par[0]))
        new start(tty, numPar, par, &hayError);
    if (!strcasecmp("FLIPFLOP", par[0]))
        new flipflop(tty, numPar, par, &hayError);
    if (!strcmp("DELAYON", par[0]))
        new delayon(tty, numPar, par, &hayError);
    if (!strcasecmp("NOT", par[0]))
        new NOT(tty, numPar, par, &hayError);
    if (!strcasecmp("REGISTRADOR", par[0]))
        new registrador(tty, numPar, par, &hayError);
    if (!strcasecmp("LLAMADOR", par[0]))
        new llamador(tty, numPar, par, &hayError);
    if (!strcasecmp("POZO", par[0]))
        new pozo(tty, numPar, par, &hayError);
    if (!strcasecmp("FECHA", par[0]))
        new fecha(tty, numPar, par, &hayError);
    if (!strcasecmp("ESDENOCHE", par[0]))
        new esdenoche(tty, numPar, par, &hayError);
    if (!strcasecmp("ESTADO", par[0]))
        new estadoEnNextion(tty, numPar, par, &hayError);
    if (!strcasecmp("SMS", par[0]))
        new sms(tty, numPar, par, &hayError, (BaseChannel *)&SD6,1);
    if (!strcasecmp("WWW", par[0]))
        new www(tty, numPar, par, &hayError);
    if (!strcasecmp("ESTADOWWW", par[0]))
        estados::ponEstadoEnWWW(tty, numPar, par, &hayError);
    if (!strcasecmp("NEXTION", par[0]))
        nextion::setNextion(tty, numPar, par, &hayError);
    if (!strcasecmp("GRUPO", par[0]))
        new grupo(tty, numPar, par, &hayError);
    if (!strcasecmp("MODBUS", par[0]))
        new modbus(tty, numPar, par, &hayError);
    if (!strcasecmp("SDM120CT", par[0]))
        new sdm120ct(tty, numPar, par, &hayError);
    if (!strcasecmp("SDM630CT", par[0]))
        new sdm630ct(tty, numPar, par, &hayError);
    if (!strcasecmp("TAC11XX", par[0]))
        new tac11xx(tty, numPar, par, &hayError);
    if (!strcasecmp("ADM4240", par[0]))
        new adm4240(tty, numPar, par, &hayError);
    if (!strcasecmp("RELES8INPUT8MB", par[0]))
        new reles8Input8MB(tty, numPar, par, &hayError);
    if (!strcasecmp("N4DIG08", par[0]))
        new N4DIG08(tty, numPar, par, &hayError);
    if (!strcasecmp("MEDIDA", par[0]))
        new medida(tty, numPar, par, &hayError);
    if (!strcasecmp("MEDIDAMAX", par[0]))
        new medidaMax(tty, numPar, par, &hayError);
    if (!strcasecmp("RELESMB", par[0]))
        new relesMB(tty, numPar, par, &hayError);
    if (!strcasecmp("PINOUT", par[0]))
        new pinOut(tty, numPar, par, &hayError);
    if (!strcasecmp("PININPUT", par[0]))
        new pinInput(tty, numPar, par, &hayError);
    if (!strcasecmp("MB16DI", par[0]))
        new mb16di(tty, numPar, par, &hayError);
    if (!strcasecmp("I2CBUS", par[0]))
        new i2cthread(tty, numPar, par, &hayError);
    if (!strcasecmp("I2COUT", par[0]))
        new placaI2COut(tty, numPar, par, &hayError);
    if (!strcasecmp("I2CINPUT", par[0]))
        new placaI2CInput(tty, numPar, par, &hayError);
    if (!strcasecmp("CANBUS", par[0]))
        new can(tty, numPar, par, &hayError);
    if (!strcasecmp("CAN2MED", par[0]))
        can::attachCan2Med(tty, numPar, par, &hayError);
    if (!strcasecmp("MED2CAN", par[0]))
        med2can::addMed2Can(tty, numPar, par, &hayError);
    if (!strcasecmp("CAN2STATE", par[0]))
        can::attachCan2State(tty, numPar, par, &hayError);
    if (!strcasecmp("STATE2CAN", par[0]))
        can::attachState2Can(tty, numPar, par, &hayError);
    if (!strcasecmp("CARGADOR", par[0]))
        new cargador(tty, numPar, par, &hayError);
    if (!strcasecmp("CARG-AJUSTES", par[0]))
        cargador::defAjustes(tty, numPar, par, &hayError);
    if (!strcasecmp("COCHESIM", par[0]))
        new cocheSim(tty, numPar, par, &hayError);
    if (!strcasecmp("CASASIM", par[0]))
        new casaSim(tty, numPar, par, &hayError);
    if (!strcasecmp("MQTT", par[0]))
        new mqttRaspi(tty, numPar, par, &hayError);
    if (!strcasecmp("SONOFF", par[0]))
        new sonoff(tty, numPar, par, &hayError);
    if (!strcasecmp("MED2MQTT", par[0]))
        medida::attachMqtt(tty, numPar, par, &hayError);
    if (!strcasecmp("CONTADOR", par[0]))
        new contador(tty, numPar, par, &hayError);
    if (!strcasecmp("T80", par[0]))
        new t80(tty, numPar, par, &hayError);
    if (hayError)
    {
        chsnprintf(bufferLog,sizeof(bufferLog),"Error linea %d (%s)",numLinea,par[0]);
        nextion::enviaLog(tty,bufferLog);
        chThdSleepMilliseconds(3000);
    }
    return hayError;
}


void borraDatos(void)
{
    mataPlc(NULL);
//    stopNextionCom();
    stopSerial();
    bloque::stopBloques();
    dispositivo::deleteAll();
    med2can::deleteAll();
    parametro::deleteAll();
    parametroFlash::deleteAll();
    bloque::deleteAll();
    campoNextion::deleteAll();
    nombres::init();
    estados::init();
    programador::numProgramadores = 0;
    medida::deleteAll();
    modbus::deleteMB();
}


static THD_WORKING_AREA(waPlc, 1024);
static THD_FUNCTION(plc, arg) {
    (void)arg;
    uint16_t ds;
    struct tm ahora;
    chRegSetThreadName("plc");
    do
    {
        systime_t time = chVTGetSystemTimeX();
        hayCambios = 0;
        rtcGetTM(&RTCD1, &ahora, &ds);
        bloque::actualizaBloques(ahora.tm_hour, ahora.tm_min, ahora.tm_sec, ds);
        bloque::addTimeBloques(1,ahora.tm_hour, ahora.tm_min, ahora.tm_sec, ds);
        bloque::actualizaBloques(ahora.tm_hour, ahora.tm_min, ahora.tm_sec, ds);
        nextion::incDs();
        chThdSleepUntilWindowed(time, chTimeAddX(time, TIME_MS2I(100)));
        //chThdSleepMilliseconds(100);
        if (chThdShouldTerminateX())
            chThdExit((msg_t) 1);
    } while (TRUE);
}


uint8_t mataPlc(BaseSequentialStream *tty)
{
    if (procesoPlc!=NULL)
    {
        chThdTerminate(procesoPlc);
        chThdWait(procesoPlc);
        procesoPlc = NULL;
        nextion::enviaLog(tty,"Matado PLC");
        return 1;
    }
    nextion::enviaLog(tty,"PLC no estaba arrancado");
    return 0;
}

void initPlc(BaseSequentialStream *tty)
{
    if (procesoPlc!=NULL)
    {
        nextion::enviaLog(tty,"PLC ya estaba arrancado");
        return;
    }
    // si hay SD, crea plcStart.txt y busca update
    checkSD(tty);
    if (leePlcFlash(tty, 0))
    {
        while (1==1) // hay error, no sigas
        {};
    }
    if (!estados::estadosInitOk(tty))
    {
        while (1==1)
        {};
    }
    nextion::enviaLog(tty,GIT_COMMIT);
    nextion::enviaLog(tty,GIT_TAG);
    // ejecuta
    hayCambios = 1;
    uint8_t hayFallo = bloque::initBloques();
    if (hayFallo)
    {
        nextion::enviaLog(tty,"Error iniciando bloques");
        registraEnLog("Error arrancando bloques");
        while (1==1)
        {};
    }
    // SOLO PARA PRUEBAS (hace falta esperar a recibir init de la raspberry)!!!
    //bloque::initWWW();
    procesoPlc = chThdCreateStatic(waPlc, sizeof(waPlc), NORMALPRIO, plc, tty);
    nextion::enviaLog(tty,"Arrancado demonio PLC");
    registraEnLog("Arrancado PLC");
}

void initPlcC(void)
{
    initPlc(NULL);
}
