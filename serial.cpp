/*
 * serial.cpp
 *
 */


/*
 * La configuración se hace siempre por HM-10 (SD6)
 * => la salida USB se usa para conexion a un ordenador
 *
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "tty.h"
#include "gets.h"
#include "string.h"
#include <stdlib.h>
#include "chprintf.h"
#include "variables.h"
#include "version.h"
#include "lcd.h"
#include "radio.h"
#include "calendarUTC.h"
#include "modbus.h"
#include "externRegistros.h"

extern "C" {
    void initHM10(void);
}

#define ttyHM10 &SD6

//extern uint16_t modoRadio;
//extern uint16_t sBeacon;
//extern uint16_t idLlamador;
//extern uint16_t dsMaxEntreMsgsLlamador;
//extern uint16_t dsMinEntreMsgsLlamador;
//extern uint16_t bloqueoAbusones;
//extern uint16_t avisaAbuso;
//extern uint16_t tiempoAbuso;         // minutos
//extern uint16_t dsMaxEntreMsgsPozo;
//extern uint16_t dsMinEntreMsgsPozo;
//extern uint16_t idVacon;


bool sdHM10open = false;
bool hayConectadoHM10;

extern uint8_t hayW25q16;
thread_t *thrHM10 = NULL;
thread_t *thrConexHM10 = NULL;

//extern float medidaP;
//extern float frecuencia;
float presion;

static const SerialConfig ser_cfg9600 = {9600, 0, 0, 0, };//{115200, 0, 0, 0, };
static const SerialConfig ser_cfg19200 = {19200, 0, 0, 0, };//{115200, 0, 0, 0, };
//struct opcion_t {             // Structure declaration
//    uint16_t *variable;
//    uint16_t valMin;
//    uint16_t valMax;
//    const char descOpcion[];
//  };


//struct opcion_t opcMR   = { &modoRadio, 1 ,3,     "Modo radio (1:llamador, 2:pozo, 3:regist)"};
//struct opcion_t opcSO   = { &sBeacon, 60 ,1200,   "Tiempo olvido (s)"};
//struct opcion_t opcID   = { &idLlamador, 1 ,9,    "Id Llamador"};
//struct opcion_t opcTMLL = { &dsMaxEntreMsgsLlamador, 100 ,3000, "Tiempo max. entre msgs (ds)"};
//struct opcion_t opcTmLL = { &dsMinEntreMsgsLlamador, 10 ,100, "Tiempo min. entre msgs (ds)"};
//struct opcion_t opcBLQ  = { &bloqueoAbusones, 0 ,1, "Bloqueo abusones"};
//struct opcion_t opcAVS =  { &avisaAbuso, 0 ,1, "Avisa abuso"};
//struct opcion_t opcTAB =  { &tiempoAbuso, 120 ,1200, "Tiempo abuso (min)"};
//struct opcion_t opcTMPZ = { &dsMaxEntreMsgsPozo, 100 ,3000, "Tiempo max. entre msgs (ds)"};
//struct opcion_t opcTmPZ = { &dsMinEntreMsgsPozo, 1 ,100, "Tiempo min. entre msgs (ds)"};
//struct opcion_t opcVacon = { &idVacon, 1 ,100, "Modbus Addr Vacon"};


uint8_t initBaudHM10(char buffer[], uint8_t sizeBuffer)
{
    uint8_t huboTimeout;
    // devuelve 1 si hay alguien, 2 si no hay nadie y esta a 19200 y 3 idem a 9600
    // compruebo si el modulo esta a 19200
    sdStart(ttyHM10, &ser_cfg19200);
    // Veo si hay alguien conectado
    chprintf((BaseSequentialStream*) ttyHM10,"AT\r\n");
    chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer, sizeBuffer, TIME_MS2I(100), &huboTimeout);
    if (strstr(buffer,"OK"))
        return 2;  // no hay nadie, y modulo a 19200
    sdStop(ttyHM10);
    sdStart(ttyHM10, &ser_cfg9600);
    chprintf((BaseSequentialStream*) ttyHM10,"AT\r\n");
    chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer, sizeBuffer, TIME_MS2I(100), &huboTimeout);
    if (strstr(buffer,"OK"))
        return 3;  // no hay nadie, y modulo a 9600
    // debe haber alguien conectado, lo dejo en 19200
    sdStop(ttyHM10);
    sdStart(ttyHM10, &ser_cfg19200);
    chprintf((BaseSequentialStream*) ttyHM10,"\nHola\n");
    return 1;
}


uint8_t esperaHM10(void)
{
    while(true)
    {
        for (uint16_t ds=0;ds<100;ds++)
        {
            if (palReadLine(LINE_STHM10))
                return 1;
            chThdSleepMilliseconds(100);
        }
    }
}


void initSerialHM10(void) {
    uint8_t huboTimeout;
    char buffer[20];
    palClearLine(LINE_TX6);
    palSetLine(LINE_RX6);
    palSetLineMode(LINE_RX6, PAL_MODE_ALTERNATE(8));
    palSetLineMode(LINE_TX6, PAL_MODE_ALTERNATE(8));
    palSetLineMode(LINE_STHM10,PAL_MODE_INPUT | PAL_STM32_PUPDR_PULLUP);
    sdStart(ttyHM10, &ser_cfg19200);
    //chprintf((BaseSequentialStream*) ttyHM10,"AT+PIO11\r\n"); // el led debe replicar el estado de conexion
    uint8_t estadoHM10 = initBaudHM10(buffer,sizeof(buffer));
    if (estadoHM10 == 1)  // debe haber alguien conectado, todo esta ok
        return;
    if (estadoHM10 == 3) // esta a 9600 => paso el modulo a 19200
    {
        chprintf((BaseSequentialStream*) ttyHM10,"AT+BAUD5\r\n"); // 4=>9600, 5=>19200
        chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer,sizeof(buffer), TIME_MS2I(100), &huboTimeout);
        chLcdprintfFila(3,"Pasado HM10 a 19200");
        sdStop(ttyHM10);
        sdStart(ttyHM10, &ser_cfg19200);
        chThdSleepMilliseconds(100);
    }
    limpiaBuffer((BaseChannel *) ttyHM10);
    chprintf((BaseSequentialStream*) ttyHM10,"AT+NAME\r\n");
    chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer,sizeof(buffer), TIME_MS2I(100), &huboTimeout);
    if (!strstr(buffer,"AT+NAME"))
    {
        chLcdprintfFila(3,"HM10 es '%s'",&buffer[6]);
        chThdSleepMilliseconds(2000);
        chLcdprintfFila(3,"");
    }
}


void ajustaHora(SerialDriver *sdCOM)
{
    uint32_t ano, mes, dia, hora, min, sec;
    char buff[50];
    struct tm tim;
    int16_t result;

    BaseSequentialStream *ttyCOM = (BaseSequentialStream *) sdCOM;
    calendar::rtcGetFecha();
    calendar::gettm(&tim);
    ano = tim.tm_year+100;
    mes = tim.tm_mon+1;
    dia = tim.tm_mday;
    hora = tim.tm_hour;
    min = tim.tm_min;
    sec = tim.tm_sec;
    if (preguntaNumeroHM10((BaseChannel *) sdCOM, "Anyo", &ano, 2023, 2060) != 0)
        return;
    preguntaNumeroHM10((BaseChannel *) sdCOM, "Mes", &mes, 1, 12);
    preguntaNumeroHM10((BaseChannel *) sdCOM, "Dia", &dia, 1, 31);
    preguntaNumeroHM10((BaseChannel *) sdCOM, "Hora", &hora, 0, 23);
    preguntaNumeroHM10((BaseChannel *) sdCOM, "Minutos", &min, 0, 59);
    result = preguntaNumeroHM10((BaseChannel *) sdCOM, "Segundos", &sec, 0, 59);
    if (result != 0)
        return;
    calendar::cambiaFechaTM(ano-1900, mes-1, dia, hora, min, sec, 0);
    calendar::printFecha(buff,sizeof(buff));
    chprintf(ttyCOM,"Fecha actual UTC: %s\n",buff);
}



void ajustaNumero(SerialDriver *sdCOM, const char *desc, holdingRegisterInt *holdReg)
{
    int16_t result;
    uint32_t opcion;
    result = preguntaNumeroHM10((BaseChannel *) sdCOM, desc, &opcion, holdReg->getValorMin(), holdReg->getValorMax());
    if (result != 0)
        return;
    holdReg->setValorInterno((uint16_t) opcion);
//    controlMB->escribeHR(false);
}

void ajustaNumero(SerialDriver *sdCOM, holdingRegisterInt *holdReg)
{
    int16_t result;
    uint32_t opcion;
    result = preguntaNumeroHM10((BaseChannel *) sdCOM, holdReg->getNombre(), &opcion, holdReg->getValorMin(), holdReg->getValorMax());
    if (result != 0)
        return;
    holdReg->setValorInterno((uint16_t) opcion);
//    controlMB->escribeHR(false);
}

void ajustaNumeroFloat(SerialDriver *sdCOM, const char *desc, holdingRegisterFloat *holdReg)
{
    int16_t result;
    float opcion;
    result = preguntaNumeroHM10Float((BaseChannel *) sdCOM, desc, &opcion, holdReg->getValorMin(), holdReg->getValorMax());
    if (result != 0)
        return;
    holdReg->setValor(opcion);
//    controlMB->escribeHR(holdReg);
}

void ajustaNumeroFloat(SerialDriver *sdCOM, holdingRegisterFloat *holdReg)
{
    int16_t result;
    float opcion;
    result = preguntaNumeroHM10Float((BaseChannel *) sdCOM, holdReg->getNombre(), &opcion, holdReg->getValorMin(), holdReg->getValorMax());
    if (result != 0)
        return;
    holdReg->setValor(opcion);
//    controlMB->escribeHR(holdReg);
}

void ajustaNumeroEscala(SerialDriver *sdCOM, const char *desc, holdingRegisterFloat *holdReg)
{
    int16_t result;
    float opcion;
    result = preguntaNumeroHM10Float((BaseChannel *) sdCOM, desc, &opcion, holdReg->getValorMin(), holdReg->getValorMax());
    if (result != 0)
        return;
    holdReg->setValor(opcion);
}

/*
 * MENUS
 */

