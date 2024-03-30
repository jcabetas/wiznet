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
#include "calendarUTC.h"

extern "C" {
    void initHM10(void);
}

#define ttyHM10 &SD6

extern uint16_t modoRadio;
extern uint16_t sOlvido;
extern uint16_t idLlamador;
extern uint16_t dsMaxEntreMsgsLlamador;
extern uint16_t dsMinEntreMsgsLlamador;
extern uint16_t bloqueoAbusones;
extern uint16_t avisaAbuso;
extern uint16_t tiempoAbuso;         // minutos
extern uint16_t dsMaxEntreMsgsPozo;
extern uint16_t dsMinEntreMsgsPozo;


bool sdHM10open = false;
bool hayConectadoHM10;

extern uint8_t hayW25q16;
thread_t *thrHM10 = NULL;
thread_t *thrConexHM10 = NULL;


static const SerialConfig ser_cfg9600 = {9600, 0, 0, 0, };//{115200, 0, 0, 0, };
static const SerialConfig ser_cfg19200 = {19200, 0, 0, 0, };//{115200, 0, 0, 0, };
struct opcion_t {             // Structure declaration
    uint16_t *variable;
    uint16_t valMin;
    uint16_t valMax;
    const char descOpcion[];
  };

struct opcion_t opcMR   = { &modoRadio, 1 ,3,     "Modo radio (1:registr, 2:llamador, 3:pozo)"};
struct opcion_t opcSO   = { &sOlvido, 60 ,1200,   "Tiempo olvido (s)"};
struct opcion_t opcID   = { &idLlamador, 1 ,9,    "Id Llamador"};
struct opcion_t opcTMLL = { &dsMaxEntreMsgsLlamador, 100 ,600, "Tiempo max. entre msgs (ds)"};
struct opcion_t opcTmLL = { &dsMinEntreMsgsLlamador, 10 ,100, "Tiempo min. entre msgs (ds)"};
struct opcion_t opcBLQ  = { &bloqueoAbusones, 0 ,1, "Bloqueo abusones"};
struct opcion_t opcAVS =  { &avisaAbuso, 0 ,1, "Avisa abuso"};
struct opcion_t opcTAB =  { &tiempoAbuso, 120 ,1200, "Tiempo abuso (min)"};
struct opcion_t opcTMPZ = { &dsMaxEntreMsgsPozo, 100 ,1200, "Tiempo max. entre msgs (ds)"};
struct opcion_t opcTmPZ = { &dsMinEntreMsgsPozo, 1 ,100, "Tiempo min. entre msgs (ds)"};


/*
 * Las ordenes a HM10 deben terminar en 0D+0A
   Dialogo tipico:
   => AT\r\n
      OK
   => AT+BAUD\r\n
      +BAUD=5    (5=19200, 4=9800)
   => AT+NAME\r\n
      +NAME=Pozo
   => AT+RESET\r\n

 */
uint8_t testHM10(char buffer[], uint8_t sizeBuffer)
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


void initSerialHM10(void) {
    uint8_t huboTimeout;
    char buffer[20];
    palClearLine(LINE_TX6);
    palSetLine(LINE_RX6);
    palSetLineMode(LINE_RX6, PAL_MODE_ALTERNATE(8));
    palSetLineMode(LINE_TX6, PAL_MODE_ALTERNATE(8));

    //chprintf((BaseSequentialStream*) ttyHM10,"AT+PIO11\r\n"); // el led debe replicar el estado de conexion
    uint8_t estadoHM10 = testHM10(buffer,sizeof(buffer));
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
    if (preguntaNumero((BaseChannel *) sdCOM, "Anyo", &ano, 2023, 2060) == 2)
        return;
    preguntaNumero((BaseChannel *) sdCOM, "Mes", &mes, 1, 12);
    preguntaNumero((BaseChannel *) sdCOM, "Dia", &dia, 1, 31);
    preguntaNumero((BaseChannel *) sdCOM, "Hora", &hora, 0, 23);
    preguntaNumero((BaseChannel *) sdCOM, "Minutos", &min, 0, 59);
    result = preguntaNumero((BaseChannel *) sdCOM, "Segundos", &sec, 0, 59);
    if (result==2)
        return;
    calendar::cambiaFechaTM(ano-1900, mes-1, dia, hora, min, sec, 0);
    calendar::printFecha(buff,sizeof(buff));
    chprintf(ttyCOM,"Fecha actual UTC: %s\n",buff);
}

