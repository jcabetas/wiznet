/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "registros.h"
#include "string.h"


void error(void)
{
    while (true) {
        chThdSleepMilliseconds(50);
    }
}

holdingRegister::holdingRegister(const char *nombrePar,const char *opcionesStr[3],
                                 const uint32_t int2ext[], uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault)
{
    strncpy(nombre, nombrePar, sizeof(nombre));
    valDefecto = opcDefault;
    valInterno = valDefecto;
    valMin = opcMin;
    valMax = opcMax;
    valInt2Ext = int2ext;
    for (uint8_t i=0;i<3;i++)
        descValInt[i] = opcionesStr[i];
}

uint8_t holdingRegister::setValorInterno(uint16_t valor)
{
    if (valor<valMin || valor>valMax)
        return 1;
    valInterno = valor;
    // falta almacenar resultado
    return 0;
}

uint16_t holdingRegister::getValor(void)
{
    if (valInt2Ext==NULL)
        return valInterno;
    if (valInterno > valMax)
        error();
    return valInt2Ext[valInterno];
}

uint32_t holdingRegister::getValorInterno(void)
{
    return valInterno;
}