uint8_t ajustaSeleccionInt2Ext(SerialDriver *sdCOM, holdingRegisterInt2Ext *regOpcHR)
{
    int16_t result;
    uint32_t opcion;
    BaseSequentialStream *ttyOpciones = (BaseSequentialStream *) sdCOM;
    for (uint16_t pos=1; pos<=regOpcHR->getNumOpciones();pos++)
        chprintf(ttyOpciones,"%d: %d\n", pos-1, regOpcHR->getValor(pos-1));
    chprintf(ttyOpciones,"%d Volver\n",regOpcHR->getNumOpciones());
    limpiaBuffer((BaseChannel *) ttyHM10);
    result = preguntaNumeroHM10((BaseChannel *) sdCOM, "Dime opcion", &opcion, 0, regOpcHR->getNumOpciones());
    if (result != 0 || (result==0 && opcion==regOpcHR->getNumOpciones()))
        return 1;
    regOpcHR->setValor(regOpcHR->getValor(opcion));
    return 0;
}

uint8_t ajustaSeleccion(SerialDriver *sdCOM, holdingRegisterOpciones *regOpcHR)
{
    int16_t result;
    uint32_t opcion;
    BaseSequentialStream *ttyOpciones = (BaseSequentialStream *) sdCOM;
    for (uint16_t pos=1; pos<=regOpcHR->getNumOpciones();pos++)
        chprintf(ttyOpciones,"%d: %s\n", pos-1, regOpcHR->getDescripcion(pos-1));
    chprintf(ttyOpciones,"%d Volver\n",regOpcHR->getNumOpciones());
    limpiaBuffer((BaseChannel *) ttyHM10);
    result = preguntaNumeroHM10((BaseChannel *) sdCOM, "Dime opcion", &opcion, 0, regOpcHR->getNumOpciones());
    if (result != 0 || (result==0 && opcion==regOpcHR->getNumOpciones()))
        return 1;
    regOpcHR->setValor(opcion);
    return 0;
}



