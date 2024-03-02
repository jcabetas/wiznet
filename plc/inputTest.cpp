/*
 * inputTest.cpp
 *
 *  Created on: 4 abr. 2020
 *      Author: joaquin
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include <stdlib.h>
#include "chprintf.h"
#include "bloques.h"

/*
class inputTest : public bloque
{
protected:
    uint16_t numOut;
    uint16_t cuentaDs;
    uint8_t segIni, segFin;

public:
    inputTest(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~inputTest();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};
 */

/*
 *      inputTest niv1 20 40
 */


inputTest::inputTest(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar != 4)
    {
        chprintf(tty,"#parametros incorrecto\n\r");
        *hayError = 1;
        return; // error
    }
    numOut = estados::addEstado(tty, pars[1], 1, hayError);
    if (numOut==0)
        return;
    segIni = atoi(pars[2]);
    segFin = atoi(pars[3]);
};

inputTest::~inputTest()
{
}
const char *inputTest::diTipo(void)
{
    return "inputTest";
}

const char *inputTest::diNombre(void)
{
    return nombres::nomConId(numOut);
}

int8_t inputTest::init(void)
{
    estados::ponEstado(numOut, 0);
    cuentaDs = 0;
    return 0;
}


void inputTest::addTime(uint16_t , uint8_t , uint8_t , uint8_t seg, uint8_t )
{
    if (seg>=segIni && seg<=segFin)
        estados::ponEstado(numOut, 1);
    else
        estados::ponEstado(numOut, 0);
}

void inputTest::print(BaseSequentialStream *tty)
{
    chprintf(tty,"[%s-%d] = inputTest (%d:%d s)\n\r",estados::nombre(numOut),numOut, segIni, segFin);
}