void ajustaValor(BaseChannel *sdCOM, struct opcion_t *opcion)
{
    uint32_t var32 = *opcion->variable;
    int16_t result = preguntaNumero(sdCOM, opcion->descOpcion, &var32, opcion->valMin, opcion->valMax);
    if (result == 0)
    {
        *opcion->variable = var32;
        escribeVariables();
    }
}


void printOpcion(BaseSequentialStream *ttyCOM, struct opcion_t *opcion)
{
    chprintf(ttyCOM,"%s: %d\n",opcion->descOpcion,*opcion->variable);
}


void ajustaVariables(SerialDriver *sdCOM)
{
    int16_t result;
    uint32_t opcion;
    BaseSequentialStream *ttyCOM = (BaseSequentialStream *) sdCOM;
    BaseChannel *bcCOM = (BaseChannel *) sdCOM;
    while (1==1)
    {
        chprintf(ttyCOM,"Ajuste variables\n");
        chprintf(ttyCOM,"1 Modo radio:%d (1:llamador, 2:pozo)\n",modoRadio);
        chprintf(ttyCOM,"2 Tiempo olvido:%d s\n",sOlvido);
        if (modoRadio==1)
        {
            chprintf(ttyCOM,"3 Id Llamador: %d\n",idLlamador);
            chprintf(ttyCOM,"4 Tiempo max entre msgs: %d ds\n",dsMaxEntreMsgsLlamador);
            chprintf(ttyCOM,"5 Tiempo min entre msgs: %d ds\n",dsMinEntreMsgsLlamador);
            chprintf(ttyCOM,"6 salir\n");
            result = preguntaNumero((BaseChannel *) sdCOM, "Dime opcion", &opcion, 1, 6);
            chprintf(ttyCOM,"\n");
            if (result==2 || (result==0 && opcion==6))
                return;
            if (result==0 && opcion==1)
                ajustaValor(bcCOM, &opcMR);
            if (result==0 && opcion==2)
                ajustaValor(bcCOM, &opcSO);
            if (result==0 && opcion==3)
                ajustaValor(bcCOM, &opcID);
            if (result==0 && opcion==4)
                ajustaValor(bcCOM, &opcTMLL);
            if (result==0 && opcion==5)
                ajustaValor(bcCOM, &opcTmLL);
        }
        else
        {
            chprintf(ttyCOM,"3 Bloqueo abusones: %d\n",bloqueoAbusones);
            chprintf(ttyCOM,"4 Aviso abuso: %d\n",avisaAbuso);
            chprintf(ttyCOM,"5 Tiempo abuso: %d min\n",tiempoAbuso);
            chprintf(ttyCOM,"6 Tiempo max entre msgs: %d (ds)\n",dsMaxEntreMsgsPozo);
            chprintf(ttyCOM,"7 Tiempo min entre msgs: %d (ds)\n",dsMinEntreMsgsPozo);
            chprintf(ttyCOM,"8 salir\n");
            result = preguntaNumero((BaseChannel *) sdCOM, "Dime opcion", &opcion, 1, 8);
            if (result==2 || (result==0 && opcion==8))
                return;
            if (result==0 && opcion==1)
                ajustaValor(bcCOM, &opcMR);
            if (result==0 && opcion==2)
                ajustaValor(bcCOM, &opcSO);
            if (result==0 && opcion==3)
                ajustaValor(bcCOM, &opcBLQ);
            if (result==0 && opcion==4)
                ajustaValor(bcCOM, &opcAVS);
            if (result==0 && opcion==5)
                ajustaValor(bcCOM, &opcTAB);
            if (result==0 && opcion==6)
                ajustaValor(bcCOM, &opcTMPZ);
            if (result==0 && opcion==7)
                ajustaValor(bcCOM, &opcTmPZ);
        }
    }
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
    chThdSleepMilliseconds(10000);
    // reseteamos el modulo
    chprintf((BaseSequentialStream *)ttyHM10,"AT+NAME%s\r\n",buffer);
    chThdSleepMilliseconds(100);
    chprintf((BaseSequentialStream *)ttyHM10,"AT+RESET\r\n");
    chThdSleepMilliseconds(100);
    chLcdprintfFila(3,"Conectate a HM10");
    chThdSleepMilliseconds(4000);
    return 1;
}



