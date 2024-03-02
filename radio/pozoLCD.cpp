/*
 * pozoLCD.c
 *
 *  Created on: 21/12/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"

void int2str(uint8_t valor, char *string);

using namespace chibios_rt;

#include "string.h"
#include "chprintf.h"
#include <stdio.h>
#include "../radio/pozo.h"
#include "time.h"
#include "lcd.h"
#include "tipoVars.h"
#include "colas.h"

extern thread_t *procesoDisplayPozo;
extern uint8_t estadoLlamaciones, estadoActivos, estadoAbusones;
extern uint8_t estadoLlamacionesOld, estadoActivosOld, estadoAbusonesOld;
extern uint8_t petBomba;
extern uint8_t cnt;
extern uint8_t fs_ready;
extern int32_t contar, contarMin, contarMax;
extern time_t timeInicioPeticion[NUMSATELITES];
// tratamiento de errores de Jandro
extern uint8_t numErrorAviso[MAXERRORESAVISO];
extern uint8_t idEstacionAviso[MAXERRORESAVISO];
extern uint8_t mensajeAviso[MAXERRORESAVISO][21];

struct msgRx_t ultMsg;

extern event_source_t qeCursorEvent;
extern event_source_t qeSwitchEvent;
extern event_source_t hayRxParaLCD_source;
extern event_source_t hayCambiosLCD_source;
extern event_source_t hayMsgParaLCD_source;
//extern event_source_t debugeaModbusLCD_source;
//extern event_source_t finDebugeaModbusLCD_source;

extern uint8_t buffer[20], bufferRx[20]; // debug Modbus
extern uint16_t bytesReceived;

extern varNOSI hayModbus;
extern varMODOPOZO modopozo;
extern varNUMERO idModbusEnergia;
extern varNUMERO idModbusVariador;
extern struct queu_t colaMsgLcd;

//extern float  kWht, kWht0, kWhtOld, kWhp, kWhv;
extern float  kW, V;
extern float frecuencia, presion;
extern volatile int16_t errorEnergia2;
extern volatile int16_t errorVariador2;

time_t dateTimePasswdOk;

extern event_source_t registraLog_source;
extern struct queu_t colaLog;

extern mutex_t MtxMedidas;

extern uint8_t callReady, smsReady, estadoCREG, rssiGPRS, pinReady;
extern char proveedor[15];

int8_t leeNumeroLCD(const char *mensaje,int32_t *numero, int32_t min, int32_t max);
uint16_t leeSDMHex(uint16_t idModbus, uint8_t function, uint16_t addressReg, int16_t *error);
void rtcShow(void);

void escribeErroresLCD(void)
{
    char binStr[10];
    uint8_t numFila = 0;
    for (uint8_t numSlot=0;numSlot<MAXERRORESAVISO;numSlot++)
    {
        if (idEstacionAviso[numSlot]!=0 && numFila<3)
            chLcdprintfFila(numFila++,"Err%d#%d %9s", numErrorAviso[numSlot],
                        idEstacionAviso[numSlot],mensajeAviso[numSlot]);
    }
    if (numFila==0)
        chLcdprintfFila(numFila++,"No hay errores");
    for (;numFila<=2;numFila++)
        chLcdprintfFila(numFila,"");
    if (estadoAbusones!=0)
    {
        int2str(estadoAbusones, binStr);
        chLcdprintfFila(3,"Abusones:%s", binStr);
    }
    else
        chLcdprintfFila(3,"");
}


void escribeMedidasLCD(void)
{
    float  kWhtP, kWhpP, kWhvP;
    float  kWP, VP, presionP, frecuenciaP;
    float m3;

    struct datosPozoGuardados *datos = (struct datosPozoGuardados *) BKPSRAM_BASE;
    if (datos->seedInicial != SEEDHISTORICO) // cuando esta iniciado
    {
        chLcdprintfFila(0,"Datos historicos");
        chLcdprintfFila(1,"corrompidos");
        chLcdprintfFila(2,"");
        chLcdprintfFila(3,"");
    }
    chMtxLock(&MtxMedidas);
    m3 = datos->m3Total;
    kWhtP = datos->kWhTotal;
    kWhpP = datos->kWhPunta;
    kWhvP = datos->kWhValle;
    kWP = kW;
    VP = V;
    frecuenciaP = frecuencia;
    presionP = presion;
    chMtxUnlock(&MtxMedidas);

    if (hayModbus.getValorNum() && idModbusEnergia.getValorNum())
    {
        if (errorEnergia2)
        {
            chLcdprintfFila(0,"Error medida energia");
            chLcdprintfFila(1,"Q: %.2f m3",m3);
            chLcdprintfFila(2,"");
        }
        else
        {
            chLcdprintfFila(0,"%d V      Pot:%5d",(int16_t) VP,(int16_t)kWP);
            if (kWhtP<=999.9f)
                chLcdprintfFila(1,"kWh:%5.1f %7.2f m3", kWhtP, m3);
            else
                chLcdprintfFila(1,"kWh:%5d %7.2f m3", (uint16_t) kWhtP, m3);
            if (kWhpP>9999.9f || kWhvP>9999.9f)
                chLcdprintfFila(2,"P:%8d V:%7d", (uint32_t)kWhpP, (uint32_t)kWhvP);
            else
                chLcdprintfFila(2,"P:%8.2f V:%7.2f", kWhpP, kWhvP);
        }
    }
    else
    {
        chLcdprintfFila(0,"Sin sensor energia");
        chLcdprintfFila(1,"Q: %.2f m3",m3);
        chLcdprintfFila(2,"");
    }

    if (hayModbus.getValorNum() && idModbusVariador.getValorNum())
    {
        if (errorVariador2)
            chLcdprintfFila(3,"No comunica variador");
        else
            chLcdprintfFila(3,"Bar:%6.1f Frec:%6.2f", presionP, frecuenciaP);
    }
    else
        chLcdprintfFila(3,"No comms variador");
}


void escriboEstacionLCD(uint8_t numEstacion) //0..7
{
    char statConex[9];
    struct tm fechaIni;
    uint8_t numFila = 0;
    uint8_t estadoConex = (estadoActivos>>numEstacion)&1;
    uint8_t estadoPetic = (estadoLlamaciones>>numEstacion)&1;
    uint8_t estadoAbuso = (estadoAbusones>>numEstacion)&1;
    if (estadoConex)
    {
        if (estadoPetic)
            strncpy(statConex,"PIDE",sizeof(statConex));
        else
            strncpy(statConex,"NO PIDE",sizeof(statConex));
    }
    else
        strncpy(statConex,"OFF",sizeof(statConex));

    chLcdprintfFila(numFila++,"Estac.:%d %11s",numEstacion+1,statConex);
    if (estadoAbuso)
    {
        if (modopozo.getValorNum()==2) // estamos en modo pozo?
        {
            localtime_r(&timeInicioPeticion[numEstacion],&fechaIni);
            chLcdprintfFila(numFila++,"Abusa %d/%d %d:%d",fechaIni.tm_mday, fechaIni.tm_mon+1,
                                        fechaIni.tm_hour, fechaIni.tm_min);
        }
        else // nos queda el mensaje del pozo
        {
            for (uint8_t numErr=0;numErr<MAXERRORESAVISO;numErr++)
            {
                if (idEstacionAviso[numErr]==(numEstacion+1))
                    chLcdprintfFila(numFila++,"Err: %s",mensajeAviso[numErr]);
            }
        }
    }
    struct datosPozoGuardados *datos = (struct datosPozoGuardados *) BKPSRAM_BASE;
    if (datos->seedInicial == SEEDHISTORICO)
    {
        uint32_t duracion = datos->datosId[numEstacion].segundosPeticion;
        uint32_t min = duracion/60;
        float m3 = datos->datosId[numEstacion].m3Total;
        chLcdprintfFila(numFila++,"Pet:%4d %d min", datos->datosId[numEstacion].numPeticiones,min);
        chLcdprintfFila(numFila++,"kWhP:%6.1f V:%6.1f",datos->datosId[numEstacion].kWhPunta, datos->datosId[numEstacion].kWhValle);
        if (numFila<=3)
            chLcdprintfFila(numFila++,"Q: %.2f m3",m3);
    }
    for (;numFila<=3;numFila++)
        chLcdprintfFila(numFila++,"");
}


void escribeLCD(const char *msgLin3)
{
    char binStr[10], statSD[3], statAbuso[6];
    if (fs_ready)
        strncpy(statSD,"SD",sizeof(statSD));
    else
        strncpy(statSD,"--",sizeof(statSD));
    if (estadoAbusones==0)
        strncpy(statAbuso,"     ",sizeof(statAbuso));
    else
        strncpy(statAbuso,"ABUSO",sizeof(statAbuso));
    chLcdprintfFila(0,"%2d Bomba:%d %5s  %s",cnt, petBomba, statAbuso, statSD);
    int2str(estadoLlamaciones, binStr);
    chLcdprintfFila(1,"Piden:%s",binStr);
    int2str(estadoActivos, binStr);
    chLcdprintfFila(2,"Activ:%s", binStr);
    chLcdprintfFila(3,msgLin3);
}


void escribeSmsLCD(void)
{
    chLcdprintfFila(0,"Conex:%d  pinOk:%d",estadoCREG, pinReady);
    chLcdprintfFila(1,"rssi:%2d  smsOk:%d",rssiGPRS, smsReady);
    chLcdprintfFila(2,"Proveedor:%s",proveedor);
    chLcdprintfFila(3,"");
}

/*
 * En VACON 100, A negativo, B positivo
 * - Coil register (0001, function 1 read, 5 para write), se refiere a bits en Control word (p.e. RUN)
 * - Discrete input (10001, function 2), es un bit read-only (p.e. run)
 * - Input register (30001, function 4), read only
 * - Holding register (40001, function 3 read, 6 write single, 16 multiple write, 23 read y write) son read/write
 *
 * Los holding registers 40001 se leen con function 3 (16 para varios) en base 0 (HR 40108 se lee en 107)
 *  * Escribir speed reference: function 16 (write multiple registers), address 0x07D0 (2000, control word)
 * Leer speed: function 0x4 (read input registers), (process data out 2103=process data 42103 => leer 2102)
 *
 * Process Data In (function 16 para varios):
 * MODBUS ADDRESS
 * 2000: Run request
 * 2002: speed reference
 *
 * Process data out (function 4):
 * MODBUS ADDRESS
 * 2101: frequency 0.01 Hz
 * 2102: speed rpm (process data 42103)
 * 2103: motor current 0.1A
 * 2104: motor torque 0.1%
 * 2105: motor power 0.1%
 *
 * Posiblemente (address no modbus):
 * Analog input 1: 59 (%*0,01)
 * Idem An2: 60
 * PID out: 23 (%*0.01)
 * PID status: 24 (0: stopped, 1: running, 3: sleep, 4: in dead-band)
 *
 */

