#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "nextion.h"
#include "sms.h"

/*
 * class NOT: public bloque {
    protected:
        uint16_t numOut;
        uint16_t numInput;
    public:
        NOT(uint8_t numPar, char *pars[]);  // lee desde string
        const char *diTipo(void);
        const char *diNombre(void);
        int8_t init(void);
        void calcula(void);
        void print(void);
        void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};
 */

extern estados est;

/*
 * NOT  output input
 */
NOT::NOT(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar<3)
    {
        nextion::enviaLog(tty,"#parametros incorrecto");
        *hayError = 1;
        return;
    }
    numOut = estados::addEstado(tty, pars[1], 1, hayError);
    if (numOut==0)
        return;
    numInput = estados::addEstado(tty, pars[2], 0, hayError);
};

NOT::~NOT()
{
}

const char *NOT::diTipo(void)
{
	return "NOT";
}

const char *NOT::diNombre(void)
{
	return estados::nombre(numOut);
}

int8_t NOT::init(void)
{
    estados::ponEstado(numOut, 0);
    return 0;
}

uint8_t NOT::calcula(uint8_t , uint8_t , uint8_t , uint8_t )
{
    estados::ponEstado(numOut, !estados::diEstado(numInput));
    return 0;
}

void NOT::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{
    estados::actualizaNextion(numOut);
}

void NOT::print(BaseSequentialStream *tty)
{
    char buffer[60];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = NOT [%s-%d]",estados::nombre(numOut),numOut,estados::nombre(numInput),numInput);
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}

void NOT::statusSMS(sms *smsPtr)
{
    char buffer[30];
    chsnprintf(buffer,sizeof(buffer),"%s:%d",estados::nombre(numOut),estados::diEstado(numOut));
    smsPtr->addMsgRespuesta(buffer);
}
