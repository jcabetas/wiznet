#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "bloques.h"
#include "string.h"
#include "parametros.h"
#include "nextion.h"
#include "sms.h"

extern estados est;


uint8_t tipDuracion(BaseSequentialStream *tty, char *descTipDuracion);
const char *descTipDuracion(uint8_t tipDura);

/*
    PULSO  output input tiempo [tipoCuenta (ds default)]

    => Arranca cuando la entrada es 1
       Mantiene la salida alta durante tiempo
       Solo se activa en transicion 0-1

        Input   ______--------------------------------------_________________________
       Output   ______----------------------------____________________________________
                      <-------  T  -------->

 */


pulso::pulso(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar < 4 || numPar > 5)
    {
        nextion::enviaLog(tty,"#parametros incorrecto PULSO");
        *hayError = 1;
        return; // error
    }
    numOut = estados::addEstado(tty, pars[1], 1, hayError);
    if (numOut == 0)
        return;
    numInput = estados::addEstado(tty, pars[2], 0, hayError);
    tiempo = parametro::addParametroU16(tty, pars[3],hayError);
    if (numPar == 5)
        tipoCuenta = tipDuracion(tty, pars[4]);
    else
        tipoCuenta = 4;
}

pulso::~pulso()
{
}

const char *pulso::diTipo(void)
{
    return "pulso";
}

const char *pulso::diNombre(void)
{
    return estados::nombre(numOut);
}

int8_t pulso::init(void)
{
    estados::ponEstado(numOut, 0);
    cuenta = 0;
    inputOld = estados::diEstado(numInput);
    return 0;
}

uint8_t pulso::calcula(uint8_t , uint8_t min, uint8_t seg, uint8_t ds)
{
    // hay que arrancarlo
    if (estados::diEstado(numInput) && inputOld==0)
    {
        cuenta = 0;                     // si, reseteamos contador
        dsIni = ds;
        secIni = seg;
        minIni = min;
        estados::ponEstado(numOut, 1);  // y activamos salida
    }
    inputOld = estados::diEstado(numInput);
    return 0;
}

//uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds
void pulso::addTime(uint16_t dsInc, uint8_t , uint8_t min, uint8_t seg, uint8_t ds)
{
    if (estados::diEstado(numOut))
    {
        // la entrada ha dejado de estar inactiva, vemos si tengo que avanzar contador
        // si arranco a las 3:55 15,3 s
        // si cuenta minutos tiene que avanzar cuando coincidan los seg y dseg
        if (tipoCuenta == 4 ||                                                      // ds
            (tipoCuenta == 3 && (dsIni == ds)) ||                                   // seg
            (tipoCuenta == 2 && (dsIni == ds) && (secIni == seg)) ||                // min
            (tipoCuenta == 1 && (dsIni == ds) && (secIni == seg) && minIni == min)) // hora
        {
            cuenta += dsInc;
            if (cuenta >= tiempo->valor())
            {
                estados::ponEstado(numOut, 0);
                cuenta = 0;
            }
        }
    }
}

void pulso::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = PULSO [%s-%d] T:%d %s", estados::nombre(numOut), numOut,
           estados::nombre(numInput), numInput, tiempo->valor(), descTipDuracion(tipoCuenta));
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void pulso::statusSMS(sms *smsPtr)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"%s:%d", estados::nombre(numOut), estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