void debugModbus(void)
{
    int32_t function, idModbus, addressReg, sigue;
    int16_t error;
    uint16_t valor;
    function = 4;
    addressReg =2101;
    if (leeNumeroLCD("Funcion",&function,4,0x16))
        return;
    idModbus = 1;
    if (leeNumeroLCD("id",&idModbus,1,4))
        return;
    do
    {
        if (leeNumeroLCD("Address",&addressReg,2000,2200))
            return;
        for (uint8_t i=0;i<8;i++)
        {
            buffer[i] = 0;
            bufferRx[i] = 0;
        }
        valor = leeSDMHex(idModbus, function, addressReg, &error);
        chLcdprintfFila(0,"=> %02X%02X%02X%02X%02X%02X%02X%02X",buffer[0], buffer[1], buffer[2], buffer[3],buffer[4], buffer[5], buffer[6], buffer[7]);
        if (error==0)
        {
            chLcdprintfFila(1,"Exito, val=%4X",valor);
            chLcdprintfFila(2,"<= %02X%02X%02X%02X%02X%02X%02X",bufferRx[0], bufferRx[1], bufferRx[2], bufferRx[3],bufferRx[4], bufferRx[5], bufferRx[6]);
        }
        else
        {
            chLcdprintfFila(1,"Err %d bytesRx:%d",error,bytesReceived);
            chLcdprintfFila(2,"<= %02X%02X%02X%02X%02X%02X%02X",bufferRx[0], bufferRx[1], bufferRx[2], bufferRx[3],bufferRx[4], bufferRx[5], bufferRx[6]);
        }
        chThdSleepMilliseconds(10000);
        sigue = 1;
        leeNumeroLCD("Sigue?",&sigue,0,1);
        if (!sigue)
            return;
    } while (TRUE);
}