void printHRInt(BaseSequentialStream *ttyCOM, holdingRegisterInt *opcion)
{
    chprintf(ttyCOM,"%s: %d\n",opcion->getNombre(),opcion->getValor());
}

void printHROpciones(BaseSequentialStream *ttyCOM, holdingRegisterOpciones *opcion)
{
    chprintf(ttyCOM,"%s: %s\n",opcion->getNombre(),opcion->getDescripcion());
}

uint8_t cambiaNombreModulo(SerialDriver *sdCOM)
{
    uint8_t huboTimeout;
    char buffer[15];
    BaseSequentialStream *ttyCOM = (BaseSequentialStream *) sdCOM;
    chprintf(ttyCOM,"Nuevo nombre modulo (3-10 caracteres):");
    chgetsNoEchoTimeOut((BaseChannel *) sdCOM, (uint8_t *) buffer,sizeof(buffer), TIME_MS2I(20000), &huboTimeout);
    chprintf(ttyCOM,"\n");
    if (strlen(buffer)<3 || strlen(buffer)>10)
    {
        chprintf(ttyCOM,"Longitud erronea\n");
        return 0;
    }
    chprintf(ttyCOM,"Pongo nombre %s\n",buffer);
    chprintf(ttyCOM,"Desconectate de HM10 !!\n");
    palClearLine(LINE_ENHM10);
    chThdSleepMilliseconds(10000);
    // reseteamos el modulo
    chprintf((BaseSequentialStream *)ttyHM10,"AT+NAME%s\r\n",buffer);
    chThdSleepMilliseconds(100);
    chprintf((BaseSequentialStream *)ttyHM10,"AT+RESET\r\n");
    chThdSleepMilliseconds(100);
    palSetLine(LINE_ENHM10);
    chLcdprintfFila(3,"Conectate a HM10");
    chThdSleepMilliseconds(4000);
    return 1;
}


