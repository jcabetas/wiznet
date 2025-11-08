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



inputRegister::inputRegister(const char *nombrePar)
{
    strncpy(nombre, nombrePar, sizeof(nombre));
    valInterno = 0;
}


uint16_t inputRegister::getValor(void)
{
    return valInterno;
}

void inputRegister::setValor(uint16_t valor)
{
    valInterno = valor;
}

char *inputRegister::getNombre(void)
{
    return nombre;
}

