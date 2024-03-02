/*
 * Flip-Flop.cpp
 *
 *  Created on: 5 abr. 2020
 *      Author: joaquin
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "sms.h"
#include "nextion.h"

extern estados est;

/*
 * class flipflop: public bloque {
    protected:
        int16_t numOut;
        int16_t numInputSet;
        int16_t numInputReset;
    public:
        flipflop(uint8_t numPar, char *pars[]);  // lee desde string
        const char *diTipo(void);
        const char *diNombre(void);
        int8_t init(void);
        void calcula(void);
        void   print(void);
        void addTime(uint16_t ms, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};
 */


/*
 * FLIPFLOP llenarPozo  nivelBajo(S)  nivelAlto(R)
 */
flipflop::flipflop(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=4)
    {
        nextion::enviaLog(tty,"#parametros FLIPFLOP");
        *hayError = 1;
        return;
    }
    numOut = estados::addEstado(tty, pars[1],1, hayError);
    if (numOut==0)
        return;
    numInputSet = estados::addEstado(tty, pars[2],0, hayError);
    numInputReset = estados::addEstado(tty, pars[3],0, hayError);
};

flipflop::~flipflop()
{
}

const char *flipflop::diTipo(void)
{
    return "FLIPFLOP";
}

const char *flipflop::diNombre(void)
{
    return estados::nombre(numOut);
}

int8_t flipflop::init(void)
{
    estados::ponEstado(numOut, 0);
    return 0;
}

uint8_t flipflop::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    if (estados::diEstado(numInputSet))
    {
        estados::ponEstado(numOut, 1);
        return 0;
    }
    if (estados::diEstado(numInputReset))
    {
        estados::ponEstado(numOut, 0);
        return 0;
    }
    return 0;
}

void flipflop::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
}

void flipflop::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = FLIPFLOP (S:[%s-%d] R:[%s-%d]",estados::nombre(numOut),numOut,estados::nombre(numInputSet),numInputSet,estados::nombre(numInputReset),numInputReset);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void flipflop::statusSMS(sms *smsPtr)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"%s:%d", estados::nombre(numOut), estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