static THD_WORKING_AREA(waThreadHM10, 1024);
static THD_FUNCTION(ThreadHM10, arg) {
    (void)arg;
    chRegSetThreadName("HM10");
    int16_t result;
    uint32_t opcion;
    char binStr[10];

    BaseSequentialStream *bssHM10 = (BaseSequentialStream *) ttyHM10;
    BaseChannel *bcHM10 = (BaseChannel *) ttyHM10;
    palSetLineMode(LINE_ENHM10, PAL_MODE_OUTPUT_PUSHPULL);
    // reseteamos modulo (mantener RESET bajo >100ms)
    palClearLine(LINE_ENHM10);
    chThdSleepMilliseconds(120);
    palSetLine(LINE_ENHM10);
    initSerialHM10();
    while (true)
    {
        if (!palReadLine(LINE_STHM10))
        {
            while (!palReadLine(LINE_STHM10))
                chThdSleepMilliseconds(100);
            chprintf((BaseSequentialStream *)&SD1,"Se han conectado a HM10\n");
            chThdSleepMilliseconds(1000);
        }
        //leeVariables();
        chprintf(bssHM10,"\n");
        chprintf(bssHM10,"GIT Tag:%s Commit:%s\n",GIT_TAG,GIT_COMMIT);
        int2str(peticionesIR->getValor(), binStr);
        chprintf(bssHM10,"Piden:%s\n",binStr);
        int2str(activosIR->getValor(), binStr);
        chprintf(bssHM10,"Activ:%s\n",binStr);
        int2str(abusonesIR->getValor(), binStr);
        chprintf(bssHM10,"Abuso:%s\n",binStr);
        printHROpciones(bssHM10,modoRadioHR);// printOpcion(ttyOpciones, &opcMR);
        chprintf(bssHM10,"Presion:%.1f\n",presion);
        chprintf(bssHM10,"Ajuste variables\n");
        chprintf(bssHM10,"1 Nombre modulo\n");
        chprintf(bssHM10,"2 Modo radio:%s\n",modoRadioHR->getDescripcion());
        if (modoRadioHR->getValor()==0)
        {
            chprintf(bssHM10,"3 T olvido llamador:%d s\n",sTimeOutLlamadoresHR->getValor());
            chprintf(bssHM10,"4 Id Llamador: %d\n",idLlamadorHR->getValor());
            chprintf(bssHM10,"5 Tiempo max entre msgs: %d s\n",sMaxEntreMsgsLlamadorHR->getValor());
            chprintf(bssHM10,"6 Tiempo min entre msgs: %d ds\n",dsMinEntreMsgsLlamadorHR->getValor());
            chprintf(bssHM10,"7 salir\n");
            result = preguntaNumeroHM10(bcHM10, "Dime opcion", &opcion, 1, 7);
            chprintf(bssHM10,"\n");
            if (result != 0 || (result==0 && opcion==7))
                continue;
            if (opcion==1)
                cambiaNombreModulo(ttyHM10);
            if (opcion==2)
            {
                uint16_t mrold = modoRadioHR->getValor();
                ajustaSeleccion(ttyHM10, modoRadioHR);
                if (modoRadioHR->getValor() != mrold)
                    radio::arrancaRadio();
            }
            if (opcion==3)
                ajustaNumero(ttyHM10, sTimeOutLlamadoresHR);
            if (opcion==4)
                ajustaNumero(ttyHM10, idLlamadorHR);
            if (opcion==5)
                ajustaNumero(ttyHM10, sMaxEntreMsgsLlamadorHR);
            if (opcion==6)
                ajustaNumero(ttyHM10, dsMinEntreMsgsLlamadorHR);
        }
        else if (modoRadioHR->getValor()==1)
        {
            chprintf(bssHM10,"3 T olvido llamador:%d s\n",sTimeOutLlamadoresHR->getValor());
            chprintf(bssHM10,"4 Bloqueo abusones: %d\n",bloqueaAbusonesHR->getValor());
            chprintf(bssHM10,"5 Tiempo abuso: %d min\n",minutosAbusoHR->getValor());
            chprintf(bssHM10,"6 Tiempo max entre msgs: %d (ds)\n",dsMaxEntreMsgsPozoHR->getValor());
            chprintf(bssHM10,"7 Tiempo min entre msgs: %d (ds)\n",dsMinEntreMsgsPozoHR->getValor());
            chprintf(bssHM10,"8 Pres. max sensor: %.1f\n",0.1f*barMaxSensPresionHR->getValor());
            chprintf(bssHM10,"9 salir\n");
            result = preguntaNumeroHM10(bcHM10, "Dime opcion", &opcion, 1, 9);
            if (result != 0 || (result==0 && opcion==9))
                continue;
            if (opcion==1)
                cambiaNombreModulo(ttyHM10);
            if (opcion==2)
            {
                uint16_t mrold = modoRadioHR->getValor();
                ajustaSeleccion(ttyHM10, modoRadioHR);
                if (modoRadioHR->getValor() != mrold)
                    radio::arrancaRadio();
            }
            if (opcion==3)
                ajustaNumero(ttyHM10, sTimeOutLlamadoresHR);
            if (opcion==4)
                ajustaNumero(ttyHM10, bloqueaAbusonesHR);
            if (opcion==5)
                ajustaNumero(ttyHM10, minutosAbusoHR);
            if (opcion==6)
                ajustaNumero(ttyHM10, dsMaxEntreMsgsPozoHR);
            if (opcion==7)
                ajustaNumero(ttyHM10, dsMinEntreMsgsPozoHR);
            if (opcion==8)
                ajustaNumeroFloat(ttyHM10, barMaxSensPresionHR);
        }
        else if (modoRadioHR->getValor()==2)
        {
            chprintf(bssHM10,"3 Pres. max sensor: %.1f\n",0.1f*barMaxSensPresionHR->getValor());
            chprintf(bssHM10,"4 salir\n");
            result = preguntaNumeroHM10(bcHM10, "Dime opcion", &opcion, 1, 4);
            if (result != 0 || (result==0 && opcion==4))
                continue;
            if (opcion==1)
                cambiaNombreModulo(ttyHM10);
            if (opcion==2)
            {
                uint16_t mrold = modoRadioHR->getValor();
                ajustaSeleccion(ttyHM10, modoRadioHR);
                if (modoRadioHR->getValor() != mrold)
                    radio::arrancaRadio();
            }
            if (opcion==3)
                ajustaNumeroFloat(ttyHM10, barMaxSensPresionHR);
        }
    }
}


void initHM10(void)
{
    if (thrHM10==NULL)
        thrHM10 = chThdCreateStatic(waThreadHM10, sizeof(waThreadHM10), NORMALPRIO, ThreadHM10, NULL);
}


