#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "bloques.h"
#include "nextion.h"
#include "sms.h"

extern estados est;
uint8_t tipDuracion(char *descTipDuracion);
const char *descTipDuracion(uint8_t tipDura);

/*
class timerNoRedisp: public bloque {
    protected:
        int16_t numOut;
        int16_t numInput;
        uint16_t cuenta;
        uint16_t tiempo;
        uint8_t tipoCuenta; // 1:horas, 2:minutos, 3:segundos, 4:dsegundos
        uint8_t horaIni, minIni, secIni, dsIni; // 

}

    TIMERNOREDISP  output input tiempo [tipoCuenta (ds default)]

    => Arranca cuando la entrada es 1
       Cuando ha arrancado esta a nivel alto hasta que transcurre el tiempo y baja la entrada
       Es Timer NO redisparable

       Input   ______-------------------______
       Output  ______----------------------___
                     <-------  T  -------->

 */



timerNoRedisp::timerNoRedisp(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar<4 || numPar>5)
    {
        nextion::enviaLog(tty,"#parametros TIMER-No Red.");
        *hayError = 1;
        return; // error
    }
    numOut = estados::addEstado(tty, pars[1],1,hayError);
    if (numOut==0)
    {
        *hayError = 1;
        return;
    }
    numInput = estados::addEstado(tty, pars[2],0, hayError);
    tiempo = parametro::addParametroU16(tty,pars[3],hayError); //atoi(pars[3]);
    if (numPar==5)
        tipoCuenta = tipDuracion(pars[4]);
    else
        tipoCuenta = 4;     
}

timerNoRedisp::~timerNoRedisp()
{
}

const char *timerNoRedisp::diTipo(void)
{
	return "timerNoRedisp";
}

const char *timerNoRedisp::diNombre(void)
{
	return estados::nombre(numOut);
}

int8_t timerNoRedisp::init(void)
{
    estados::ponEstado(numOut, 0);
    cuenta = 0;
    return 0;
}

uint8_t timerNoRedisp::calcula(uint8_t , uint8_t min, uint8_t seg, uint8_t ds)
{
    // si no estaba arrancado. Hay que arrancar ahora??
    if (!estados::diEstado(numOut) && estados::diEstado(numInput))
    {
        estados::ponEstado(numOut, 1);
        minIni = min;
        secIni = seg;
        dsIni = ds;
        cuenta = 0;
    }
    return 0;
}

void timerNoRedisp::addTime(uint16_t dsInc, uint8_t , uint8_t min, uint8_t seg, uint8_t ds)
{
    if (estados::diEstado(numOut))
    {
        // estabamos activos, miro si tengo que avanzar contador
        if  (tipoCuenta==4 ||  // ds
            (tipoCuenta==3 && (dsIni==ds)) ||   // seg
            (tipoCuenta==2 && (dsIni==ds) && (secIni==seg)) ||  // min
            (tipoCuenta==1 && (dsIni==ds) && (secIni==seg) && minIni==min)) // hora
        {
            if (cuenta<tiempo->valor())
                cuenta += dsInc;
        }
        // si hemos llegado al maximo, pero la entrada esta baja, desactiva
        if (cuenta==tiempo->valor() && !estados::diEstado(numInput))
        {
            estados::ponEstado(numOut, 0);
        }
    }
}

void timerNoRedisp::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = TIMERNOREDISP [%s-%d] T:%d %s",estados::nombre(numOut),numOut,
           estados::nombre(numInput), numInput, tiempo->valor(), descTipDuracion(tipoCuenta));
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void timerNoRedisp::statusSMS(sms *smsPtr)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"%s:%d", estados::nombre(numOut), estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