//struct opcion_t opcMR   = { &modoRadio, 1 ,2,     "Modo radio (1:llamador, 2:pozo)"};
//struct opcion_t opcSO   = { &sOlvido, 60 ,1200,   "Tiempo olvido (s)"};
//struct opcion_t opcID   = { &idLlamador, 1 ,9,    "Id Llamador"};
//struct opcion_t opcTMLL = { &dsMaxEntreMsgsLlamador, 100 ,600, "Tiempo max. entre msgs (ds)"};
//struct opcion_t opcTMLL = { &dsMinEntreMsgsLlamador, 10 ,100, "Tiempo min. entre msgs (ds)"};
//struct opcion_t opcBLQ  = { &bloqueoAbusones, 0 ,1, "Bloqueo abusones"};
//struct opcion_t opcAVS =  { &avisaAbuso, 0 ,1, "Avisa abuso"};
//struct opcion_t opcTAB =  { &tiempoAbuso, 120 ,1200, "Tiempo abuso (min)"};
//struct opcion_t opcTMPZ = { &dsMaxEntreMsgsPozo, 100 ,1200, "Tiempo max. entre msgs (ds)"};
//struct opcion_t opcTmPZ = { &dsMinEntreMsgsPozo, 1 ,100, "Tiempo min. entre msgs (ds)"};



static THD_WORKING_AREA(waThreadHM10, 1024);
static THD_FUNCTION(ThreadHM10, arg) {
    (void)arg;
    chRegSetThreadName("HM10");
    int16_t result;
    uint32_t opcion;
    char buff[25];
    BaseSequentialStream *ttyOpciones;
    ttyOpciones = (BaseSequentialStream *)ttyHM10;
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
            chThdSleepMilliseconds(100);
            continue;
        }
        chprintf((BaseSequentialStream *)&SD1,"Se han conectado a HM10\n");
        leeVariables();
        chprintf(ttyOpciones,"\n");
        chprintf(ttyOpciones,"GIT Tag:%s Commit:%s\n",GIT_TAG,GIT_COMMIT);
        calendar::printFecha(buff,sizeof(buff));
        chprintf((BaseSequentialStream*)&SD6,"Fecha actual UTC: %s\n",buff);
        printOpcion(ttyOpciones, &opcMR);
        printOpcion(ttyOpciones, &opcSO);
        if (modoRadio==1)
        {
            printOpcion(ttyOpciones, &opcID);
            printOpcion(ttyOpciones, &opcTMLL);
            printOpcion(ttyOpciones, &opcTmLL);
        }
        else
        {
            printOpcion(ttyOpciones, &opcBLQ);
            printOpcion(ttyOpciones, &opcAVS);
            printOpcion(ttyOpciones, &opcTAB);            printOpcion(ttyOpciones, &opcID);
            printOpcion(ttyOpciones, &opcTMPZ);
            printOpcion(ttyOpciones, &opcTmPZ);
        }
        chprintf(ttyOpciones,"\n");
        chprintf(ttyOpciones,"1 Ajusta variables\n");
        chprintf(ttyOpciones,"2 Ajusta fecha\n");
        chprintf(ttyOpciones,"3 Cambiar nombre modulo\n");

        limpiaBuffer((BaseChannel *) ttyHM10); // por si esta conectado HM-10 y da mensajes de error
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 3);
        chprintf(ttyOpciones,"\n");
        if (result==1 || result ==0)
            continue;
        if (result==0 && opcion==1)
            ajustaVariables(ttyHM10);
        if (result==0 && opcion==2)
            ajustaHora(ttyHM10);
        if (result==0 && opcion==3)
            cambiaNombreModulo(ttyHM10);
    }
}

void initHM10(void)
{
    if (thrHM10==NULL)
        thrHM10 = chThdCreateStatic(waThreadHM10, sizeof(waThreadHM10), NORMALPRIO, ThreadHM10, NULL);
}