static THD_WORKING_AREA(displayPozo_wa,1400);
static THD_FUNCTION(displayPozo, p) {
  (void)p;
  uint16_t msTimeout;
  int32_t passwd;
  int8_t error;
  char buffer[21];
  struct msgLcd_t msgLcd;
  event_listener_t el0, el1, el2, el3, el4;

  chRegSetThreadName("dispPozo");
  chEvtRegister(&hayRxParaLCD_source, &el0, 0);
  chEvtRegister(&hayCambiosLCD_source, &el1,1);
  chEvtRegister(&hayMsgParaLCD_source, &el2,2);
  chEvtRegister(&qeSwitchEvent, &el3, 3);
  chEvtRegister(&qeCursorEvent, &el4, 4);

  contar = 0;
  contarMin = 0;
  contarMax = 12; //
  msTimeout = 0;
  if (contar==0)
      escribeLCD(""); // mensajes
  do {
      if (contar==1)
          escribeErroresLCD();
      if (contar==2)
          escribeMedidasLCD();
      if (contar==3)
      {
          rtcShow();
          struct datosPozoGuardados *datos = (struct datosPozoGuardados *) BKPSRAM_BASE;
          if (datos->seedInicial == SEEDHISTORICO)
          {
              float m3 = datos->m3Total;
              chLcdprintfFila(3,"Q:%.3f m3",m3);
          }
      }
      if (contar==4)
          escribeSmsLCD();
      if (contar>=5 && contar<=12)
          escriboEstacionLCD(contar-5);
      eventmask_t evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(500));
      if (evt == 0) // al cabo de 10s vuelve a display normal
      {
          msTimeout += 500;
          if (msTimeout>10000)
          {
              contar = 0;
              msTimeout = 0;
              escribeLCD("");
          }
          continue;
      }
      // nuevo mensaje
      if (evt & EVENT_MASK(0))
          if (contar==0)
          {
              uint8_t tipoMsg = ultMsg.msg[0];
              switch (tipoMsg)
              {
                  case MSG_STATUSLLAMACIONLOCAL:
                      chsnprintf(buffer,sizeof(buffer),"LLAM#%d Pet:%d dB:%4d",ultMsg.msg[1],ultMsg.msg[2]-'0',ultMsg.rssi);
                      break;
                  case MSG_STATUSPOZO:
                      chsnprintf(buffer,sizeof(buffer),"POZO ST      dB:%4d",ultMsg.rssi);
                      break;
                  case MSG_ERROR:
                      chsnprintf(buffer,sizeof(buffer),"POZO ERR:#%d  dB:%4d",ultMsg.msg[3],ultMsg.rssi);
                      break;
                  case MSG_CLEARERROR:
                      chsnprintf(buffer,sizeof(buffer),"POZO CLR Est:%d dB:%4d",ultMsg.msg[3],ultMsg.rssi);
                      break;
               }
              escribeLCD(buffer);
          }
      // ha habido cambios
      if (evt & EVENT_MASK(1))
          escribeLCD("Cambios");
      // mensaje para ense�ar en LCD
      if (evt & EVENT_MASK(2))
      {
          getQueu(&colaMsgLcd, &msgLcd);
          escribeLCD((char *) msgLcd.msg);
      }
      // presionado cursor, llamamos al menu
      if (evt & EVENT_MASK(3))
      {
        uint8_t contarOld = contar;
        chEvtUnregister(&hayRxParaLCD_source, &el0);
        chEvtUnregister(&hayCambiosLCD_source, &el1);
        chEvtUnregister(&hayMsgParaLCD_source, &el2);
        chEvtUnregister(&qeSwitchEvent, &el3);
        chEvtUnregister(&qeCursorEvent, &el4);
        uint8_t modoAntiguo = modopozo.getValorNum();

        // pide password si no se ha metido en 2 hora
        time_t ahora = GetTimeUnixSec();
        if (ahora-dateTimePasswdOk>2*3600)
        {
            passwd = 0;
            error = leeNumeroLCD("Dime password (355)",&passwd,0,999);
            if (!error && passwd==355)
                dateTimePasswdOk = GetTimeUnixSec();
            else
            {
                chLcdprintfFila(0,"Password erronea!");
                chLcdprintfFila(1,"debes esperar");
                // escribe en log de SD
                struct msgLog_t msgLog;
                msgLog.timet = GetTimeUnixSec();
                chsnprintf(msgLog.msg,sizeof(msgLog.msg),"Intento erroneo de login");
                putQueu(&colaLog,&msgLog);
                chEvtBroadcast(&registraLog_source);
                chThdSleepMilliseconds(10000);
            }
        }
        if (ahora-dateTimePasswdOk<2*3600)
            menu();
        if (modoAntiguo != modopozo.getValorNum())
            reseteaVariables();
        contar = contarOld;
        contarMin = 0;
        contarMax = 12; //
        msTimeout = 0;
        chEvtRegister(&hayRxParaLCD_source, &el0, 0);
        chEvtRegister(&hayCambiosLCD_source, &el1,1);
        chEvtRegister(&hayMsgParaLCD_source, &el2,2);
        chEvtRegister(&qeSwitchEvent, &el3, 3);
        chEvtRegister(&qeCursorEvent, &el4, 4);
        escribeLCD("");
      }
      // girado cursor, simplemente reiniciamos temporizador
      if (evt & EVENT_MASK(5))
      {
          msTimeout = 0;
          if (contar==0)
              escribeLCD(""); // mensajes
      }
  } while (1==1);
}

void arrancaPozoLCD(void)
{
    dateTimePasswdOk = 0;
    if (!procesoDisplayPozo)
        procesoDisplayPozo = chThdCreateStatic(displayPozo_wa, sizeof(displayPozo_wa), NORMALPRIO,  displayPozo, NULL);
}
